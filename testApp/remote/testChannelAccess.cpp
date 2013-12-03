/*
 * testChannelAccess.cpp
 */
#define TESTSERVERNOMAIN

#include <epicsExit.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/serverContext.h>
#include <pv/clientFactory.h>
#include <pv/clientContextImpl.h>

#include "channelAccessIFTest.h"

#include "testServer.cpp"


class ServerContextAction : public Runnable {

  public:

    ServerContextAction(): 
      m_serverThread(){}


    virtual void run()
    {
      testServer(0);
    }


    void stop() {
      testServerShutdown();
    }


    void start() {
      m_serverThread.reset(new epics::pvData::Thread("pvAccess", highPriority, this));
    }


  private:
    auto_ptr<epics::pvData::Thread> m_serverThread;
};


class ChannelAccessIFRemoteTest: public ChannelAccessIFTest  {

  public:
    
    ChannelAccessIFRemoteTest(): m_serverContextAction() 
    {
      m_serverContextAction.start();
      ClientFactory::start();
    }


    virtual ChannelProvider::shared_pointer getChannelProvider() {
      return getChannelAccess()->getProvider(
          ClientContextImpl::PROVIDER_NAME);
    }


    virtual long getTimeoutSec() {
      return 10;
    }


    virtual bool isLocal() { return false;}


    ~ChannelAccessIFRemoteTest() {
      m_serverContextAction.stop();   
      ClientFactory::stop();
      structureChangedListeners.clear();
      structureStore.clear();
    }

  private:
    ServerContextAction m_serverContextAction;
};


MAIN(testChannelProvider)
{
  ChannelAccessIFRemoteTest caRemoteTest;
  return caRemoteTest.runAllTest();
}
