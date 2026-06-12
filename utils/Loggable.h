#ifndef UTILS_LOGGABLE_H
#define UTILS_LOGGABLE_H

#include "utils/Logger.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <typeinfo>
#include <utility>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace utils {

/**
 * Base class for components that need terminal logging.
 */
class Loggable {
   public:
    virtual ~Loggable() = default;

   protected:
    template <typename... Args>
    void log(LogLevel level, Args&&... args) const {
        std::ostringstream oss;
        oss << demangled_class_name() << ": ";
        (oss << ... << std::forward<Args>(args));
        log_line(level, oss.str());
    }

    std::string demangled_class_name() const {
#if defined(__GNUG__)
        int status = 0;
        char* dem = abi::__cxa_demangle(typeid(*this).name(), nullptr, nullptr, &status);
        std::string s = (status == 0 && dem) ? dem : typeid(*this).name();
        std::free(dem);
        return s;
#else
        return typeid(*this).name();
#endif
    }
};

}  // namespace utils

#endif  // UTILS_LOGGABLE_H
