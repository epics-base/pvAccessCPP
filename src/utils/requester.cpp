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

#define epicsExportSharedSymbols
#include <pv/lock.h>
#include <pv/requester.h>

using std::string;

namespace epics { namespace pvData { 

static StringArray messageTypeName(MESSAGE_TYPE_COUNT);

string getMessageTypeName(MessageType messageType)
{
    // TODO not thread-safe
    static Mutex mutex;
    Lock xx(mutex);
    if(messageTypeName[0].size()==0) {
        messageTypeName[0] = "info";
        messageTypeName[1] = "warning";
        messageTypeName[2] = "error";
        messageTypeName[3] = "fatalError";
    }
    return messageTypeName[messageType];
}

void Requester::message(std::string const & message,MessageType messageType)
{
    std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")\n";
}

}}
