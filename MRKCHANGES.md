========
Overview
========

pvaCientCPP did not terminate properly.
It has a singleton class named **PvaClient**.
When the singleton is created it calls:

    ClientFactory::start();

The destructor for **PvaClient** calls:

    ClientFactory::stop();

This call results in the following message:

    terminate called after throwing an instance of 'epicsMutex::invalidMutex'
       what():  epicsMutex::invalidMutex() 

Further investigation found that this was caused by a mutex belonging to ChannelProviderRegistry.

If fact it appeared that the only way the crash did not occur is if the client code was of the form:

    int main(int argc,char *argv[])
    {
         ClientFactory::start();
         ...
         ClientFactory::stop();
    }

While investigating this problem I also realized that the existing implementation of
ClientFactory only worked if a single call was made to start and a single call to stop.

In pvAccessCPP changes were made to the followimg:

1) ChannelProviderRegistry
2) ClientFactory
3) CAChannelProviderFactory

These are discussed below.

Changes were also made to pvaClientCPP.
The change was to use the new ChannelProviderRegistry methods.

All these changes are in the github repositories belonging to mrkraimer.

-----------------------
ChannelProviderRegistry
-----------------------

The following new method is added to ChannelProviderRegistry:


    static ChannelProviderRegistry::shared_pointer getChannelProviderRegistry();

**getChannelProviderRegistry** creates the single instance of **ChannelProviderRegistry**
the first time it is called and always returns a shared pointer to the single
instance,

Any facility that calls the start/stop methods of any channelProvider factory must keep the shared pointer
until the facility no longer requires use of the registry or any channel provider the facility uses.

The above methods replace the following functions:

    // THE FOLLOWING SHOULD NOT BE CALLED
    epicsShareFunc ChannelProviderRegistry::shared_pointer getChannelProviderRegistry()      EPICS_DEPRECATED;
    epicsShareFunc void registerChannelProviderFactory(
         ChannelProviderFactory::shared_pointer const & channelProviderFactory) EPICS_DEPRECATED;
    epicsShareFunc void unregisterChannelProviderFactory(
         ChannelProviderFactory::shared_pointer const & channelProviderFactory) EPICS_DEPRECATED;
     epicsShareFunc void unregisterAllChannelProviderFactory() EPICS_DEPRECATED;

Note that the methods are all deprecated.
They were kept so that existing code will still compile and run.
Any code that uses these methods may see the invalidMutex exception on termination.

-------------
ClientFactory
-------------

This has two major changes:

1) It uses the new **ChannelProviderRegistry** methods,

2) It allows multiple calls to start and stop.


------------------------
CAChannelProviderFactory
------------------------

This has two major changes:

1) It uses the new **ChannelProviderRegistry** methods,

2) It allows multiple calls to start and stop.



