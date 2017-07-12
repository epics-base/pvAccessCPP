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

#include <pv/pvType.h>
#include <pv/sharedPtr.h>

#include <shareLib.h>

namespace epics { namespace pvData { 

class Requester;
typedef std::tr1::shared_ptr<Requester> RequesterPtr;

enum MessageType {
   infoMessage,warningMessage,errorMessage,fatalErrorMessage
};
#define MESSAGE_TYPE_COUNT 4

epicsShareExtern std::string getMessageTypeName(MessageType messageType);

/**
 *  @brief Callback class for passing messages to a requester.
 *
 *  This is used by many other classes and also extended by other classes.
 *  The request is passed a message and a messageType.
 *  A message is just a string and a messageType is:
@code
enum MessageType {
   infoMessage,warningMessage,errorMessage,fatalErrorMessage
};
@endcode
 *
 */

class epicsShareClass Requester {
public:
    POINTER_DEFINITIONS(Requester);
    /**
     * Destructor
     */
    virtual ~Requester(){}
    /**
     * The requester must have a name.
     * @return The requester's name.
     */
    virtual std::string getRequesterName() = 0;
    /**
     * 
     * A message for the requester.
     * @param message The message.
     * @param messageType The type of message:
     @code
     enum MessageType {
        infoMessage,warningMessage,errorMessage,fatalErrorMessage
     };
     @endcode
     */
    virtual void message(std::string const & message,MessageType messageType);
};

}}
#endif  /* REQUESTER_H */
