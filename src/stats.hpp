#ifndef MOROS_STATS_HPP_
#define MOROS_STATS_HPP_

#include <atomic>
#include <vector>
#include <cstdint>


namespace moros {

class Metrics final {
public:
    Metrics(const Metrics&) = delete;
    Metrics(Metrics&&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    Metrics& operator=(Metrics&&) = delete;

    Metrics() noexcept = default;

    enum class Kind {
        EREAD,
        EWRITE,
        ESTATUS,
        ECONNECT,
        ETIMEOUT,
        COMPLETES,
        BYTES,
        MAX,
    };

    static Metrics& getInstance() noexcept {
        static Metrics ins;
        return ins;
    }

    void count(Kind k, std::size_t c = 1) noexcept {
        cnt_[static_cast<std::size_t>(k)] += c;
    }

    std::uint64_t get(Kind k) const noexcept {
        return cnt_[static_cast<std::size_t>(k)];
    }

    std::uint64_t operator[](Kind k) const noexcept {
        return get(k);
    }

private:
    // avoid false sharing
    alignas(64) std::atomic_uint64_t cnt_[static_cast<std::size_t>(Kind::MAX)] = {};
};


class Stats {
public:
    Stats(std::size_t sz) noexcept;

    bool record(std::uint64_t n) noexcept;

    double max() const noexcept;

    double mean() const noexcept;

    double stdev(double m) const noexcept;

    double inStdevPercent(double m, double stdev, std::size_t n) const noexcept;

    std::uint64_t derank(double p) const noexcept;

private:
    std::size_t count_;
    std::uint64_t min_, max_;
    std::vector<std::uint64_t> xs_;
};

}

#endif
