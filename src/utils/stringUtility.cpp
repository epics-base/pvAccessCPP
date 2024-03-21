/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cctype>
#include <sstream>
#include <algorithm>
#include "pv/stringUtility.h"

namespace epics { namespace pvAccess { namespace StringUtility {

std::string leftTrim(const std::string& s)
{
    unsigned int i;
    unsigned int n = (unsigned int)s.length();
    for (i = 0; i < n; i++) {
        if (!isspace(s[i])) {
            break;
        }
    }
    return s.substr(i,n-i);
}

std::string rightTrim(const std::string& s)
{
    unsigned int i;
    unsigned int n = (unsigned int)s.length();
    for (i = n; i > 0; i--) {
        if (!isspace(s[i-1])) {
            break;
        }
    }
    return s.substr(0,i);
}

std::string trim(const std::string& s)
{
    return rightTrim(leftTrim(s));
}

std::vector<std::string>& split(const std::string& s, char delimiter, std::vector<std::string>& elements)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        elements.push_back(trim(item));
    }
    return elements;
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> elements;
    split(s, delimiter, elements);
    return elements;
}

std::string toLowerCase(const std::string& input)
{
    std::stringstream ss;
    for (unsigned int i = 0; i < input.size(); i++) {
        char c = std::tolower(input.at(i));
        ss << c;
    }
    return ss.str();
}

std::string toUpperCase(const std::string& input)
{
    std::stringstream ss;
    for (unsigned int i = 0; i < input.size(); i++) {
        char c = std::toupper(input.at(i));
        ss << c;
    }
    return ss.str();
}
}}}
