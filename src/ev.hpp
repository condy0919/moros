#ifndef MOROS_EV_HPP_
#define MOROS_EV_HPP_

#include <new>
#include <chrono>
#include <cassert>
#include <ctime>
#include <cstdint>
#include <vector>
#include <functional>
#include <unordered_map>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>


namespace moros {

enum class Mask {
    NONE = 0,
    READABLE = 1,
    WRITABLE = 1 << 1,
};

constexpr Mask operator&(const Mask& lhs, const Mask& rhs) noexcept {
    return Mask(static_cast<std::size_t>(lhs) & static_cast<std::size_t>(rhs));
}

constexpr Mask& operator&=(Mask& lhs, const Mask& rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

constexpr Mask operator|(const Mask& lhs, const Mask& rhs) noexcept {
    return Mask(static_cast<std::size_t>(lhs) | static_cast<std::size_t>(rhs));
}

constexpr Mask& operator|=(Mask& lhs, const Mask& rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool operator!(const Mask& m) noexcept {
    return m == Mask::NONE;
}

constexpr Mask operator~(const Mask& m) noexcept {
    return Mask(~static_cast<std::size_t>(m));
}


class Event {
    friend class EventLoop;
public:
    Event() noexcept = default;

    void onRead() noexcept {
        if (rproc_) {
            rproc_();
        }
    }

    void onWrite() noexcept {
        if (wproc_) {
            wproc_();
        }
    }

private:
    Mask mask_ = Mask::NONE;

    static void noop() {}

    std::function<void(void)> rproc_;
    std::function<void(void)> wproc_;
};


class EventLoop {
public:
    EventLoop(std::size_t sz) {
        epfd_ = ::epoll_create(1024);
        if (epfd_ == -1) {
            throw std::bad_alloc();
        }

        events_.resize(sz);
        evs_.reserve(sz);
    }

    ~EventLoop() noexcept {
        ::close(epfd_);
    }

    std::size_t size() const noexcept {
        return events_.size();
    }

    // 约定:
    // 如果在 evs_ 内存在，则必然已经在 epoll 中注册过
    // 同理，在 close(fd) 时也应保证 evs_ 内不存在 fd 信息
    template <typename Fn>
    bool addEvent(int fd, Mask m, Fn cb) {
        assert(fd > 0);
        auto iter = evs_.find(fd);
        if (iter == evs_.end()) {
            struct epoll_event e = {
                .events = EPOLLET | EPOLLIN | EPOLLOUT,
                .data = {
                    .fd = fd,
                },
            };
            if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &e) == -1) {
                return false;
            }

            bool ins = false;
            std::tie(iter, ins) = evs_.emplace(fd, Event());
            if (!ins) {
                return false;
            }
        }

        auto& ev = iter->second;

        ev.mask_ |= m;
        if (!!(m & Mask::READABLE)) {
            ev.rproc_ = cb;
        }
        if (!!(m & Mask::WRITABLE)) {
            ev.wproc_ = cb;
        }

        return true;
    }

    void delEvent(int fd, Mask m) noexcept {
        auto iter = evs_.find(fd);
        if (iter == evs_.end()) {
            return;
        }
        auto& ev = iter->second;

        if (!!(m & Mask::READABLE)) {
            ev.rproc_ = Event::noop;
        }
        if (!!(m & Mask::WRITABLE)) {
            ev.wproc_ = Event::noop;
        }

        m = ev.mask_ & ~m;
        if (!m) {
            evs_.erase(fd);
        }
    }

    template <typename Rep, typename Period, typename Fn>
    int addTimerEvent(std::chrono::duration<Rep, Period> interval, Fn cb) {
        struct timespec now;
        ::clock_gettime(CLOCK_MONOTONIC, &now);

        const time_t tv_sec =
            std::chrono::duration_cast<std::chrono::seconds>(interval).count();
        const long tv_nsec =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                interval - std::chrono::seconds(tv_sec))
                .count();

        const struct itimerspec ts = {
            .it_interval = {
                .tv_sec = tv_sec,
                .tv_nsec = tv_nsec,
            },
            .it_value = now,
        };
        
        int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (fd == -1) {
            return -1;
        }

        if (::timerfd_settime(fd, TFD_TIMER_ABSTIME, &ts, nullptr) == -1) {
            ::close(fd);
            return -1;
        }

        return addEvent(fd, Mask::READABLE, [&, fd, cb=std::move(cb)] {
                    std::uint64_t e;
                    ::read(fd, &e, sizeof(e));

                    cb();
                }) ? fd : -1;
    }

    template <typename Rep, typename Period>
    void poll(std::chrono::duration<Rep, Period> t) noexcept {
        const int ret = ::epoll_wait(
            epfd_, &events_[0], events_.size(),
            std::chrono::duration_cast<std::chrono::milliseconds>(t).count());
        if (ret <= 0) {
            return;
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(ret); ++i) {
            auto& ev = events_[i];

            const auto iter = evs_.find(ev.data.fd);
            if (iter == evs_.end()) {
                continue;
            }

            // EPOLLERR, EPOLLHUP 表现为连接被关闭，此时靠 read callback 来处理
            if ((ev.events & EPOLLIN) || (ev.events & EPOLLERR) || (ev.events & EPOLLHUP)) {
                iter->second.onRead();
            }

            // fd not closed by read
            if (ev.events & EPOLLOUT) {
                const auto ii = evs_.find(ev.data.fd);
                if (ii != evs_.end()) {
                    ii->second.onWrite();
                }
            }
        }
    }

    void run() noexcept {
        while (!__atomic_load_n(&stop_, __ATOMIC_RELAXED)) {
            poll(std::chrono::milliseconds(1));
        }
    }

    void stop() noexcept {
        __atomic_store_n(&stop_, true, __ATOMIC_RELAXED);
    }

private:
    int epfd_;
    std::vector<struct epoll_event> events_;

    std::unordered_map<int, Event> evs_;

    bool stop_ = false;
};

}

#endif
