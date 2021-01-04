// testConveyor.cpp
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

/*
 * Tests for notifierConveyor:
 *    1. Basic queueing functionality.
 *    2. Queue accepts notification while conveyor is busy.
 *    3. Client's notifier can re-queue itself.
 *    4. Delete a client inside its own notifier.
 *    5. Queue Notification to an already-dead client.
 */

#include <cstddef>

#include <epicsEvent.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include <notifierConveyor.h>

#define TIMEOUT 5.0

using namespace epics::pvAccess::ca;

typedef void (*voidFunc)(void);

class basicClient :
    public NotifierClient
{
public:
    basicClient(voidFunc notify, voidFunc destroy = NULL) :
        notifyFunc(notify), destroyFunc(destroy) {}
    virtual ~basicClient() {
        if (destroyFunc) destroyFunc();
    }
    virtual void notifyClient() {
        if (notifyFunc) notifyFunc();
    }
private:
    voidFunc notifyFunc;
    voidFunc destroyFunc;
};

class owner {
public:
    owner(voidFunc destroy = NULL) :
        destroyFunc(destroy) {
        conveyor.start();
    }
    ~owner() {
        if (destroyFunc) destroyFunc();
    }
    void notifyClient(NotificationPtr const &notificationPtr)
    {
        conveyor.notifyClient(notificationPtr);
    }

private:
    NotifierConveyor conveyor;
    voidFunc destroyFunc;
};

epicsEvent ogone;
void odestroy(void) {
    testPass("Owner's destructor called");
    ogone.trigger();
}

epicsEvent noted;
void notify(void) {
    testPass("Client's notify() called");
    noted.trigger();
}
epicsEvent cgone;
void cdestroy(void) {
    testPass("Client's destructor called");
    cgone.trigger();
}

epicsEvent blocking, unblock;
void blockingNotify(void) {
    blocking.trigger();
    unblock.wait();
}

class requeingClient :
    public NotifierClient
{
public:
    requeingClient(owner *q, NotificationPtr n, int r) :
        queue(q), myself(n), requeues(r), count(0) {}
    virtual void notifyClient() {
        if (++count < requeues) {
            queue->notifyClient(myself);
        }
        else {
            testPass("Requeued myself %d times", count);
            done.trigger();
        }
    }
    bool wait(double timeout) {
        return done.wait(timeout);
    }
private:
    owner *queue;
    NotificationPtr myself;
    int requeues;
    int count;
    epicsEvent done;
};

void testOperation(void)
{
    testDiag("*** testOperation ***");
    {
        owner o1(&odestroy);
        NotifierClientPtr c1;
        NotificationPtr n1;

        testDiag("1. Basic queueing functionality.");
        c1 = NotifierClientPtr(new basicClient(&notify, &cdestroy));
        n1 = NotificationPtr(new Notification(c1));

        o1.notifyClient(n1);
        if (!noted.wait(TIMEOUT))
            testFail("Client's notify() not called");

        c1.reset();     // Clean up c1
        if (!cgone.wait(TIMEOUT))
            testFail("Client's destructor not called");

        testDiag("2. Queue accepts notification while conveyor is busy.");
        {
            c1 = NotifierClientPtr(new basicClient(&blockingNotify));
            n1->setClient(c1);
            o1.notifyClient(n1);
            if (!blocking.wait(TIMEOUT))
                testAbort("Conveyor not stalled");

            NotifierClientPtr c2 = NotifierClientPtr(new
                basicClient(&notify));
            NotificationPtr n2 = NotificationPtr(new Notification);
            n2->setClient(c2);
            o1.notifyClient(n2);
            testPass("Notification added to stalled queue");

            unblock.trigger();
            if (!noted.wait(TIMEOUT))
                testFail("Queue didn't recover from stall!");
        }

        testDiag("3. Client's notifier can re-queue itself.");
        {
            requeingClient *rq = new requeingClient(&o1, n1, 100);
            c1 = NotifierClientPtr(rq);
            n1->setClient(c1);
            o1.notifyClient(n1);
            if (!rq->wait(TIMEOUT))
                testFail("Requeueing client didn't finish?");
            c1.reset();
        }
    }
    if (!ogone.wait(TIMEOUT))
        testFail("Owner's destructor not called");
}

NotifierClientPtr cptr;
void dropClient(void) {
    testPass("dropClient() called");
    cptr.reset();
    noted.trigger();
}

void testDestruction(void)
{
    testDiag("*** testDestruction ***");

    std::tr1::shared_ptr<owner> optr;
    NotificationPtr n1;

    testDiag("4. Delete a client inside its own notifier.");
    {
        optr = std::tr1::shared_ptr<owner> (new owner(&odestroy));
        cptr = NotifierClientPtr(new basicClient(&dropClient, &cdestroy));
        n1 = NotificationPtr(new Notification(cptr));

        optr->notifyClient(n1);
        if (!noted.wait(TIMEOUT))
            testFail("dropClient() not called");
        // Client's destructor should also have been called
        if (!cgone.wait(TIMEOUT))
            testFail("Client's destructor not called");
    }

    NotifierClientPtr c1;

    testDiag("5. Queue Notification to an already-dead client.");
    {
        c1 = NotifierClientPtr(new basicClient(&notify, &cdestroy));
        n1->setClient(c1);

        c1.reset();     // Kill the client n1 pointed to
        if (!cgone.tryWait())
            testFail("Client's destructor not called");

        optr->notifyClient(n1);     // Should do nothing

        optr.reset();   // Clean up optr
        if (!ogone.tryWait())
            testFail("Owner's destructor not called");

        testOk(!noted.tryWait(), "Client's notify() NOT called");
    }
}

MAIN(testConveyor)
{
    testPlan(11);

    testOperation();
    testDestruction();

    return testDone();
}
