/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#ifndef PVA_SERVER_H
#define PVA_SERVER_H

#include <string>
#include <vector>
#include <map>

#include <shareLib.h>
#include <pv/sharedPtr.h>
#include <pv/sharedVector.h>

namespace epics{namespace pvAccess{
class ChannelProvider;
class Channel;
class ChannelRequester;
struct PeerInfo; // see pv/security.h
}} // epics::pvAccess

//! See @ref pvas API
namespace pvas {

/** @addtogroup pvas Server API
 *
 * PVA Server Providers, for use with a PVA epics::pvAccess::ServerContext
 *
 * These are implementations of epics::pvAccess::ChannelProvider which manage "PVs",
 * which are sources of epics::pvAccess::Channel instances.  Typically SharedPV .
 *
 * Two containers are provided StaticProvider, and for some special cases DynamicProvider.
 * It is recommended to use StaticProvider where possible, with DynamicProvider for exception cases.
 *
 * A StaticProvider maintains an internal lookup table of StaticProvider::ChannelBuilder (aka. SharedPV).
 * This table is manipulated by StaticProvider::add() and StaticProvider::remove(), which can
 * be called at any time.
 *
 * A DynamicProvider does not maintain an internal lookup table.  Instead it provides
 * the DynamicProvider::Handler interface, through which remote search and connection
 * requests are delivered.
 *
 * See @ref examples_mailbox for a working example.
 *
 @code
 namespace pva = epics::pvAccess;
 pvas::SharedPV::shared_pointer pv(pvas::SharedPV::buildMailbox());
 pvas::StaticProvider sprov("arbitrary");
 pva::ServerContext::shared_pointer server(
    pva::ServerContext::create(
        pva::ServerContext::Config() .provider(sprov.provider()) ));
 sprov->add("pv:name", pv);
 @endcode
 *
 * @section pvas_sharedptr Server API shared_ptr Ownership
 *
 * shared_ptr<> relationships internal to server API classes.
 * Solid red lines are shared_ptr<>.
 * Dashed red lines are shared_ptr<> which may exist safely in user code.
 * Rectangles are public API classes.  Circles are internal classes.
 * "ChannelProvider" is an arbitrary ChannelProvider, possibly StaticProvider or DynamicProvider.
 *
 @dot "Internal shared_ptr<> relationships.
 digraph sspv {
   SharedPV [shape="box"];
   SharedPVHandler [label="SharedPV::Handler", shape="box"];
   SharedChannel [shape="ellipse"];
   ChannelOp [label="SharedPut/RPC/MonitorFIFO", shape="ellipse"];

   DynamicProvider [shape="box"];
   DynamicHandler [label="DynamicProvider::Handler", shape="box"];
   StaticProvider [shape="ellipse"];

   ChannelRequester [shape="ellipse"];
   ChannelProvider [shape="box"];

   ServerContext [shape="box"];

   ChannelProvider -> SharedPV [color="red", style="dashed"];
   DynamicProvider -> DynamicHandler [color="red"];
   StaticProvider -> SharedPV [color="red"];
   ServerContext -> ChannelProvider [color="red"];
   ServerContext -> DynamicProvider [color="red"];
   ServerContext -> StaticProvider [color="red"];
   ServerContext -> ChannelRequester [color="red"];
   ServerContext -> SharedChannel [color="red"];
   ServerContext -> ChannelOp [color="red"];
   SharedPV -> SharedPVHandler [color="red"];
   SharedChannel -> SharedPV [color="red"];
   ChannelOp -> SharedChannel [color="red"];
 }
 @enddot
 *
 * @{
 */

/** @brief A Provider based on a list of SharedPV instance.
 *
 * SharedPV instances may be added/removed at any time.  So it is only "static"
 * in the sense that the list of PV names is known to StaticProvider at all times.
 *
 * @see @ref pvas_sharedptr
 */
class epicsShareClass StaticProvider {
public:
    POINTER_DEFINITIONS(StaticProvider);
    struct Impl;
private:
    std::tr1::shared_ptr<Impl> impl; // const after ctor
public:

    //! Interface for something which can provide Channels.  aka A "PV".  Typically a SharedPV
    struct epicsShareClass ChannelBuilder {
        POINTER_DEFINITIONS(ChannelBuilder);
        virtual ~ChannelBuilder();
        //! called to create a new Channel through the given ChannelProvider
        virtual std::tr1::shared_ptr<epics::pvAccess::Channel> connect(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider,
                                                                       const std::string& name,
                                                                       const std::tr1::shared_ptr<epics::pvAccess::ChannelRequester>& requester) =0;
        //! Disconnect all Channels created through the given ChannelProvider.
        //! destroy==true if the ChannelProvider is shutting down.
        virtual void disconnect(bool destroy, const epics::pvAccess::ChannelProvider* provider) =0;
    };
private:
    typedef std::map<std::string, std::tr1::shared_ptr<ChannelBuilder> > builders_t;
public:
    typedef builders_t::const_iterator const_iterator;

