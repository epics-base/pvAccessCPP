/*
 * baseChannelRequester.h
 */

#ifndef BASECHANNELREQUESTER_H_
#define BASECHANNELREQUESTER_H_

#include "serverContext.h"
#include "serverChannelImpl.h"

#include <requester.h>
#include <destroyable.h>

namespace epics {
namespace pvAccess {

class BaseChannelRequester :  public epics::pvData::Requester, public epics::pvData::Destroyable
{
public:
	BaseChannelRequester(ServerContextImpl* context, ServerChannelImpl* channel,const pvAccessID ioid, Transport* transport);
	~BaseChannelRequester() {};

	boolean startRequest(int32 qos);
	void stopRequest();
	int32 getPendingRequest();
	String getRequesterName();
	void message(const String& message, const epics::pvData::MessageType messageType);
	static void message(Transport* transport, const pvAccessID ioid, const String& message, const epics::pvData::MessageType messageType);
	static void sendFailureMessage(const int8 command, Transport* transport, const pvAccessID ioid, const int8 qos, const Status status);

	static const Status okStatus;
	static const Status badCIDStatus;
	static const Status badIOIDStatus;
	static const Status noReadACLStatus;
	static const Status noWriteACLStatus;
	static const Status noProcessACLStatus;
	static const Status otherRequestPendingStatus;
protected:
	const pvAccessID _ioid;
	Transport* _transport;
	ServerChannelImpl* _channel;
private:
	ServerContextImpl* _context;
	static const int32 NULL_REQUEST;
	int32 _pendingRequest;
	epics::pvData::Mutex _mutex;
};

class BaseChannelRequesterMessageTransportSender : public TransportSender
{
public:
	BaseChannelRequesterMessageTransportSender(const pvAccessID _ioid, const String message,const epics::pvData::MessageType messageType);
	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
	void lock();
	void unlock();
	void release();
	void acquire();
private:
	const pvAccessID _ioid;
	const String _message;
	const epics::pvData::MessageType _messageType;
};

class BaseChannelRequesterFailureMessageTransportSender : public TransportSender
{
public:
	BaseChannelRequesterFailureMessageTransportSender(const int8 command, Transport* transport, const pvAccessID ioid, const int8 qos, const Status status);
	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
	void lock();
	void unlock();
	void release();
	void acquire();

private:
	const int8 _command;
	const pvAccessID _ioid;
	const int8 _qos;
	const Status _status;
	Transport* _transport;
};

}
}


#endif /* BASECHANNELREQUESTER_H_ */
