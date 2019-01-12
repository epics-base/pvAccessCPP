/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SECURITY_H
#define SECURITY_H

/** @page pva_security PVA Security
 *
 * Summary of PVA auth. process.
 *
 @msc
     arcgradient = 8;

     CP [label="Client Plugin"], CS [label="Client Session"], C [label="Client"], S [label="Server"], SS [label="Server Session"], SP [label="Server Plugin"];

     C:>S  [label="Opens TCP connection"];
     S box SP [label="Server lists plugins"];
     S=>SP [label="isValidFor()"];
     C<:S  [label="CONNECTION_VALIDATION"];
     CP box C [label="Client lists plugins"];
     CP box C [label="Client select plugin"];
     CP<=C [label="createSession()"];
     CP>>C [label="Returns Session"];
     CS<=C [label="initializationData()"];
     CS>>C [label="Initial payload"];
     C:>S  [label="CONNECTION_VALIDATION"];
     S=>SP [label="createSession()"];
     S<<SP [label="Returns Session"];
     ---   [label="Optional (Repeatable)"];
     S<=SS [label="sendSecurityPluginMessage()"];
     C<:S  [label="AUTHZ"];
     CS<=C [label="messageReceived()"];
     ...;
     CS=>C [label="sendSecurityPluginMessage()"];
     C:>S  [label="AUTHZ"];
     S=>SS [label="messageReceived()"];
     ...;
     ---   [label="Completion"];
     S<=SS [label="authenticationCompleted()"];
     C<:S  [label="CONNECTION_VALIDATED"];
     CS<=C [label="authenticationComplete()"];
 @endmsc
 *
 * Ownership
 *
 @dot
 digraph authplugin {
   External;
   AuthenticationRegistry [shape=box];
   AuthenticationPlugin [shape=box];
   AuthenticationPluginControl [shape=box];
   AuthenticationSession [shape=box];
   External -> AuthenticationRegistry;
   AuthenticationRegistry -> AuthenticationPlugin;
   External -> AuthenticationSession;
   AuthenticationSession -> AuthenticationPluginControl [color=green, style=dashed];
   External -> AuthenticationPluginControl;
   AuthenticationPluginControl -> AuthenticationSession;
   AuthenticationPlugin -> AuthenticationSession [style=dashed];
 }
 @enddot
 *
 * Locking
 *
 * All methods of AuthenticationSession are called from a single thread.
 * Methods of AuthenticationPlugin and AuthenticationPluginControl can be called
 * from any threads.
 *
 * AuthenticationPluginControl is an Operation, AuthenticationSession is a Requester.
 * @see provider_roles_requester_locking
 */

#ifdef epicsExportSharedSymbols
#   define securityEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <string>
#include <osiSock.h>
#include <epicsMutex.h>

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

/** @brief Information provded by a client to a server-type ChannelProvider.
 *
 * All peers must be identified by a peer name, which may be a network address (IP+port#) and transport.
 * Peer names must be unique for a given transport.
 *
 * Transport names include:
 *
 * # "local" in-process client.  Peer name is optional and arbitrary.  Must set local flag.
 * # "pva" PVA over TCP.  Used by PVA client provider.  Peer name is IP address and TCP port number as "XXX.XXX.XXX.XXX:YYYYY".
 *
 * Authority names include:
 *
 * "anonymous" - No credentials provided.  Must not set identified flag.
 * "plain" - Unauthenticated credential.
 */
struct epicsShareClass PeerInfo {
    POINTER_DEFINITIONS(PeerInfo);

    static size_t num_instances;

    std::string peer;      //!< network address of remote peer.  eg. "192.168.1.1:5075".
    std::string transport; //!< transport protocol used          eg. "pva".  Must not be empty.
    std::string authority; //!< authentication mechanism used.   eg. "anonymous" or "gssapi".  Must not be empty.
    std::string realm;     //!< scope of authority.              eg. "mylab.gov"
    std::string account;   //!< aka. user name

    //! NULL or extra authority specific information.
    pvData::PVStructure::const_shared_pointer aux;

    typedef std::set<std::string> roles_t;
    //! Set of strings which may be used to modify access control decisions.
    roles_t roles;

    unsigned transportVersion; //!< If applicable, the protocol minor version number

    // attributes for programatic consumption
    bool local; //!< Short-hand for transport=="local"
    bool identified; //!< Short-hand for authority!="anonymous"

    PeerInfo();
    virtual ~PeerInfo();
};

/** A particular authentication exchange.  See AuthenticationPlugin::createSession()
 *
 * @note Must not hold a strong reference to AuthenticationPluginControl
 */
class epicsShareClass AuthenticationSession
{
public:
    POINTER_DEFINITIONS(AuthenticationSession);

    virtual ~AuthenticationSession();

    //! For client plugins only, call to find the payload returned with CONNECTION_VALIDATION.
    //! May return NULL.
    virtual epics::pvData::PVStructure::const_shared_pointer initializationData()
    { return epics::pvData::PVStructure::const_shared_pointer(); }

    //! Called when an AUTHZ message is recieved from the peer.
    //! See AuthenticationPluginControl::sendSecurityPluginMessage().
    //! callee accepts ownership of data, which will not be modified.
    virtual void messageReceived(epics::pvData::PVStructure::const_shared_pointer const & data) {}

