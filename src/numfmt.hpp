#ifndef MOROS_NUMFMT_HPP_
#define MOROS_NUMFMT_HPP_

#include <cstdint>
#include <chrono>
#include <string>
#include <boost/format.hpp>

namespace moros {

std::string numfmt(std::chrono::hours h) {
    return str(boost::format("%1%h") % h.count());
}

std::string numfmt(std::chrono::minutes m) {
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(m);
    m -= hours;

    if (hours.count()) {
        return m.count()
                   ? str(boost::format("%1% %2%m") % numfmt(hours) % m.count())
                   : numfmt(hours);
    }
    return str(boost::format("%1%min") % m.count());
}

std::string numfmt(std::chrono::seconds s) {
    const auto mins = std::chrono::duration_cast<std::chrono::minutes>(s);
    s -= mins;

    if (mins.count()) {
        return s.count()
                   ? str(boost::format("%1% %2%s") % numfmt(mins) % s.count())
                   : numfmt(mins);
    }
    return str(boost::format("%1%s") % s.count());
}

std::string numfmt(std::chrono::milliseconds ms) {
    const auto sec = std::chrono::duration_cast<std::chrono::seconds>(ms);
    ms -= sec;

    if (sec.count()) {
        return ms.count()
                   ? str(boost::format("%1% %2%ms") % numfmt(sec) % ms.count())
                   : numfmt(sec);
    }
    return str(boost::format("%1%ms") % ms.count());
}

std::string numfmt(std::chrono::microseconds us) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(us);
    us -= ms;

    if (ms.count()) {
        return us.count()
                   ? str(boost::format("%1% %2%us") % numfmt(ms) % us.count())
                   : numfmt(ms);
    }
    return str(boost::format("%1%us") % us.count());
}

std::string numfmt(double n) {
    static const char* units[] = {
        "", "K", "M", "G", "T", "P"
    };

    std::size_t idx = 0;
    while (idx < sizeof(units) / sizeof(units[0]) && n > 1024) {
        n /= 1024;
        ++idx;
    }
    return str(boost::format("%.2lf%s") % n % units[idx]);
}

}


#endif
