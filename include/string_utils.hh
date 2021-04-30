#ifndef STRING_UTILS_HH
#define STRING_UTILS_HH

#include <string>
#include <vector>

namespace utils {
std::vector<std::string> split(const std::string &str, char delim,
                               bool include_empty = false);
std::string join(const std::vector<std::string> &strs, const std::string &sep, bool quote = false);
std::string escape(const std::string &orig, bool html = true);
} // namespace utils

#endif // STRING_UTILS_HH
