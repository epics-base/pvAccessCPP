/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.04
 */
#ifndef StopMonitorThread_H
#define StopMonitorThread_H
#include <queue>
#include <cadef.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {


class StopMonitorThread;
typedef std::tr1::shared_ptr<StopMonitorThread> StopMonitorThreadPtr;


class StopMonitorThread :
     public epicsThreadRunable
{
public:
    ~StopMonitorThread();
    virtual void run();
    void start();
    void stop();
    static StopMonitorThreadPtr get();
    void callStop(evid pevid);
    void attachContext(ca_client_context* current_context);
    void addNoEventsCallback(epics::pvData::Event * event);
private:
    StopMonitorThread();

    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForCommand;
    epics::pvData::Event waitForStop;
    std::queue<evid> evidQueue;
    std::queue<epics::pvData::Event *> noEventsCallbackQueue;
    bool isStop;
    bool isAttachContext;
    ca_client_context* current_context;
};


}}}

#endif  /* StopMonitorThread_H */
