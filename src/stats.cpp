#include "stats.hpp"
#include <cmath>
#include <climits>
#include <algorithm>


namespace moros {

Stats::Stats(std::size_t sz) noexcept
    : count_(0), min_(UINT64_MAX), max_(0), xs_(sz, 0) {}

bool Stats::record(std::uint64_t n) noexcept {
    if (n >= xs_.size()) {
        return false;
    }

    __atomic_add_fetch(&count_, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&xs_[n], 1, __ATOMIC_RELAXED);

    std::uint64_t min = __atomic_load_n(&min_, __ATOMIC_RELAXED),
                  max = __atomic_load_n(&max_, __ATOMIC_RELAXED);
    while (n < min &&
           !__atomic_compare_exchange_n(&min_, &min, n, true, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
    }
    while (n > max &&
           !__atomic_compare_exchange_n(&max_, &max, n, true, __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
    }

    return true;
}

double Stats::max() const noexcept {
    return max_;
}

double Stats::mean() const noexcept {
    if (count_ == 0) {
        return 0.0;
    }

    std::uint64_t sum = 0;
    for (std::uint64_t i = min_; i <= max_; ++i) {
        sum += xs_[i] * i;
    }
    return 1.0 * sum / count_;
}

double Stats::stdev(double m) const noexcept {
    if (count_ <= 1) {
        return 0.0;
    }

    double sum = 0;
    for (std::uint64_t i = min_; i <= max_; ++i) {
        if (xs_[i]) {
            sum += std::pow(i - m, 2) * xs_[i];
        }
    }
    return std::sqrt(sum / (count_ - 1));
}

double Stats::inStdevPercent(double m, double sev, std::size_t n) const noexcept {
    const std::uint64_t lo = std::max(0.0 + min_, std::ceil(m - n * sev)),
                        hi = std::min(0.0 + max_, std::ceil(m + n * sev));

    std::uint64_t sum = 0;
    for (std::uint64_t i = lo; i <= hi; ++i) {
        sum += xs_[i];
    }
    return 100.0 * sum / count_;
}

std::uint64_t Stats::derank(double p) const noexcept {
    const std::uint64_t rank = std::ceil(p * count_);

    std::uint64_t total = 0;
    for (std::uint64_t i = min_; i <= max_; ++i) {
        total += xs_[i];
        if (total >= rank) {
            return i;
        }
    }
    return 0;
}

}