    //! Build a new, empty, provider.
    //! @param name Provider Name.  Only relevant if registerAsServer() is called, then must be unique in this process.
    explicit StaticProvider(const std::string& name);
    ~StaticProvider();

    //! Call Channelbuilder::close(destroy) for all currently added ChannelBuilders.
    //! @see SharedPV::close()
    //! @note Provider locking rules apply (@see provider_roles_requester_locking).
    void close(bool destroy=false);

    //! Add a PV (eg. SharedPV) to this provider.
    void add(const std::string& name,
             const std::tr1::shared_ptr<ChannelBuilder>& builder);
    //! Remove a PV.  Closes any open Channels to it.
    //! @returns the PV which has been removed.
    //! @note Provider locking rules apply (@see provider_roles_requester_locking).
    std::tr1::shared_ptr<ChannelBuilder> remove(const std::string& name);

    //! Fetch the underlying ChannelProvider.  Usually to build a ServerContext around.
    std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> provider() const;

    // iterate through currently add()'d PVs.  Iteraters are invalidated by concurrent add() or remove()
    const_iterator begin() const;
    const_iterator end() const;
};

/** @brief A Provider which has no pre-configured list of names.
 *
 * Through an associated Handler, this provider sees all searchs, and may claim
 * them.
 *
 * @see @ref pvas_sharedptr
 */
class epicsShareClass DynamicProvider {
public:
    POINTER_DEFINITIONS(DynamicProvider);
    struct Impl;
private:
    std::tr1::shared_ptr<Impl> impl; // const after ctor
public:
    //! A single client serach request.  May be associated with more than one name
    class Search {
        friend struct Impl;
        bool isclaimed;
        std::string cname;
        const ::epics::pvAccess::PeerInfo* peerinfo;
        Search(const std::string& name, const ::epics::pvAccess::PeerInfo* peer)
            :isclaimed(false),cname(name),peerinfo(peer)
        {}
    public:
        //! The name being queried
        const std::string& name() const { return cname; }
        //! Stake a claim.
        bool claimed() const { return isclaimed; }
        //! Has been claimed()
        void claim() { isclaimed = true; }
        //! Information about peer making search request.
        //! May be NULL if not information is available.
        //! @since >7.1.0
        const ::epics::pvAccess::PeerInfo* peer() const { return peerinfo; }
    };
    typedef std::vector<Search> search_type;

    /** Callbacks associated with DynamicProvider.
     *
     * For the purposes of locking, this class is a Requester (see @ref provider_roles_requester_locking).
     * It's methods will not be called with locks held.  It may call
     * methods which lock.
     */
    struct epicsShareClass Handler {
        POINTER_DEFINITIONS(Handler);
        typedef epics::pvData::shared_vector<std::string> names_type;
        virtual ~Handler() {}
        //! Called with name(s) which some client is searching for
        virtual void hasChannels(search_type& name) =0;
        //! Called when a client is requesting a list of channel names we provide.  Callee should set dynamic=false if this list is exhaustive.
        virtual void listChannels(names_type& names, bool& dynamic) {}
        //! Called when a client is attempting to open a new channel to this SharedPV
        virtual std::tr1::shared_ptr<epics::pvAccess::Channel> createChannel(const std::tr1::shared_ptr<epics::pvAccess::ChannelProvider>& provider,
                                                                             const std::string& name,
                                                                             const std::tr1::shared_ptr<epics::pvAccess::ChannelRequester>& requester) =0;
        //! Called when the last reference to a DynamicProvider is released.  Should close any channels.
        virtual void destroy() {}
    };

    //! Build a new provider.
    //! @param name Provider Name.  Only relevant if registerAsServer() is called, then must be unique in this process.
    //! @param handler Our callbacks.  Internally stored a shared_ptr (strong reference).
    DynamicProvider(const std::string& name,
                    const std::tr1::shared_ptr<Handler>& handler);
    ~DynamicProvider();

    Handler::shared_pointer getHandler() const;

    //void close();

    //! Fetch the underlying ChannelProvider.  Usually to build a ServerContext around.
    std::tr1::shared_ptr<epics::pvAccess::ChannelProvider> provider() const;
};

//! @}

} // namespace pvas

#endif // PVA_SERVER_H
