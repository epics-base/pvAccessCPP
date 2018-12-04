/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SECURITY_H
#define SECURITY_H

#ifdef epicsExportSharedSymbols
#   define securityEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <string>
#include <osiSock.h>

#include <pv/status.h>
#include <pv/pvData.h>
#include <pv/sharedPtr.h>

#ifdef securityEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef securityEpicsExportSharedSymbols
#endif

#include <pv/pvaDefs.h>
#include <pv/pvaConstants.h>
#include <pv/serializationHelper.h>
#include <pv/logger.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {

// notify client only on demand, configurable via pvRequest
// add the following method to ChannelRequest:
// void credentialsChanged(std::vector<BitSet> credentials);


// pvAccess message: channel client id, ioid (if invalid then it's for channel) and array of bitSets
// or leave to the plugin?


// when clients gets initial credentialsChanged call before create is called
// and then on each change

class epicsShareClass ChannelSecuritySession {
public:
    POINTER_DEFINITIONS(ChannelSecuritySession);

    virtual ~ChannelSecuritySession() {}

    /// closes this session
    virtual void close() = 0;

    // for every authroizeCreate... a release() must be called
    virtual void release(pvAccessID ioid) = 0;

    // bitSet w/ one bit
    virtual epics::pvData::Status authorizeCreateChannelProcess(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    virtual epics::pvData::Status authorizeProcess(pvAccessID ioid) = 0;

    // bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateChannelGet(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    virtual epics::pvData::Status authorizeGet(pvAccessID ioid) = 0;

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // write: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateChannelPut(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    virtual epics::pvData::Status authorizePut(
        pvAccessID ioid,
        epics::pvData::PVStructure::shared_pointer const & dataToPut,
        epics::pvData::BitSet::shared_pointer const & fieldsToPut) = 0;

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // write: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // process: bitSet w/ one bit (allowed, not allowed)
    virtual epics::pvData::Status authorizeCreateChannelPutGet(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    virtual epics::pvData::Status authorizePutGet(
        pvAccessID ioid,
        epics::pvData::PVStructure::shared_pointer const & dataToPut,
        epics::pvData::BitSet::shared_pointer const & fieldsToPut) = 0;

    // bitSet w/ one bit
    virtual epics::pvData::Status authorizeCreateChannelRPC(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    // one could authorize per operation basis
    virtual epics::pvData::Status authorizeRPC(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & arguments) = 0;

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateMonitor(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    virtual epics::pvData::Status authorizeMonitor(pvAccessID ioid) = 0;

    // read: bitSet w/ one bit (allowed, not allowed) and rest put/get/set length
    virtual epics::pvData::Status authorizeCreateChannelArray(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & pvRequest) = 0;
    // use authorizeGet
    virtual epics::pvData::Status authorizePut(pvAccessID ioid, epics::pvData::PVArray::shared_pointer const & dataToPut) = 0;
    virtual epics::pvData::Status authorizeSetLength(pvAccessID ioid) = 0;


    // introspection authorization
    virtual epics::pvData::Status authorizeGetField(pvAccessID ioid, std::string const & subField) = 0;
};

class SecurityPlugin;

class SecurityException: public std::runtime_error {
public:
    explicit SecurityException(std::string const & what): std::runtime_error(what) {}
};

class epicsShareClass SecuritySession {
public:
    POINTER_DEFINITIONS(SecuritySession);

    virtual ~SecuritySession() {}

    // optional (can be null) initialization data for the remote party
    // client to server
    virtual epics::pvData::PVField::shared_pointer initializationData() = 0;

    // get parent
    virtual std::tr1::shared_ptr<SecurityPlugin> getSecurityPlugin() = 0;

    // can be called any time, for any reason
    virtual void messageReceived(epics::pvData::PVField::shared_pointer const & data) = 0;

    /// closes this session
    virtual void close() = 0;

    // notification to the client on allowed requests (bitSet, a bit per request)
    virtual ChannelSecuritySession::shared_pointer createChannelSession(std::string const & channelName) = 0;
};

class epicsShareClass SecurityPluginControl {
public:
    POINTER_DEFINITIONS(SecurityPluginControl);

    virtual ~SecurityPluginControl() {}

    // can be called any time, for any reason
    virtual void sendSecurityPluginMessage(epics::pvData::PVField::shared_pointer const & data) = 0;

    // if Status.isSuccess() == false,
    // pvAccess will send status to the client and close the connection
    // can be called more then once (in case of re-authentication process)
    virtual void authenticationCompleted(epics::pvData::Status const & status) = 0;
};


class epicsShareClass SecurityPlugin {
public:
    POINTER_DEFINITIONS(SecurityPlugin);

    virtual ~SecurityPlugin() {}

    /**
     * Short, unique name for the plug-in, used to identify the plugin.
     * @return the ID.
     */
    virtual std::string getId() const = 0;

    /**
     * Description of the security plug-in.
     * @return the description string.
     */
    virtual std::string getDescription() const = 0;

    /**
     * Check whether the remote instance with given network address is
     * valid to use this security plug-in to authNZ.
     * @param remoteAddress
     * @return <code>true</code> if this security plugin can be used for remote instance.
     */
    virtual bool isValidFor(osiSockAddr const & remoteAddress) const = 0;

    /**
     * Create a security session (usually one per transport).
     * @param remoteAddress
     * @return a new session.
     * @throws SecurityException
     *
     * @warning a Ref. loop is created if the SecuritySession stores a pointer to 'control'
     */
    // authentication must be done immediately when connection is established (timeout 3seconds),
    // later on authentication process can be repeated
    // the server and the client can exchange (arbitrary number) of messages using SecurityPluginControl.sendMessage()
    // the process completion must be notified by calling AuthenticationControl.completed()
    virtual SecuritySession::shared_pointer createSession(
        osiSockAddr const & remoteAddress,
        SecurityPluginControl::shared_pointer const & control,
        epics::pvData::PVField::shared_pointer const & data) = 0;
};



class epicsShareClass NoSecurityPlugin :
    public SecurityPlugin,
    public SecuritySession,
    public ChannelSecuritySession,
    public std::tr1::enable_shared_from_this<NoSecurityPlugin> {
protected:
    NoSecurityPlugin() {}

public:
    POINTER_DEFINITIONS(NoSecurityPlugin);

    static NoSecurityPlugin::shared_pointer INSTANCE;

    virtual ~NoSecurityPlugin() {}

    // optional (can be null) initialization data for the remote party
    // client to server
    virtual epics::pvData::PVField::shared_pointer initializationData() {
        return epics::pvData::PVField::shared_pointer();
    }

    // get parent
    virtual std::tr1::shared_ptr<SecurityPlugin> getSecurityPlugin() {
        return shared_from_this();
    }

    // can be called any time, for any reason
    virtual void messageReceived(epics::pvData::PVField::shared_pointer const & data) {
        // noop
    }

    /// closes this session
    virtual void close() {
        // noop
    }

    // notification to the client on allowed requests (bitSet, a bit per request)
    virtual ChannelSecuritySession::shared_pointer createChannelSession(std::string const & /*channelName*/)
    {
        return shared_from_this();
    }

    /**
     * Short, unique name for the plug-in, used to identify the plugin.
     * @return the ID.
     */
    virtual std::string getId() const {
        return "none";
    }

    /**
     * Description of the security plug-in.
     * @return the description string.
     */
    virtual std::string getDescription() const {
        return "No security plug-in";
    }

    /**
     * Check whether the remote instance with given network address is
     * valid to use this security plug-in to authNZ.
     * @param remoteAddress
     * @return <code>true</code> if this security plugin can be used for remote instance.
     */
    virtual bool isValidFor(osiSockAddr const & /*remoteAddress*/) const {
        return true;
    }

    /**
     * Create a security session (usually one per transport).
     * @param remoteAddress
     * @return a new session.
     * @throws SecurityException
     */
    // authentication must be done immediately when connection is established (timeout 3seconds),
    // later on authentication process can be repeated
    // the server and the client can exchange (arbitrary number) of messages using SecurityPluginControl.sendMessage()
    // the process completion must be notified by calling AuthenticationControl.completed()
    virtual SecuritySession::shared_pointer createSession(
        osiSockAddr const & /*remoteAddress*/,
        SecurityPluginControl::shared_pointer const & control,
        epics::pvData::PVField::shared_pointer const & /*data*/) {
        control->authenticationCompleted(epics::pvData::Status::Ok);
        return shared_from_this();
    }

    // for every authroizeCreate... a release() must be called
    virtual void release(pvAccessID ioid) {
        // noop
    }

    // bitSet w/ one bit
    virtual epics::pvData::Status authorizeCreateChannelProcess(
        pvAccessID ioid, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizeProcess(pvAccessID /*ioid*/) {
        return epics::pvData::Status::Ok;
    }

    // bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateChannelGet(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizeGet(pvAccessID /*ioid*/) {
        return epics::pvData::Status::Ok;
    }

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // write: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateChannelPut(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizePut(
        pvAccessID /*ioid*/,
        epics::pvData::PVStructure::shared_pointer const & /*dataToPut*/,
        epics::pvData::BitSet::shared_pointer const & /*fieldsToPut*/) {
        return epics::pvData::Status::Ok;
    }

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // write: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    // process: bitSet w/ one bit (allowed, not allowed)
    virtual epics::pvData::Status authorizeCreateChannelPutGet(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizePutGet(
        pvAccessID /*ioid*/,
        epics::pvData::PVStructure::shared_pointer const & /*dataToPut*/,
        epics::pvData::BitSet::shared_pointer const & /*fieldsToPut*/) {
        return epics::pvData::Status::Ok;
    }

    // bitSet w/ one bit
    virtual epics::pvData::Status authorizeCreateChannelRPC(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    // one could authorize per operation basis
    virtual epics::pvData::Status authorizeRPC(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*arguments*/) {
        return epics::pvData::Status::Ok;
    }

    // read: bitSet w/ one bit (allowed, not allowed) and rest of the bit per field
    virtual epics::pvData::Status authorizeCreateMonitor(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizeMonitor(pvAccessID /*ioid*/) {
        return epics::pvData::Status::Ok;
    }

    // read: bitSet w/ one bit (allowed, not allowed) and rest put/get/set length
    virtual epics::pvData::Status authorizeCreateChannelArray(
        pvAccessID /*ioid*/, epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/) {
        return epics::pvData::Status::Ok;
    }

    // use authorizeGet
    virtual epics::pvData::Status authorizePut(
        pvAccessID /*ioid*/, epics::pvData::PVArray::shared_pointer const & /*dataToPut*/) {
        return epics::pvData::Status::Ok;
    }

    virtual epics::pvData::Status authorizeSetLength(pvAccessID /*ioid*/) {
        return epics::pvData::Status::Ok;
    }

    // introspection authorization
    virtual epics::pvData::Status authorizeGetField(pvAccessID /*ioid*/, std::string const & /*subField*/) {
        return epics::pvData::Status::Ok;
    }

};

class epicsShareClass CAClientSecurityPlugin :
    public NoSecurityPlugin {
protected:
    epics::pvData::PVStructure::shared_pointer m_userAndHost;

    CAClientSecurityPlugin();


public:
    POINTER_DEFINITIONS(CAClientSecurityPlugin);

    static CAClientSecurityPlugin::shared_pointer INSTANCE;

    virtual epics::pvData::PVField::shared_pointer initializationData() {
        return m_userAndHost;
    }

    virtual std::string getId() const {
        return "ca";
    }

    virtual std::string getDescription() const {
        return "CA client security plug-in";
    }
};

class epicsShareClass SecurityPluginRegistry
{
    EPICS_NOT_COPYABLE(SecurityPluginRegistry)
public:

    static SecurityPluginRegistry& instance()
    {
        static SecurityPluginRegistry thisInstance;
        return thisInstance;
    }

    typedef std::map<std::string, std::tr1::shared_ptr<SecurityPlugin> > securityPlugins_t;

    securityPlugins_t& getClientSecurityPlugins()
    {
        return m_clientSecurityPlugins;
    }

    securityPlugins_t& getServerSecurityPlugins()
    {
        return m_serverSecurityPlugins;
    }

    void installClientSecurityPlugin(std::tr1::shared_ptr<SecurityPlugin> plugin)
    {
        m_clientSecurityPlugins[plugin->getId()] = plugin;
        LOG(epics::pvAccess::logLevelDebug, "Client security plug-in '%s' installed.", plugin->getId().c_str());
    }

    void installServerSecurityPlugin(std::tr1::shared_ptr<SecurityPlugin> plugin)
    {
        m_serverSecurityPlugins[plugin->getId()] = plugin;
        LOG(epics::pvAccess::logLevelDebug, "Server security plug-in '%s' installed.", plugin->getId().c_str());
    }

private:
    SecurityPluginRegistry();

    securityPlugins_t m_clientSecurityPlugins;
    securityPlugins_t m_serverSecurityPlugins;
};

}
}

#endif // SECURITY_H
