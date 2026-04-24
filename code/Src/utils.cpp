#include "utils.h"

bool matchPrefix(const std::string& str, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (str.size() < prefix.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

bool matchPrefixIgnoreCase(const std::string& str, const std::string& prefix) {
    if (prefix.empty()) return true;
    if (str.size() < prefix.size()) return false;

    for (size_t i = 0; i < prefix.size(); ++i) {
        unsigned char c1 = static_cast<unsigned char>(str[i]);
        unsigned char c2 = static_cast<unsigned char>(prefix[i]);

        if (std::tolower(c1) != std::tolower(c2)) {
            return false;
        }
    }
    return true;
}