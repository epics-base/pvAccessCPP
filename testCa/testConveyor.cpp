// testConveyor.cpp
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

/*
 * Tests for notifierConveyor:
 *    1. Queueing a notification calls the client.
 *    2. Notifications can be added while notifyClient() is asleep/busy.
 *    3. A client can re-queue itself from notifyClient().
 *    4. Queue a Notification to an empty or already-dead client
 *         -- unreference after adding it to the Notification
 *    5. Notifying can delete an almost-dead client
 *         -- unreference the client in the notifyClient() routine.
 *    6. Deleting the conveyor in a notifier doesn't crash it.
 */

#include <cstddef>

#include <epicsEvent.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include <notifierConveyor.h>

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

namespace basic {
    epicsEvent ogone;
    void odestroy(void) {
        testPass("Owner's destructor called");
        ogone.trigger();
    }

    epicsEvent noted, gone;
    void notify(void) {
        testPass("Client's notify() called");
        noted.trigger();
    }
    void cdestroy(void) {
        testPass("Client's destructor called");
        gone.trigger();
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

            testDiag("1. Basic queueing functionality");
            {
                NotifierClientPtr c1 = NotifierClientPtr(new
                    basicClient(&notify, &cdestroy));
                NotificationPtr n1 = NotificationPtr(new Notification);
                n1->setClient(c1);

                o1.notifyClient(n1);
                if (!noted.wait(5.0))
                    testFail("Client's notify() not called");

                // c1's destructor should be called here
            }
            if (!gone.tryWait())
                testFail("Client's destructor not called");

            testDiag("2. Queue accepts notification while conveyor is busy");
            {
                NotifierClientPtr c2 = NotifierClientPtr(new
                    basicClient(&blockingNotify));
                NotificationPtr n2 = NotificationPtr(new Notification);
                n2->setClient(c2);
                o1.notifyClient(n2);
                if (!blocking.wait(5.0))
                    testAbort("Conveyor not stalled");

                NotifierClientPtr c3 = NotifierClientPtr(new
                    basicClient(&notify));
                NotificationPtr n3 = NotificationPtr(new Notification);
                n3->setClient(c3);
                o1.notifyClient(n3);
                testPass("Notification added to stalled queue");

                unblock.trigger();
                if (!noted.wait(5.0))
                    testFail("Queue didn't recover from stall!");
            }

            testDiag("3. Client can re-queue itself from notifyClient()");
            {
                NotificationPtr n4 = NotificationPtr(new Notification);
                requeingClient *rq = new requeingClient(&o1, n4, 100);
                NotifierClientPtr c4 = NotifierClientPtr(rq);
                n4->setClient(c4);
                o1.notifyClient(n4);
                if (!rq->wait(5.0))
                    testFail("Requeueing client didn't finish?");
            }

            testDiag("4. Queue Notification to an already-dead client");
            {
                NotifierClientPtr c5 = NotifierClientPtr(new
                    basicClient(&notify, &cdestroy));
                NotificationPtr n1 = NotificationPtr(new Notification);
                n1->setClient(c5);

                c5.reset();
                if (!gone.tryWait())
                    testFail("Client's destructor not called");

                o1.notifyClient(n1);
                testOk(!noted.wait(2.0), "Client's notify() not called");
            }

            // o1's destructor gets called here
        }
        if (!ogone.wait(5.0))
            testFail("Owner's destructor not called");
    }
}

namespace torture {
    static epicsEvent ogone;
    void odestroy(void) {
        testPass("Owner's destructor called");
        ogone.trigger();
    }

    static epicsEvent noted, cgone;
    void notify(void) {
        testPass("Client's notify() called");
        noted.trigger();
    }
    void cdestroy(void) {
        testPass("Client's destructor called");
        cgone.trigger();
    }

    NotifierClientPtr cptr;
    void dropClient(void) {
        testPass("dropClient() called");
        cptr.reset();
        noted.trigger();
    }

    std::shared_ptr<owner> optr;
    void mayhem(void) {
        testPass("mayhem() called");
        optr.reset();
        noted.trigger();
    }

    void testDestruction(void)
    {
        testDiag("*** testDestruction ***");

        testDiag("5. Notifying can delete an almost-dead client");
        {
            owner o1(&odestroy);
            {
                cptr = NotifierClientPtr(new
                    basicClient(&dropClient, &cdestroy));
                NotificationPtr n1 = NotificationPtr(new Notification);
                n1->setClient(cptr);

                o1.notifyClient(n1);
                if (!noted.wait(5.0))
                    testFail("Client's not notify() called");
                // c1's destructor should also have been called
                if (!cgone.wait(5.0))
                    testFail("Client's destructor not called");

            }

            // o1's destructor gets called here
        }
        if (!ogone.wait(5.0))
            testFail("Owner's destructor not called");

        testDiag("6. Deleting the conveyor inside a notifier doesn't crash it.");
        {
            optr = std::shared_ptr<owner> (new owner(&odestroy));
            {
                NotifierClientPtr c2 = NotifierClientPtr(new
                    basicClient(&mayhem, &cdestroy));
                NotificationPtr n2 = NotificationPtr(new Notification);
                n2->setClient(c2);

                // optr's destructor called by conveyor here:
                optr->notifyClient(n2);
                if (!noted.wait(5.0))
                    testFail("mayhem() not called");
                if (!ogone.tryWait())
                    testFail("Owner's destructor not called");

                // c2's destructor gets called here
            }
            if (!cgone.wait(5.0))
                testFail("Client's destructor not called");
        }
    }
}

MAIN(testConveyor)
{
    testPlan(14);

    basic::testOperation();
    torture::testDestruction();

    return testDone();
}
