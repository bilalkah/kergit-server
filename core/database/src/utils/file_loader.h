#ifndef FILELOADER_H
#define FILELOADER_H

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace utils {

inline std::string loadFileToString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace utils

#endif  // FILELOADER_H
