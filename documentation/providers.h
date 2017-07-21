#ifndef PROVIDERS_H
#define PROVIDERS_H

/**
@page providers ChannelProvider API

@tableofcontents

The epics::pvAccess namespace.
See pv/pvAccess.h header.

@code
#include <pv/configuration.h>
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
@endcode

@section providers_roles Roles

The Client and Server APIs revolve around the epics::pvAccess::ChannelProvider class.

In the following discussion the @ref providers_client calls methods of ChannelProvider and associated classes.
The @ref providers_server implements ChannelProvider and associated classes and is called by a client.

By convention, instances of ChannelProvider are registered and retrieved through one of
epics::pvAccess::ChannelProviderRegistry::clients()
or
epics::pvAccess::ChannelProviderRegistry::servers()

@subsection provider_roles_requester Operation and Requester

The classes associated with ChannelProvider come in pairs.
eg.

- epics::pvAccess::Channel and epics::pvAccess::ChannelRequester
- epics::pvAccess::ChannelGet and epics::pvAccess::ChannelGetRequester
- epics::pvAccess::ChannelPut and epics::pvAccess::ChannelPutRequester
- epics::pvAccess::Monitor and epics::pvAccess::MonitorRequester
- epics::pvAccess::ChannelRPC and epics::pvAccess::ChannelRPCRequester
- epics::pvAccess::ChannelProcess and epics::pvAccess::ChannelProcessRequester
- epics::pvAccess::ChannelPutGet and epics::pvAccess::ChannelPutGetRequester
- epics::pvAccess::ChannelArray and epics::pvAccess::ChannelArrayRequester

In the following discussions the term "Operation" refers to eg. Channel, ChannelGet, or similar
while "Requester" refers to ChannelRequester, ChannelGetRequester, or similar.

The "Requester" classes are implemented by the Client role
and called by the Server role to give notification to the client of certain events.
For example,
epics::pvAccess::ChannelRequester::channelStateChange()
is called when a Channel becomes (dis)connected.

A "Requester" sub-class must be provided when each "Operation" is created.
This Requester then becomes bound to the Operation.

@note An exception to this is epics::pvAccess::ChannelProvider::createChannel()
      Where a epics::pvAccess::ChannelRequester may be omitted.

For convenience each Operation class has a member typedef for it's associated Requester, and vis. versa.
For example ChannelGet::requester_type is ChannelGetRequester
and ChannelGetRequester::operation_type is ChannelGet.

@subsubsection provider_roles_requester_locking

Operations methods may call requester methods, and vis versa.
The following rules must be followed to avoid deadlocks.

- No locks must be held when Requester methods are called.
- Locks may be held when Operation methods are called.

These rules place the burdon of avoiding deadlocks on the ChannelProvider implementation (Server role).

Clients must still be aware when some Operation methods can call some Requester methods recursively,
and consider this when locking.

For example, the following call stack may legitimetly occur for a ChannelProvider
to for a Get which accesses locally stored data.

- Channel::createChannelGet()
 - ChannelGetRequester::channelGetConnect()
  - ChannelGet::get()
   - ChannelGetRequester::getDone()

Thus care should be taken when calling ChannelGet::get() from within ChannelGetRequester::getDone()
to avoid infinite recursion.

@subsection providers_ownership shared_ptr<> and Ownership

"Operations" and "Requesters" are always handled via std::tr1::shared_ptr.

In the following dicussions an instance of std::tr1::shared_ptr is referred to as a "reference",
specifically a strong reference.
The associated std::tr1::weak_ptr is referred to as a weak reference.

shared_ptr instances can exist on the stack (local variables) or as
struct/class members.

Situations where an object contains a reference to itself, either directly or indirectly,
are known as "reference loops".
As long as a reference loop persists, any cleanup of resources associated with
the shared_ptr (s) involved will not be done.
A reference loop which is never broken is called a "reference leak".

In order to avoid reference leaks, required relationships between
various classes will be described, and some rules stated.

In discussing the usage of an API diagrams like the following will be used to illustrate roles and ownership requirements.

The distinction of what is "user" code will depend on context.
For example, when discussing the Client role, epics::pvAccess::Channel will not be implemented by "user code".
When discussing the Server role, user code will implement Channel.

@subsubsection providers_ownership_unique Uniqueness

A shared_ptr is said to be unique if it is the only strong reference to the underlying object.
In this case shared_ptr::unique() returns true.

The general rule is that functions which create/allocate new objects using shared_ptr must yield a unique shared_ptr.
Yielding a non-unique shared_ptr is a sign that an internal reference leak exists.

@dotfile ownership.dot "shared_ptr relationships"

- A box denotes a class implemented by user code
- An oval denotes a class not implemented by user code
- A red line is a shared_ptr<>
- A green line is a weak_ptr<>
- A dashed line indicates a relationship which is outside the control of user code

@code
struct MyClient {
   epics::pvAccess::Channel::shared_ptr channel;
};
struct MyChannelRequester : public epics::pvAccess::ChannelRequester
{
    std::tr1::weak_ptr<MyClient> client;
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) {
        std::tr1::shared_ptr<MyClient> C(client.lock());
        if(!C) return;
        ...
    }
};
@endcode

In this example user code implements a custom MyClient class and a sub-class of ChannelRequester in order
to make use of a Channel.
In order to avoid a reference loop, the sub-class of ChannelRequester uses a weak_ptr
to refer to MyClient during channelStateChange() events.

@section providers_client Client Role

A client will by configured with a certain provider name string.
It will begin by passing this string to
epics::pvAccess::ChannelProviderRegistry::createProvider()
to obtain a ChannelProvider instance.

Custom configuration of provider can be accomplished by
passing a
epics::pvAccess::Configuration
to createProvider().
A Configuration is a set of string key/value parameters
which the provider may use as it sees fit.

If a Configuration is not provided (or NULL) then
the provider will use some arbitrary defaults.
By convention default Configuration use the process environment.

@code
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
// add "pva" to registry
epics::pvAccess::ClientFactory::start();
// create a new client instance.
epics::pvAccess::ChannelProvider::shared_pointer prov;
prov = epics::pvAccess::getChannelProviderRegistry()->createProvider("pva");
// createProvider() returns NULL if the named provider hasn't been registered
if(!prov)
    throw std::runtime_error("PVA provider not registered");
@endcode

@subsection providers_client_channel Client Channel

The primary (and in many cases only) ChannelProvider method of interest is
epics::pvAccess::ChannelProvider::createChannel()
from which a new
epics::pvAccess::Channel
can be obtained.

Each call to createChannel() produces a "new" std::shared_ptr<Channel>
which is uniquely owned by the caller (see @ref providers_ownership_unique).
As such, the caller must keep a reference to to the Channel or it will be destroyed.
This may be done explicitly, or implicitly by storing a reference to an Operation.

@note The returned Channel does *not* hold a strong reference for the ChannelProvider
      from which it was created.
      User code *must* keep a reference to the provider as long as Channels are in use.
      All Channels are automatically closed when their provider is destroyed.

A Channel can be created at any time, and shall succeed as long as
the provided name and address are syntactically valid, and the priority is in the valid range.

When created, a Channel may or may not already be in the Connected state.

On creation epics::pvAccess::ChannelRequester::channelCreated will be called
before createChannel() returns.

Notification of connection state changes are made through
epics::pvAccess::ChannelRequester::channelStateChange()
as well as through the *Connect() and channelDisconnect() methods
of Requesters of any Operations on a Channel (eg.
epics::pvAccess::MonitorRequester::channelDisconnect()
).

@subsection providers_client_operations Client Operations

This section describes commonalities between the various Operation supported:
Get, Put, Monitor, RPC, PutGet, Process, and Array.

An Operation is created/allocated with one of the create* methods of Channel.
All behave in a similar way.

- epics::pvAccess::Channel::createChannelGet()
- epics::pvAccess::Channel::createChannelPut()
- epics::pvAccess::Channel::createMonitor()
- epics::pvAccess::Channel::createChannelRPC()
- epics::pvAccess::Channel::createChannelPutGet()
- epics::pvAccess::Channel::createChannelProcess()
- epics::pvAccess::Channel::createChannelArray()

The created Operation is unique (see @ref providers_ownership_unique).

The \*Connect() method of the corresponding Requester will be called when
the Operation is "ready" (underlying Channel is connected).
This may happen before the create* method has returned, or at some time later.

- epics::pvAccess::ChannelGetRequester::channelGetConnect()
- epics::pvAccess::ChannelPutRequester::channelPutConnect()
- epics::pvAccess::MonitorRequester::monitorConnect()
- epics::pvAccess::ChannelRPCRequester::channelRPCConnect()
- epics::pvAccess::ChannelPutGetRequester::channelPutGetConnect()
- epics::pvAccess::ChannelProcessRequester::channelProcessConnect()
- epics::pvAccess::ChannelArrayRequester::channelArrayConnect()

When the underlying Channel becomes disconnected or is destroyed,
then the channelDisconnect() method of each Requester is called (eg.
see epics::pvAccess::ChannelBaseRequester::channelDisconnect()
).
All operations are implicitly cancelled/stopped before channelDisconnect() is called.

@subsubsection providers_client_operations_lifetime Operation Lifetime and (dis)connection

An Operation can be created at any time regardless of whether a Channel is connected or not.
An Operation will remain associated with a Channel through (re)connection and disconnection.

@subsubsection providers_client_operations_exec Executing an Operation

After an Operation becomes ready/connected an additional step is necessary to request data.

- epics::pvAccess::ChannelGet::get()
- epics::pvAccess::ChannelPut::get()
- epics::pvAccess::ChannelPut::put()
- epics::pvAccess::ChannelRPC::request()
- epics::pvAccess::ChannelPutGet::putGet()
- epics::pvAccess::ChannelPutGet::getPut()
- epics::pvAccess::ChannelPutGet::getGet()
- epics::pvAccess::ChannelProcess::process()
- epics::pvAccess::ChannelArray::putArray()
- epics::pvAccess::ChannelArray::getArray()
- epics::pvAccess::ChannelArray::getLength()
- epics::pvAccess::ChannelArray::setLength()

Once one of these methods is called to execute an operation,
none may be again until the corresponding completion callback is called,
or the operation is cancel()ed (or epics::pvAccess::Monitor::stop() ).

- epics::pvAccess::ChannelGetRequester::getDone()
- epics::pvAccess::ChannelPutRequester::getDone()
- epics::pvAccess::ChannelPutRequester::putDone()
- epics::pvAccess::ChannelRPCRequester::requestDone()
- epics::pvAccess::ChannelPutGetRequester::putGetDone()
- epics::pvAccess::ChannelPutGetRequester::getPutDone()
- epics::pvAccess::ChannelPutGetRequester::getGetDone()
- epics::pvAccess::ChannelProcessRequester::processDone()
- epics::pvAccess::ChannelArrayRequester::putArrayDone()
- epics::pvAccess::ChannelArrayRequester::getArrayDone()
- epics::pvAccess::ChannelArrayRequester::getLengthDone()
- epics::pvAccess::ChannelArrayRequester::setLengthDone()

@subsubsection providers_client_operations_monitor Monitor Operation

epics::pvAccess::Monitor operations are handled differently than others as
more than one subscription update may be delivered after start() is called.

During or after epics::pvAccess::MonitorRequester::monitorConnect()
it is necessary to call epics::pvAccess::Monitor::start()
to begin receiving subscription updates.

The epics::pvAccess::Monitor::poll() and epics::pvAccess::Monitor::release()
methods access a FIFO queue of subscription updates which have been received.
The epics::pvAccess::MonitorRequester::monitorEvent() method is called
when this FIFO becomes not empty.

@note The pvAccess::MonitorRequester::monitorEvent() is called from a server internal thread
      which may be shared with other operations.
      In order to avoid delaying other channels/operations
      it is recommended to use monitorEvent() as notification for a client
      specific worker thread where poll() and release() are called.

epics::pvAccess::MonitorRequester::unlisten() is called to indicate a subscription
has reached a definite end without an error.
Not all subscription sources will use this.

@warning It is critical that any non-NULL MonitorElement returned by poll()
         must be passed to release().
         Failure to do this will result in a resource leak and possibly stall
         the monitor.
         See epics::pvAccess::Monitor::Stats::noutstanding

epics::pvAccess::Monitor::getStats() can help diagnose problems related
to the Monitor FIFO.
See epics::pvAccess::Monitor::Stats.

@subsection provides_client_ownership Client Ownership

The following shows the implicit ownership in classes outside the control of client code,
as well as the expected ownerships of of Client user code.
"External" denotes references stored by Client objects which can't participate in reference cycles.

@dotfile client_ownership.dot Client implicit relationships

- Channel holds weak ref. to ChannelProvider
- ChannelProvider holds weak ref. to Channel
- Channel holds weak ref. to ChannelRequester
- Channel holds weak refs to all Operations
- Operations hold weak refs to the corresponding Requester, and Channel

@subsection provides_client_examples Client Examples

- @ref examples_getme
- @ref examples_monitorme


@section providers_server Server Role
*/

#endif /* PROVIDERS_H */
