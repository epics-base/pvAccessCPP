/* requester.h */
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/**
 *  @author mrk
 */
#ifndef REQUESTER_H
#define REQUESTER_H
#include <string>

#ifdef epicsExportSharedSymbols
#   define requesterEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvType.h>
#include <pv/sharedPtr.h>

#ifdef requesterEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef requesterEpicsExportSharedSymbols
#endif


#include <shareLib.h>

namespace epics { namespace pvAccess {

class Requester;
typedef std::tr1::shared_ptr<Requester> RequesterPtr;

enum MessageType {
   infoMessage,warningMessage,errorMessage,fatalErrorMessage
};
#define MESSAGE_TYPE_COUNT 4

epicsShareExtern std::string getMessageTypeName(MessageType messageType);

/** @brief Callback class for passing messages to a requester.
 */
class epicsShareClass Requester {
public:
    POINTER_DEFINITIONS(Requester);
    virtual ~Requester(){}
    /**
     * The requester must have a name.
     * @return The requester's name.
     */
    virtual std::string getRequesterName() = 0;
    /** Push notification
     */
    virtual void message(std::string const & message,MessageType messageType = errorMessage);
};

}}
namespace epics { namespace pvData {
using ::epics::pvAccess::Requester;
using ::epics::pvAccess::RequesterPtr;
using ::epics::pvAccess::MessageType;
using ::epics::pvAccess::getMessageTypeName;
using ::epics::pvAccess::infoMessage;
using ::epics::pvAccess::warningMessage;
using ::epics::pvAccess::errorMessage;
using ::epics::pvAccess::fatalErrorMessage;
}}
#endif  /* REQUESTER_H */