    /** For client plugins only.  Notification that server has declared the exchange complete.
     * @param status Check Status::isSuccess()
     * @param peer Final information about pe
     */
    virtual void authenticationComplete(const epics::pvData::Status& status) {}
};

//! Callbacks for use by AuthenticationSession
class epicsShareClass AuthenticationPluginControl
{
public:
    POINTER_DEFINITIONS(AuthenticationPluginControl);
    virtual ~AuthenticationPluginControl();

    //! Send AUTHZ to peer with payload.
    //! caller gives up ownership of data, which must not be modified.
    virtual void sendSecurityPluginMessage(epics::pvData::PVStructure::const_shared_pointer const & data) = 0;

    /** Called by server plugin to indicate the the exchange has completed.
     *
     * @param status If !status.isSuccess() then the connection will be closed without being used.
     * @param peer Partially initialized PeerInfo.  See AuthenticationPlugin::createSession().
     *             PeerInfo::realm and/or PeerInfo::account will now be considered valid.
     *             Caller transfers ownership to callee, which may modify.
     */
    virtual void authenticationCompleted(const epics::pvData::Status& status,
                                         const std::tr1::shared_ptr<PeerInfo>& peer) = 0;
};

//! Actor through which authentication exchanges are initiated.
class epicsShareClass AuthenticationPlugin
{
public:
    POINTER_DEFINITIONS(AuthenticationPlugin);
    virtual ~AuthenticationPlugin();

    /** Allow this plugin to be advertised to a particular peer.
     *
     * At this point the PeerInfo has only been partially initialized with
     * transport/protocol specific information: PeerInfo::peer, PeerInfo::transport, and PeerInfo::transportVersion.
     */
    virtual bool isValidFor(const PeerInfo& peer) const { return true; }

    /** Begin a new session with a peer.
     *
     * @param peer Partially initialized PeerInfo.  See isValidFor().
     *        PeerInfo::authority is also set.
     *        Caller transfers ownership to callee, which may modify.
     * @param control callee uses to asynchronously continue, and complete the session.
     * @param data Always NULL for client-type plugins.  For server-type plugins,
     *        the result of initializationData() from the peer
     */
    virtual std::tr1::shared_ptr<AuthenticationSession> createSession(
        const std::tr1::shared_ptr<PeerInfo>& peer,
        std::tr1::shared_ptr<AuthenticationPluginControl> const & control,
        epics::pvData::PVStructure::shared_pointer const & data) = 0;
};

/** Registry(s) for plugins
 */
class epicsShareClass AuthenticationRegistry
{
    EPICS_NOT_COPYABLE(AuthenticationRegistry) // would need locking
public:
    POINTER_DEFINITIONS(AuthenticationRegistry);

private:
    typedef std::map<int, std::pair<std::string, AuthenticationPlugin::shared_pointer> > map_t;
    map_t map;
    mutable epicsMutex mutex;
public:
    typedef std::vector<map_t::mapped_type> list_t;

    //! The client side of the conversation
    static AuthenticationRegistry& clients();
    //! The server side of the conversation
    static AuthenticationRegistry& servers();

    AuthenticationRegistry() {}
    ~AuthenticationRegistry();

    //! Save a copy of the current registry in order of increasing priority
    void snapshot(list_t& plugmap) const;

    /** @brief Add a new plugin to this registry.
     *
     @param prio Order in which plugins are considered.  highest is preferred.
     @param name Name under which this plugin will be known
     @param plugin Plugin instance
     */
    void add(int prio, const std::string& name, const AuthenticationPlugin::shared_pointer& plugin);
    //! Remove an existing entry.  Remove true if the entry was actually removed.
    bool remove(const AuthenticationPlugin::shared_pointer& plugin);
    //! Fetch a single plugin explicitly by name.
    //! @returns NULL if no entry for this name is available.
    AuthenticationPlugin::shared_pointer lookup(const std::string& name) const;
};

//! I modify PeerInfo after authentication is complete.
//! Usually to update PeerInfo::roles
class epicsShareClass AuthorizationPlugin
{
public:
    POINTER_DEFINITIONS(AuthorizationPlugin);

    virtual ~AuthorizationPlugin();

    //! Hook to modify PeerInfo
    virtual void authorize(const std::tr1::shared_ptr<PeerInfo>& peer) =0;
};

class epicsShareClass AuthorizationRegistry
{
    EPICS_NOT_COPYABLE(AuthorizationRegistry)
public:
    POINTER_DEFINITIONS(AuthenticationRegistry);

    static AuthorizationRegistry &plugins();

    AuthorizationRegistry();
    ~AuthorizationRegistry();

private:
    typedef std::map<int, AuthorizationPlugin::shared_pointer> map_t;
    map_t map;
    void *busy;
    mutable epicsMutex mutex;
public:

    void add(int prio, const AuthorizationPlugin::shared_pointer& plugin);
    bool remove(const AuthorizationPlugin::shared_pointer& plugin);
    void run(const std::tr1::shared_ptr<PeerInfo>& peer);
};

/** @brief Query OS specific DB for role/group names assocated with a user account.
 * @param account User name
 * @param roles Role names are added to this set.  Existing names are not removed.
 */
epicsShareFunc
void osdGetRoles(const std::string &account, PeerInfo::roles_t& roles);

}
}

#endif // SECURITY_H
