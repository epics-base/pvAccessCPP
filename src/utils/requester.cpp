/* requester.cpp */
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/**
 *  @author mrk
 */
#include <string>
#include <cstdio>
#include <iostream>

#include <epicsMutex.h>

#include <pv/lock.h>

#define epicsExportSharedSymbols
#include <pv/requester.h>

using std::string;

namespace epics { namespace pvAccess {


string getMessageTypeName(MessageType messageType)
{
    switch(messageType) {
    case infoMessage: return "info";
    case warningMessage: return "warning";
    case errorMessage: return "error";
    case fatalErrorMessage: return "fatalError";
    default: return "unknown";
    }
}

void Requester::message(std::string const & message,MessageType messageType)
{
    std::cerr << "[" << getRequesterName() << "] " << getMessageTypeName(messageType) << " : " << message << "\n";
}

}}
