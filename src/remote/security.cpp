/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#include <osiProcess.h>

#include <epicsThread.h>
#include <epicsGuard.h>
#include <pv/epicsException.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/securityImpl.h>

typedef epicsGuard<epicsMutex> Guard;

namespace {
namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;


pvd::StructureConstPtr userAndHostStructure(
        pvd::FieldBuilder::begin()->
            add("user", pvd::pvString)->
            add("host", pvd::pvString)->
            createStructure()
);

struct SimpleSession : public pva::AuthenticationSession
{
    const pvd::PVStructure::const_shared_pointer initdata;

    SimpleSession(const pvd::PVStructure::const_shared_pointer& data) :initdata(data) {}
    virtual ~SimpleSession() {}

    virtual epics::pvData::PVStructure::const_shared_pointer initializationData() OVERRIDE FINAL
    { return initdata; }
};

struct AnonPlugin : public pva::AuthenticationPlugin
{
    const bool server;

    AnonPlugin(bool server) :server(server) {}
    virtual ~AnonPlugin() {}

    virtual std::tr1::shared_ptr<pva::AuthenticationSession> createSession(
        const std::tr1::shared_ptr<pva::PeerInfo>& peer,
        std::tr1::shared_ptr<pva::AuthenticationPluginControl> const & control,
        epics::pvData::PVStructure::shared_pointer const & data) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<SimpleSession> sess(new SimpleSession(pvd::PVStructure::const_shared_pointer())); // no init data
        if(server) {
            peer->identified = false;
            peer->account = "anonymous";
            control->authenticationCompleted(pvd::Status::Ok, peer);
        }
        return sess;
    }
};

struct CAPlugin : public pva::AuthenticationPlugin
{
    const bool server;
    // fully const after ctor
    const pvd::PVStructurePtr user;

    CAPlugin(bool server)
        :server(server)
        ,user(userAndHostStructure->build())
    {
        std::vector<char> buffer(256u);
        if(osiGetUserName(&buffer[0], buffer.size()) != osiGetUserNameSuccess)
            throw std::runtime_error("Unable to determine user account name");

        buffer[buffer.size()-1] = '\0';
        user->getSubFieldT<pvd::PVString>("user")->put(&buffer[0]);

        // use of unverified host name is considered deprecated.
        // use PeerInfo::peer instead.
        if (gethostname(&buffer[0], buffer.size()) != 0)
            throw std::runtime_error("Unable to determine host name");

        buffer[buffer.size()-1] = '\0';
        user->getSubFieldT<pvd::PVString>("host")->put(&buffer[0]);
    }
    virtual ~CAPlugin() {}

    virtual std::tr1::shared_ptr<pva::AuthenticationSession> createSession(
        const std::tr1::shared_ptr<pva::PeerInfo>& peer,
        std::tr1::shared_ptr<pva::AuthenticationPluginControl> const & control,
        epics::pvData::PVStructure::shared_pointer const & data) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<SimpleSession> sess(new SimpleSession(user)); // no init data
        if(server) {
            peer->identified = true;
            peer->account = data->getSubFieldT<pvd::PVString>("user")->get();
            peer->aux = pvd::getPVDataCreate()->createPVStructure(data); // clone to ensure it won't be modified
            control->authenticationCompleted(pvd::Status::Ok, peer);
        }
        return sess;
    }
};

} // namespace

