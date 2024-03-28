/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cctype>
#include <sstream>
#include <algorithm>

#define epicsExportSharedSymbols
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

std::vector<std::string>& split(const std::string& s, char delimiter, std::vector<std::string>& elements, bool ignoreEmptyTokens)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        item = trim(item);
        if (!item.empty() || !ignoreEmptyTokens) {
            elements.push_back(item);
        }
    }
    return elements;
}

std::vector<std::string> split(const std::string& s, char delimiter, bool ignoreEmptyTokens)
{
    std::vector<std::string> elements;
    split(s, delimiter, elements, ignoreEmptyTokens);
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

std::string replace(const std::string& input, char oldChar, char newChar)
{
    std::string oldString;
    oldString += oldChar;
    std::string newString;
    newString += newChar;
    return replace(input, oldString, newString);
}

std::string replace(const std::string& input, char oldChar, const std::string& newString)
{
    std::string oldString;
    oldString += oldChar;
    return replace(input, oldString, newString);
}

std::string replace(const std::string& input, const std::string& oldString, const std::string& newString)
{
    if (oldString.empty()) {
        return input;
    }
    std::string output = input;
    std::string::size_type pos = input.find(oldString);
    while (pos != std::string::npos) {
        output = output.replace(pos, oldString.size(), newString);
        pos = pos + newString.size();
        pos = output.find(oldString, pos);
    }
    return output;
}

}}}
