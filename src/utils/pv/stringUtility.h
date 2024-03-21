/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef STRING_UTILITY_H
#define STRING_UTILITY_H

#include <vector>
#include <string>

namespace epics { namespace pvAccess { namespace StringUtility {

std::string leftTrim(const std::string& s);
std::string rightTrim(const std::string& s);
std::string trim(const std::string& s);
std::vector<std::string>& split(const std::string& s, char delimiter, std::vector<std::string>& elements);
std::vector<std::string> split(const std::string& s, char delimiter);
std::string toLowerCase(const std::string& input);
std::string toUpperCase(const std::string& input);

}}} // namespace epics::pvAccess::StringUtility

#endif
