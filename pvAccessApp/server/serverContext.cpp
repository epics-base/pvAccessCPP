/*
 * serverContext.cpp
 */

#include "serverContext.h"
#include "responseHandlers.h"

namespace epics { namespace pvAccess {

const char* ServerContextImpl::StateNames[] = { "NOT_INITIALIZED", "INITIALIZED", "RUNNING", "SHUTDOWN", "DESTROYED"};
const int32 ServerContextImpl::VERSION_MAJOR = 2;
const int32 ServerContextImpl::VERSION_MINOR = 0;
const int32 ServerContextImpl::VERSION_MAINTENANCE = 0;
const int32 ServerContextImpl::VERSION_DEVELOPMENT = 0;
const Version ServerContextImpl::VERSION("Channel Access Server in C++", "C++",
		ServerContextImpl::VERSION_MAJOR,
		ServerContextImpl::VERSION_MINOR,
		ServerContextImpl::VERSION_MAINTENANCE,
		ServerContextImpl::VERSION_DEVELOPMENT);


ServerContextImpl::ServerContextImpl():
				_state(NOT_INITIALIZED),
				_beaconAddressList(""),
				_ignoreAddressList(""),
				_autoBeaconAddressList(true),
				_beaconPeriod(15.0),
				_broadcastPort(CA_BROADCAST_PORT),
				_serverPort(CA_SERVER_PORT),
				_receiveBufferSize(MAX_TCP_RECV),
				_timer(NULL),
				_broadcastTransport(NULL),
				_broadcastConnector(NULL),
				_beaconEmitter(NULL),
				_acceptor(NULL),
				_transportRegistry(NULL),
				_channelAccess(NULL),
				_channelProviderName(CAJ_DEFAULT_PROVIDER),
				_channelProvider(NULL),
				_beaconServerStatusProvider(NULL)

{
	initializeLogger();
	loadConfiguration();
}

ServerContextImpl::~ServerContextImpl()
{
	if(_beaconEmitter) delete _beaconEmitter;
	if(_broadcastTransport) delete _broadcastTransport;
	if(_acceptor) delete _acceptor;
	if(_transportRegistry) delete _transportRegistry;
	if(_timer) delete _timer;
}

const Version& ServerContextImpl::getVersion()
{
    return ServerContextImpl::VERSION;
}

void ServerContextImpl::initializeLogger()
{
	createFileLogger("serverContextImpl.log");
}

Configuration* ServerContextImpl::getConfiguration()
{
	ConfigurationProvider* configurationProvider =  ConfigurationFactory::getProvider();
	Configuration* config = configurationProvider->getConfiguration("pvAccess-server");
	if (config == NULL)
	{
		config = configurationProvider->getConfiguration("system");
	}
	return config;
}

/**
 * Load configuration.
 */
void ServerContextImpl::loadConfiguration()
{
	Configuration* config = getConfiguration();

	_beaconAddressList = config->getPropertyAsString("EPICS4_CA_ADDR_LIST", _beaconAddressList);
	_beaconAddressList = config->getPropertyAsString("EPICS4_CAS_BEACON_ADDR_LIST", _beaconAddressList);

	_autoBeaconAddressList = config->getPropertyAsBoolean("EPICS4_CA_AUTO_ADDR_LIST", _autoBeaconAddressList);
	_autoBeaconAddressList = config->getPropertyAsBoolean("EPICS4_CAS_AUTO_BEACON_ADDR_LIST", _autoBeaconAddressList);

	_beaconPeriod = config->getPropertyAsFloat("EPICS4_CA_BEACON_PERIOD", _beaconPeriod);
	_beaconPeriod = config->getPropertyAsFloat("EPICS4_CAS_BEACON_PERIOD", _beaconPeriod);

	_serverPort = config->getPropertyAsInteger("EPICS4_CA_SERVER_PORT", _serverPort);
	_serverPort = config->getPropertyAsInteger("EPICS4_CAS_SERVER_PORT", _serverPort);

	_broadcastPort = config->getPropertyAsInteger("EPICS4_CA_BROADCAST_PORT", _broadcastPort);
	_broadcastPort = config->getPropertyAsInteger("EPICS4_CAS_BROADCAST_PORT", _broadcastPort);

	_receiveBufferSize = config->getPropertyAsInteger("EPICS4_CA_MAX_ARRAY_BYTES", _receiveBufferSize);
	_receiveBufferSize = config->getPropertyAsInteger("EPICS4_CAS_MAX_ARRAY_BYTES", _receiveBufferSize);

	_channelProviderName = config->getPropertyAsString("EPICS4_CA_PROVIDER_NAME", _channelProviderName);
	_channelProviderName = config->getPropertyAsString("EPICS4_CAS_PROVIDER_NAME", _channelProviderName);
}

void ServerContextImpl::initialize(ChannelAccess* channelAccess)
{
	//TODO uncomment
	/*Lock guard(_mutex);
	if (channelAccess == NULL)
	{
		THROW_BASE_EXCEPTION("non null channelAccess expected");
	}

	if (_state == DESTROYED)
	{
		THROW_BASE_EXCEPTION("Context destroyed.");
	}
	else if (_state != NOT_INITIALIZED)
	{
		THROW_BASE_EXCEPTION("Context already initialized.");
	}

	_channelAccess = channelAccess;

	_channelProvider = _channelAccess->getProvider(_channelProviderName);
	if (_channelProvider == NULL)
	{
		std::string msg = "Channel provider with name '" + _channelProviderName + "' not available.";
		THROW_BASE_EXCEPTION(msg.c_str());
	}*/

	internalInitialize();

	_state = INITIALIZED;
}

void ServerContextImpl::internalInitialize()
{
	_timer = new Timer("pvAccess-server timer",lowerPriority);
	_transportRegistry = new TransportRegistry();

	// setup broadcast UDP transport
	initializeBroadcastTransport();

	_acceptor = new BlockingTCPAcceptor(this, _serverPort, _receiveBufferSize);
	_serverPort = _acceptor->getBindAddress()->ia.sin_port;

	_beaconEmitter = new BeaconEmitter(_broadcastTransport, this);
}

void ServerContextImpl::initializeBroadcastTransport()
{

	// setup UDP transport
	try
	{
		// where to bind (listen) address
	    osiSockAddr listenLocalAddress;
	    listenLocalAddress.ia.sin_family = AF_INET;
	    listenLocalAddress.ia.sin_port = htons(_broadcastPort);
	    listenLocalAddress.ia.sin_addr.s_addr = htonl(INADDR_ANY);

		// where to send address
	    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	    if (socket == INVALID_SOCKET)
	    {
	    	THROW_BASE_EXCEPTION("Failed to initialize broadcast UDP transport");
	    }
	    auto_ptr<InetAddrVector> broadcastAddresses(getBroadcastAddresses(socket,_broadcastPort));
	    epicsSocketDestroy(socket);

	    auto_ptr<BlockingUDPConnector> broadcastConnector(new BlockingUDPConnector(true, true));
		_broadcastTransport = static_cast<BlockingUDPTransport*>(broadcastConnector->connect(
									NULL, new ServerResponseHandler(this),
									listenLocalAddress, CA_MINOR_PROTOCOL_REVISION,
									CA_DEFAULT_PRIORITY));
		_broadcastTransport->setBroadcastAddresses(broadcastAddresses.get());

		// set ignore address list
		if (!_ignoreAddressList.empty())
		{
			// we do not care about the port
			auto_ptr<InetAddrVector> list(getSocketAddressList(_ignoreAddressList, 0, NULL));
			if (list.get() != NULL && list->size() > 0)
			{
				_broadcastTransport->setIgnoredAddresses(list.get());
			}
		}
		// set broadcast address list
		if (!_beaconAddressList.empty())
		{
			// if auto is true, add it to specified list
			InetAddrVector* appendList = NULL;
			if (_autoBeaconAddressList == true)
			{
				appendList = _broadcastTransport->getSendAddresses();
			}

			auto_ptr<InetAddrVector> list(getSocketAddressList(_beaconAddressList, _broadcastPort, appendList));
			if (list.get() != NULL  && list->size() > 0)
			{
				_broadcastTransport->setBroadcastAddresses(list.get());
			}
		}

		_broadcastTransport->start();
	}
	catch (std::exception& e)
	{
		THROW_BASE_EXCEPTION_CAUSE("Failed to initialize broadcast UDP transport", e);
	}
	catch (...)
	{
		THROW_BASE_EXCEPTION("Failed to initialize broadcast UDP transport");
	}
}

void ServerContextImpl::run(int32 seconds)
{
	if (seconds < 0)
	{
		THROW_BASE_EXCEPTION("seconds cannot be negative.");
	}

	{
		Lock guard(_mutex);

		if (_state == NOT_INITIALIZED)
		{
			THROW_BASE_EXCEPTION("Context not initialized.");
		}
		else if (_state == DESTROYED)
		{
			THROW_BASE_EXCEPTION("Context destroyed.");
		}
		else if (_state == RUNNING)
		{
			THROW_BASE_EXCEPTION("Context is already running.");
		}
		else if (_state == SHUTDOWN)
		{
			THROW_BASE_EXCEPTION("Context was shutdown.");
		}

		_state = RUNNING;
	}

	// run...
	_beaconEmitter->start();

	//TODO review this
	if(seconds == 0)
	{
		_runEvent.wait();
	}
	else
	{
		_runEvent.wait(seconds);
	}

	{
		Lock guard(_mutex);
		_state = SHUTDOWN;
	}
}

void ServerContextImpl::shutdown()
{
	Lock guard(_mutex);
	if(_state == DESTROYED)
	{
		THROW_BASE_EXCEPTION("Context already destroyed.");
	}

	// notify to stop running...
	_runEvent.signal();
}

void ServerContextImpl::destroy()
{
	Lock guard(_mutex);
	if (_state == DESTROYED)
	{
		THROW_BASE_EXCEPTION("Context already destroyed.");
	}

	// shutdown if not already
	shutdown();

	// go into destroyed state ASAP
	_state = DESTROYED;

	internalDestroy();
}


void ServerContextImpl::internalDestroy()
{
	// stop responding to search requests
	if (_broadcastTransport != NULL)
	{
		_broadcastTransport->close(true);
	}

	// stop accepting connections
	if (_acceptor != NULL)
	{
		_acceptor->destroy();
	}

	// stop emitting beacons
	if (_beaconEmitter != NULL)
	{
		_beaconEmitter->destroy();
	}

	// this will also destroy all channels
	destroyAllTransports();
}

void ServerContextImpl::destroyAllTransports()
{

	// not initialized yet
	if (_transportRegistry == NULL)
	{
		return;
	}

	int32 size;
	auto_ptr<Transport*> transports(_transportRegistry->toArray(size));

	if (size == 0)
	{
		return;
	}

	errlogSevPrintf(errlogInfo, "Server context still has %d transport(s) active and closing...", size);

	for (int i = 0; i < size; i++)
	{
		Transport* transport = transports.get()[i];
		try
		{
			transport->close(true);
		}
		catch (std::exception &e)
		{
			// do all exception safe, log in case of an error
			errlogSevPrintf(errlogMajor, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what());
		}
		catch (...)
		{
			// do all exception safe, log in case of an error
			 errlogSevPrintf(errlogMajor, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__);
		}
	}
}

void ServerContextImpl::printInfo()
{
	printInfo(cout);
}

void ServerContextImpl::printInfo(ostream& str)
{
	Lock guard(_mutex);
	str << "VERSION : " << getVersion().getVersionString() << endl \
		<< "CHANNEL PROVIDER : " << _channelProviderName << endl \
		<< "BEACON_ADDR_LIST : " << _beaconAddressList << endl \
	    << "AUTO_BEACON_ADDR_LIST : " << _autoBeaconAddressList << endl \
	    << "BEACON_PERIOD : " << _beaconPeriod << endl \
	    << "BROADCAST_PORT : " << _broadcastPort << endl \
	    << "SERVER_PORT : " << _serverPort << endl \
	    << "RCV_BUFFER_SIZE : " << _receiveBufferSize << endl \
	    << "IGNORE_ADDR_LIST: " << _ignoreAddressList << endl \
	    << "STATE : " << ServerContextImpl::StateNames[_state] << endl;
}

void ServerContextImpl::dispose()
{
	try
	{
		destroy();
	}
	catch(...)
	{
		// noop
	}
}

void ServerContextImpl::setBeaconServerStatusProvider(BeaconServerStatusProvider* beaconServerStatusProvider)
{
	_beaconServerStatusProvider = beaconServerStatusProvider;
}

bool ServerContextImpl::isInitialized()
{
	Lock guard(_mutex);
	return _state == INITIALIZED || _state == RUNNING || _state == SHUTDOWN;
}

bool ServerContextImpl::isDestroyed()
{
	Lock guard(_mutex);
	return _state == DESTROYED;
}

std::string ServerContextImpl::getBeaconAddressList()
{
	return _beaconAddressList;
}

bool ServerContextImpl::isAutoBeaconAddressList()
{
	return _autoBeaconAddressList;
}

float ServerContextImpl::getBeaconPeriod()
{
	return _beaconPeriod;
}

int32 ServerContextImpl::getReceiveBufferSize()
{
	return _receiveBufferSize;
}

int32 ServerContextImpl::getServerPort()
{
	return _serverPort;
}

void ServerContextImpl::setServerPort(int32 port)
{
	_serverPort = port;
}

int32 ServerContextImpl::getBroadcastPort()
{
	return _broadcastPort;
}

std::string ServerContextImpl::getIgnoreAddressList()
{
	return _ignoreAddressList;
}

BeaconServerStatusProvider* ServerContextImpl::getBeaconServerStatusProvider()
{
	return _beaconServerStatusProvider;
}

osiSockAddr* ServerContextImpl::getServerInetAddress()
{
	if(_acceptor != NULL)
	{
		return _acceptor->getBindAddress();
	}
	return NULL;
}

BlockingUDPTransport* ServerContextImpl::getBroadcastTransport()
{
	return _broadcastTransport;
}

ChannelAccess* ServerContextImpl::getChannelAccess()
{
	return _channelAccess;
}

std::string ServerContextImpl::getChannelProviderName()
{
	return _channelProviderName;
}

ChannelProvider* ServerContextImpl::getChannelProvider()
{
	return _channelProvider;
}

Timer* ServerContextImpl::getTimer()
{
	return _timer;
}

TransportRegistry* ServerContextImpl::getTransportRegistry()
{
	return _transportRegistry;
}

Channel* ServerContextImpl::getChannel(pvAccessID id)
{
	//TODO
	return NULL;
}

Transport* ServerContextImpl::getSearchTransport()
{
	//TODO
	return NULL;
}
// TODO
void ServerContextImpl::acquire() {}
void ServerContextImpl::release() {}

}
}