namespace epics {
namespace pvAccess {

size_t PeerInfo::num_instances;

PeerInfo::PeerInfo()
    :transportVersion(0u)
    ,local(false)
    ,identified(false)
{
    REFTRACE_INCREMENT(num_instances);
}

PeerInfo::~PeerInfo()
{
    REFTRACE_DECREMENT(num_instances);
}

AuthenticationSession::~AuthenticationSession() {}

AuthenticationPluginControl::~AuthenticationPluginControl() {}

AuthenticationPlugin::~AuthenticationPlugin() {}

AuthenticationRegistry::~AuthenticationRegistry() {}

AuthorizationPlugin::~AuthorizationPlugin() {}

AuthorizationRegistry::~AuthorizationRegistry() {}

namespace {
struct authGbl_t {
    mutable epicsMutex mutex;
    AuthenticationRegistry servers, clients;
    AuthorizationRegistry authorizers;
} *authGbl;

void authGblInit(void *)
{
    authGbl = new authGbl_t;

    {
        AnonPlugin::shared_pointer plugin(new AnonPlugin(true));
        authGbl->servers.add(-1024, "anonymous", plugin);
    }
    {
        AnonPlugin::shared_pointer plugin(new AnonPlugin(false));
        authGbl->clients.add(-1024, "anonymous", plugin);
    }

    {
        CAPlugin::shared_pointer plugin(new CAPlugin(true));
        authGbl->servers.add(0, "ca", plugin);
    }
    {
        CAPlugin::shared_pointer plugin(new CAPlugin(false));
        authGbl->clients.add(0, "ca", plugin);
    }

    epics::registerRefCounter("PeerInfo", &PeerInfo::num_instances);
}

epicsThreadOnceId authGblOnce = EPICS_THREAD_ONCE_INIT;
} // namespace

AuthenticationRegistry& AuthenticationRegistry::clients()
{
    epicsThreadOnce(&authGblOnce, &authGblInit, 0);
    assert(authGbl);
    return authGbl->clients;
}

AuthenticationRegistry& AuthenticationRegistry::servers()
{
    epicsThreadOnce(&authGblOnce, &authGblInit, 0);
    assert(authGbl);
    return authGbl->servers;
}

void AuthenticationRegistry::snapshot(list_t &plugmap) const
{
    plugmap.clear();
    Guard G(mutex);
    plugmap.reserve(map.size());
    for(map_t::const_iterator it(map.begin()), end(map.end()); it!=end; ++it) {
        plugmap.push_back(it->second);
    }
}

void AuthenticationRegistry::add(int prio, const std::string& name,
                                 const AuthenticationPlugin::shared_pointer& plugin)
{
    Guard G(mutex);
    if(map.find(prio)!=map.end())
        THROW_EXCEPTION2(std::logic_error, "Authentication plugin already registered with this priority");
    map[prio] = std::make_pair(name, plugin);
}

bool AuthenticationRegistry::remove(const AuthenticationPlugin::shared_pointer& plugin)
{
    Guard G(mutex);
    for(map_t::iterator it(map.begin()), end(map.end()); it!=end; ++it) {
        if(it->second.second==plugin) {
            map.erase(it);
            return true;
        }
    }
    return false;
}

AuthenticationPlugin::shared_pointer AuthenticationRegistry::lookup(const std::string& name) const
{
    Guard G(mutex);
    // assuming the number of plugins is small, we don't index by name.
    for(map_t::const_iterator it(map.begin()), end(map.end()); it!=end; ++it) {
        if(it->second.first==name)
            return it->second.second;
    }
    return AuthenticationPlugin::shared_pointer();
}


AuthorizationRegistry::AuthorizationRegistry()
    :busy(0)
{}

AuthorizationRegistry& AuthorizationRegistry::plugins()
{
    epicsThreadOnce(&authGblOnce, &authGblInit, 0);
    assert(authGbl);
    return authGbl->authorizers;
}

void AuthorizationRegistry::add(int prio, const AuthorizationPlugin::shared_pointer& plugin)
{
    Guard G(mutex);
    // we don't expect changes after server start
    if(busy)
        throw std::runtime_error("AuthorizationRegistry busy");
    if(map.find(prio)!=map.end())
        THROW_EXCEPTION2(std::logic_error, "Authorization plugin already registered with this priority");
    map[prio] = plugin;
}

bool AuthorizationRegistry::remove(const AuthorizationPlugin::shared_pointer& plugin)
{
    Guard G(mutex);
    if(busy)
        throw std::runtime_error("AuthorizationRegistry busy");
    for(map_t::iterator it(map.begin()), end(map.end()); it!=end; ++it) {
        if(it->second==plugin) {
            map.erase(it);
            return true;
        }
    }
    return false;
}

void AuthorizationRegistry::run(const std::tr1::shared_ptr<PeerInfo>& peer)
{
    int marker;
    {
        Guard G(mutex);
        if(busy)
            throw std::runtime_error("AuthorizationRegistry busy");
        busy = &marker;
    }
    for(map_t::iterator it(map.begin()), end(map.end()); it!=end; ++it)
    {
        (it->second)->authorize(peer);
    }
    {
        Guard G(mutex);
        assert(busy==&marker);
        busy = 0;
    }
}

void AuthNZHandler::handleResponse(osiSockAddr* responseFrom,
                                   Transport::shared_pointer const & transport,
                                   epics::pvData::int8 version,
                                   epics::pvData::int8 command,
                                   size_t payloadSize,
                                   epics::pvData::ByteBuffer* payloadBuffer)
{
    ResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

    pvd::PVStructure::shared_pointer data;
    {
        pvd::PVField::shared_pointer raw(SerializationHelper::deserializeFull(payloadBuffer, transport.get()));
        if(raw->getField()->getType()==pvd::structure) {
            data = std::tr1::static_pointer_cast<pvd::PVStructure>(raw);
        } else {
            // was originally possible, but never used
        }
    }

    transport->authNZMessage(data);
}

}} // namespace epics::pvAccess
