#pragma once
#include <string>

// 匹配字符串str是否以prefix为前缀
bool matchPrefix(const std::string& str, const std::string& prefix);

// 匹配字符串str是否以prefix为前缀（忽略大小写）
bool matchPrefixIgnoreCase(const std::string& str, const std::string& prefix);