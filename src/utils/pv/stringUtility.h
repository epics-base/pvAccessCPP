/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef STRING_UTILITY_H
#define STRING_UTILITY_H

#include <vector>
#include <string>

#include <shareLib.h>

namespace epics { namespace pvAccess { namespace StringUtility {

epicsShareFunc std::string leftTrim(const std::string& s);
epicsShareFunc std::string rightTrim(const std::string& s);
epicsShareFunc std::string trim(const std::string& s);
epicsShareFunc std::vector<std::string>& split(const std::string& s, char delimiter, std::vector<std::string>& elements, bool ignoreEmptyTokens = false);
epicsShareFunc std::vector<std::string> split(const std::string& s, char delimiter, bool ignoreEmptyTokens = false);
epicsShareFunc std::string toLowerCase(const std::string& input);
epicsShareFunc std::string toUpperCase(const std::string& input);
epicsShareFunc std::string replace(const std::string& input, char oldChar, char newChar);
epicsShareFunc std::string replace(const std::string& input, char oldChar, const std::string& newString);
epicsShareFunc std::string replace(const std::string& input, const std::string& oldString, const std::string& newString);

}}} // namespace epics::pvAccess::StringUtility

#endif
