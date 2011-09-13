/*
 * baseChannelRequester.h
 */

#ifndef BASECHANNELREQUESTER_H_
#define BASECHANNELREQUESTER_H_

#include <pv/serverContext.h>
#include <pv/serverChannelImpl.h>

#include <pv/requester.h>
#include <pv/destroyable.h>

using epics::pvData::Status;

namespace epics {
namespace pvAccess {

class BaseChannelRequester :  virtual public epics::pvData::Requester, public epics::pvData::Destroyable
{
public:
	BaseChannelRequester(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
	                     const pvAccessID ioid, Transport::shared_pointer const & transport);
	virtual ~BaseChannelRequester() {};

	bool startRequest(int32 qos);
	void stopRequest();
	int32 getPendingRequest();
	String getRequesterName();
	void message(const String message, const epics::pvData::MessageType messageType);
	static void message(Transport::shared_pointer const & transport, const pvAccessID ioid, const String message, const epics::pvData::MessageType messageType);
	static void sendFailureMessage(const int8 command, Transport::shared_pointer const & transport, const pvAccessID ioid, const int8 qos, const Status status);

	static const Status okStatus;
	static const Status badCIDStatus;
	static const Status badIOIDStatus;
	static const Status noReadACLStatus;
	static const Status noWriteACLStatus;
	static const Status noProcessACLStatus;
	static const Status otherRequestPendingStatus;
protected:
	const pvAccessID _ioid;
	Transport::shared_pointer _transport;
	ServerChannelImpl::shared_pointer _channel;
	epics::pvData::Mutex _mutex;
private:
	ServerContextImpl::shared_pointer _context;
	static const int32 NULL_REQUEST;
	int32 _pendingRequest;
};

class BaseChannelRequesterMessageTransportSender : public TransportSender
{
public:
	BaseChannelRequesterMessageTransportSender(const pvAccessID _ioid, const String message,const epics::pvData::MessageType messageType);
	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
	void lock();
	void unlock();
private:
	const pvAccessID _ioid;
	const String _message;
	const epics::pvData::MessageType _messageType;
};

class BaseChannelRequesterFailureMessageTransportSender : public TransportSender
{
public:
	BaseChannelRequesterFailureMessageTransportSender(const int8 command, Transport::shared_pointer const & transport, const pvAccessID ioid, const int8 qos, const Status& status);
	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
	void lock();
	void unlock();

private:
	const int8 _command;
	const pvAccessID _ioid;
	const int8 _qos;
	const Status _status;
	Transport::shared_pointer _transport;
};

}
}


#endif /* BASECHANNELREQUESTER_H_ */
