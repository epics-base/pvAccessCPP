#ifndef PROVIDERS_H
#define PROVIDERS_H

/**
@page providers ChannelProvider API

@tableofcontents

@section providers_roles Roles

The Client and Server APIs revolve around the epics::pvAccess::ChannelProvider class.

In the following discussion the @ref providers_client calls methods of ChannelProvider and associated classes.
The @ref providers_server implements ChannelProvider and associated classes and is called by a "client.

By convention, instances of ChannelProvider are registered through a
epics::pvAccess::ChannelProviderRegistry
Typically the global instance
epics::pvAccess::getChannelProviderRegistry()

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

For convenience each Operation class has a member typedef for it's associated Requester, and vis. versa.
For example ChannelGet::requester_type is ChannelGetRequester
and ChannelGetRequester::operation_type is ChannelGet.

The "Requester" classes are implemented by the Client role
and called by the Server role to give notification to the client of certain events.
For example,
epics::pvAccess::ChannelRequester::channelStateChange()
is called when a Channel becomes (dis)connected.

A "Requester" sub-class must be provided when each "Operation" is created.
This Requester then becomes bound to the Operation.

@subsection providers_ownership shared_ptr<> and Ownership

"Operations" and "Requesters" are always handled via std::tr1::shared_ptr.

In the following dicussions an instance of std::tr1::shared_ptr is referred to as a "reference",
specifically a strong reference.
The associated std::tr1::weak_ptr is referred to as a weak reference.

shared_ptr instances can exist on the stack (local variables) or as
struct/class members.

Situations where a object contains are reference to itself, either directly or indirectly,
are known as "reference loops".
As long as a reference loop persists any cleanup of resources associated with
the shared_ptr (s) involved will not be done.
A reference loop which is never broken is called a "reference leak".

In order to avoid reference leaks, required relationships between
various classes will be described, and some rules stated.

In discussing the usage of an API diagrams like the following will be used to illustrate roles and ownership requirements.

The distinction of what is "user" code will depend on context.
For example, when discussing the Client role, epics::pvAccess::Channel will not be implemented by "user code".
When discussing the Server role, user code will implement Channel.

@subsubsection providers_ownership_unique Uniqueness

A shared_ptr is said to be unique if it's method shared_ptr::unique() returns true.

The general rule is that functions which create/allocate new objects using shared_ptr must yield unique shared_ptr.
Yielding a non-unique shared_ptr is a sign that an internal reference leak could occur.

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

@subsubsection providers_client_operations_lifetime Operation Lifetime and (dis)connection

An Operation can be created at any time regardless of whether a Channel is connected or not.
An Operation will remain associated with a Channel through (re)connection and disconnection.

@subsection provides_client_ownership Client Ownership

Implicit ownership in classes outside the control of client code.

@dotfile client_ownership.dot Client implicit relationships

- Channel hold strong refs. of ChannelProvider and ChannelRequester
- Channel holds weak refs to all Operations
- Operation holds strong refs to the corresponding Requester, and Channel

@section providers_server Server Role
*/

#endif /* PROVIDERS_H */
