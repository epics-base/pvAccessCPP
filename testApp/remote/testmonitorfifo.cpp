/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <vector>

#include <pv/pvUnitTest.h>
#include <testMain.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/pvAccess.h>
#include <pv/current_function.h>

#if __cplusplus>=201103L

#include <functional>
#include <initializer_list>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

struct Tester {
    // we only have one thread, so no need for sync.
    enum cb_t {
        Connect,
        ConnectError,
        Event,
        Unlisten,
        Close,
        LowWater,
    };
    static const char* name(cb_t cb) {
        switch(cb) {
#define CASE(NAME) case NAME: return #NAME
        CASE(Connect);
        CASE(ConnectError);
        CASE(Event);
        CASE(Unlisten);
        CASE(Close);
        CASE(LowWater);
        default: return "???";
        }
    }

    typedef std::vector<cb_t> timeline_t;
    static timeline_t timeline;

    struct Requester : public pva::MonitorRequester {
        POINTER_DEFINITIONS(Requester);

        epicsMutex mutex; // not strictly needed, but a good simulation of usage

        virtual ~Requester() {}
        virtual std::string getRequesterName() OVERRIDE FINAL {return "Tester::Requester";}
        virtual void channelDisconnect(bool destroy) OVERRIDE FINAL {
            testDiag("In %s", CURRENT_FUNCTION);
            Guard G(mutex);
            Tester::timeline.push_back(Close);
        }
        virtual void monitorConnect(epics::pvData::Status const & status,
            pva::MonitorPtr const & monitor, epics::pvData::StructureConstPtr const & structure) OVERRIDE FINAL {
            testDiag("In %s : %s", CURRENT_FUNCTION, status.isSuccess() ? "OK" : status.getMessage().c_str());
            Guard G(mutex);
            if(status.isSuccess())
                Tester::timeline.push_back(Connect);
            else
                Tester::timeline.push_back(ConnectError);
        }
        virtual void monitorEvent(pva::MonitorPtr const & monitor) OVERRIDE FINAL {
            testDiag("In %s", CURRENT_FUNCTION);
            Guard G(mutex);
            Tester::timeline.push_back(Event);
        }
        virtual void unlisten(pva::MonitorPtr const & monitor) OVERRIDE FINAL {
            testDiag("In %s", CURRENT_FUNCTION);
            Guard G(mutex);
            Tester::timeline.push_back(Unlisten);
        }
    };
    Requester::shared_pointer requester;

    struct Handler : public pva::MonitorFIFO::Source {
        POINTER_DEFINITIONS(Handler);

        epicsMutex mutex; // not strictly needed, but a good simulation of usage

        std::function<void(pva::MonitorFIFO *mon, size_t)> action;

        virtual ~Handler() {}
        virtual void freeHighMark(pva::MonitorFIFO *mon, size_t numEmpty) OVERRIDE FINAL {
            testDiag("In %s", CURRENT_FUNCTION);
            Guard G(mutex);
            Tester::timeline.push_back(LowWater);
            if(action)
                action(mon, numEmpty);
        }
    };
    Handler::weak_pointer handler;

    void setAction(const std::function<void(pva::MonitorFIFO *mon, size_t)>& action) {
        Handler::shared_pointer H(handler);

        H->action = action;
    }

    pva::MonitorFIFO::shared_pointer mon;

    pvd::StructureConstPtr type;

    Tester(const pvd::PVStructure::const_shared_pointer& pvReq,
           pva::MonitorFIFO::Config *conf)
    {
        Handler::shared_pointer H(new Handler);
        handler = H;
        requester.reset(new Requester);
        mon.reset(new pva::MonitorFIFO(requester, pvReq, H, conf));
    }

    void reset() {
        timeline.clear();
    }

    void testTimeline(std::initializer_list<cb_t> l) {
        size_t i=0;
        for(auto event : l) {
            if(i >= timeline.size()) {
                testFail("timeline ends early.  Expected %s", name(event));
                continue;
            }
            testOk(event==timeline[i], "%s == %s", name(event), name(timeline[i]));

            i++;
        }
        for(; i<timeline.size(); i++) {
            testFail("timeline ends late.  Unexpected %s", name(timeline[i]));
        }
        if(i==0 && timeline.empty())
            testPass("timeline no events as expected");
        reset();
    }

    void connect(pvd::ScalarType t) {
        testDiag("connect() with %s", pvd::ScalarTypeFunc::name(t));
        type = pvd::getFieldCreate()->createFieldBuilder()
                    ->add("value", t)
                    ->createStructure();

        mon->open(type);
    }

    void close() {
        testDiag("close()");
        type.reset();
        mon->close();
    }

    template<typename T>
    void post(T val)
    {
        testShow()<<"post("<<val<<")";
        assert(!!type);
        pvd::PVStructurePtr V(pvd::getPVDataCreate()->createPVStructure(type));
        pvd::PVScalarPtr fld(V->getSubFieldT<pvd::PVScalar>("value"));
        fld->putFrom(val);
        pvd::BitSet changed;
        changed.set(fld->getFieldOffset());
        mon->post(*V, changed);
    }

    template<typename T>
    void tryPost(T val, bool expect, bool force = false)
    {
        assert(!!type);
        pvd::PVStructurePtr V(pvd::getPVDataCreate()->createPVStructure(type));
        pvd::PVScalarPtr fld(V->getSubFieldT<pvd::PVScalar>("value"));
        fld->putFrom(val);
        pvd::BitSet changed;
        changed.set(fld->getFieldOffset());
        testEqual(mon->tryPost(*V, changed, pvd::BitSet(), force), expect)<<" value="<<val;
    }
};

Tester::timeline_t Tester::timeline;

void testEmpty(pva::Monitor& mon)
{
    pva::MonitorElement::Ref elem(mon);
    testTrue(!elem)<<"Queue expected empty";
}

template<typename T>
void testPop(pva::Monitor& mon, T expected, bool overrun = false)
{
    pva::MonitorElement::Ref elem(mon);
    if(!elem) {
        testFail("Queue unexpected empty");
        return;
    }
    pvd::PVScalarPtr fld(elem->pvStructurePtr->getSubFieldT<pvd::PVScalar>("value"));
    T actual(fld->getAs<T>());
    bool changed = elem->changedBitSet->get(fld->getFieldOffset());
    bool overran = elem->overrunBitSet->get(fld->getFieldOffset());

    bool pop = actual==expected && changed && overran==overrun;
    testTrue(pop)
            <<" "<<actual<<" == "<<expected<<" changed="<<changed<<" "<<overran<<"=="<<overrun;
}

static const pvd::PVStructure::const_shared_pointer pvReqEmpty(pvd::getPVDataCreate()->createPVStructure(
                                                                   pvd::getFieldCreate()->createFieldBuilder()
                                                                     ->createStructure()));
pvd::PVStructure::const_shared_pointer
pvReqPipelineBuild() {
    pvd::PVStructure::shared_pointer ret(pvd::getPVDataCreate()->createPVStructure(
                                       pvd::getFieldCreate()->createFieldBuilder()
                                          ->addNestedStructure("record")
                                            ->addNestedStructure("_options")
                                                ->add("pipeline", pvd::pvBoolean)
                                                ->add("queueSize", pvd::pvUInt)
                                            ->endNested()
                                          ->endNested()
                                          ->createStructure()));
    ret->getSubFieldT<pvd::PVScalar>("record._options.pipeline")->putFrom<pvd::boolean>(true);
    ret->getSubFieldT<pvd::PVScalar>("record._options.queueSize")->putFrom<pvd::uint32>(2);
    return ret;
}

static const pvd::PVStructure::const_shared_pointer pvReqPipeline(pvReqPipelineBuild());

// Test the basic life cycle of a FIFO.
// post and pop single elements
void checkPlain()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    Tester tester(pvReqEmpty, 0);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.mon->start();
    tester.testTimeline({});

    testEmpty(*tester.mon);

    tester.post(5);
    tester.mon->notify();

    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 5);
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// close() while FIFO not empty.
void checkAfterClose()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    Tester tester(pvReqEmpty, 0);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.mon->start();
    tester.testTimeline({});

    testEmpty(*tester.mon);

    tester.post(5);
    tester.mon->stop();
    tester.close();
    tester.mon->notify();

    tester.testTimeline({Tester::Event, Tester::Close});

    // FIFO not cleared by close
    testPop(*tester.mon, 5);
    testEmpty(*tester.mon);

    tester.mon->notify();
    tester.testTimeline({});
}

// close() while FIFO not empty, then re-open
void checkReOpenLost()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    Tester tester(pvReqEmpty, 0);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.mon->start();
    tester.testTimeline({});

    testEmpty(*tester.mon);

    tester.post(5);
    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    // FIFO cleared by connect()
    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.mon->start();

    tester.testTimeline({Tester::Event, Tester::Close, Tester::Connect});

    // update 5 was lost!
    testEmpty(*tester.mon);

    tester.mon->notify();
    tester.testTimeline({});
}

// type change while FIFO is empty
void checkTypeChange()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    Tester tester(pvReqEmpty, 0);

    tester.connect(pvd::pvInt);
    tester.mon->start();
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.post(5);
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 5);
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});

    testDiag("Type change");

    tester.connect(pvd::pvString);
    tester.mon->start();
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.post(std::string("hello"));
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, std::string("hello"));
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// post() until the FIFO is full, and keep going.  Check overrun behavour
void checkFill()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=2;
    Tester tester(pvReqEmpty, &conf);

    testEqual(conf.actualCount, 2u);

    tester.connect(pvd::pvInt);
    tester.mon->notify();

    tester.testTimeline({Tester::Connect});

    tester.mon->start();

    testEqual(conf.actualCount, tester.mon->freeCount());

    testShow()<<"Empty before "<<*tester.mon;

    // fill up and overflow
    tester.post(5);
    testShow()<<"A5 "<<*tester.mon;
    tester.post(6);
    testShow()<<"Full "<<*tester.mon;
    tester.post(7);

    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testShow()<<"Overrun1 "<<*tester.mon;

    tester.post(8);
    tester.mon->notify();
    tester.testTimeline({});

    testShow()<<"Overrun2 "<<*tester.mon;

    testPop(*tester.mon, 5);
    tester.testTimeline({});
    testPop(*tester.mon, 6);
    tester.testTimeline({Tester::LowWater});
    testPop(*tester.mon, 8, true);
    tester.testTimeline({});
    testEmpty(*tester.mon);

    testShow()<<"Empty again "<<*tester.mon;

    // repeat
    tester.post(15);
    tester.post(16);
    tester.post(17);
    tester.post(18);
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 15);
    tester.testTimeline({});
    testPop(*tester.mon, 16);
    tester.testTimeline({Tester::LowWater});
    testPop(*tester.mon, 18, true);
    tester.testTimeline({});
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// post() until past full, then pop() and post() on a partially full queue
void checkSaturate()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=2;
    Tester tester(pvReqEmpty, &conf);

    testEqual(conf.actualCount, 2u);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.mon->start();

    testShow()<<"Empty before "<<*tester.mon;

    // fill up and overflow
    tester.post(5);
    tester.post(6);
    tester.post(7);
    tester.post(8);
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 5);
    tester.testTimeline({});
    tester.post(9);
    tester.mon->notify();
    tester.testTimeline({});

    testPop(*tester.mon, 6);
    tester.testTimeline({});
    tester.post(10);
    tester.mon->notify();
    tester.testTimeline({});

    testPop(*tester.mon, 8, true);
    tester.testTimeline({});
    tester.post(11);
    tester.mon->notify();
    tester.testTimeline({});

    testShow()<<"Overrun2 "<<*tester.mon;

    testPop(*tester.mon, 9);
    tester.testTimeline({});
    testPop(*tester.mon, 10);
    tester.testTimeline({Tester::LowWater});
    testPop(*tester.mon, 11);
    tester.testTimeline({});
    testEmpty(*tester.mon);
    tester.testTimeline({});

    testShow()<<"Empty again "<<*tester.mon;

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// check handling of pipeline=true
void checkPipeline()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=3;
    Tester tester(pvReqPipeline, &conf);

    testEqual(conf.actualCount, 2u);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    tester.mon->start();

    testEqual(tester.mon->freeCount(), 0u);
    tester.mon->reportRemoteQueueStatus(2);
    testEqual(tester.mon->freeCount(), 2u);
    tester.testTimeline({Tester::LowWater});

    // fill up and overflow
    tester.post(5);
    tester.post(6);
    tester.post(7);
    tester.post(8);
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testEqual(tester.mon->freeCount(), 0u);
    testPop(*tester.mon, 5);
    testPop(*tester.mon, 6);
    testEmpty(*tester.mon);

    testDiag("ACK 2");
    testShow()<<"Before ACK "<<*tester.mon;
    tester.mon->reportRemoteQueueStatus(2);
    testShow()<<"After ACK "<<*tester.mon;
    tester.testTimeline({Tester::LowWater});
    testEqual(tester.mon->freeCount(), 1u);

    // still have the overflow'd element on the queue

    testPop(*tester.mon, 8, true);
    testEmpty(*tester.mon);

    tester.mon->reportRemoteQueueStatus(1);
    testEqual(tester.mon->freeCount(), 2u);
    tester.testTimeline({});

    testShow()<<"Empty before re-fill "<<*tester.mon;

    // fill up and overflow
    tester.post(15);
    tester.post(16);
    tester.post(17);
    tester.post(18);
    tester.mon->notify();
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 15);
    testPop(*tester.mon, 16);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    testDiag("ACK 1");
    testShow()<<"Before ACK "<<*tester.mon;
    tester.mon->reportRemoteQueueStatus(1);
    testShow()<<"After ACK "<<*tester.mon;
    tester.testTimeline({});

    // 1 inuse, 1 returned, 1 empty

    tester.post(19);
    tester.post(20);

    testPop(*tester.mon, 18, true);
    testEmpty(*tester.mon);

    tester.post(21);

    testEmpty(*tester.mon);

    testShow()<<"Before final ACK "<<*tester.mon;

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater});

    testPop(*tester.mon, 21, true);
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// test the spam counter (keeps FIFO full) with pipeline=true
void checkSpam()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=3;
    Tester tester(pvReqPipeline, &conf);

    pvd::uint32 cnt = 0;

    tester.setAction([&tester, &cnt] (pva::MonitorFIFO *mon, size_t nfree) {
        testDiag("Spamming %zu", nfree);
        while(nfree--) {
            testDiag("nfree=%zu %c", nfree, (nfree>0)?'T':'F');
            tester.tryPost(cnt++, nfree>0);
        }
    });
    tester.mon->setFreeHighMark(0); // run action when all buffers free

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    testEqual(conf.actualCount, 2u);

    testDiag("prime the pump");
    tester.mon->reportRemoteQueueStatus(conf.actualCount);

    tester.mon->notify();
    tester.testTimeline({Tester::LowWater});

    testShow()<<"Before start() "<<*tester.mon<<"\n";
    tester.mon->start();
    testShow()<<"After start() "<<*tester.mon<<"\n";
    tester.testTimeline({Tester::Event});

    testPop(*tester.mon, 0);
    testPop(*tester.mon, 1);
    testEmpty(*tester.mon);

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 2);
    testPop(*tester.mon, 3);
    testEmpty(*tester.mon);

    tester.mon->reportRemoteQueueStatus(1);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 4);
    testEmpty(*tester.mon);

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 5);
    testPop(*tester.mon, 6);
    testEmpty(*tester.mon);

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

// check pipeline=true, watermark, and unlisten.
// a sequence with a definite end
void checkCountdown()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=3;
    Tester tester(pvReqPipeline, &conf);

    pvd::int32 cnt = 10;

    tester.setAction([&tester, &cnt] (pva::MonitorFIFO *mon, size_t nfree) {
        testDiag("Spamming %zu", nfree);
        while(cnt >= 0 && nfree--) {
            pvd::int32 c = cnt--;
            testDiag("Count %d nfree=%zu %c", c, nfree, (nfree>0)?'T':'F');
            tester.tryPost(c, nfree>0);
        }
        if(cnt<0) {
            testDiag("Finished!");
            tester.mon->finish();
        }
    });
    tester.mon->setFreeHighMark(0.5); // run action() when one of two buffer elements is free

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::Connect});

    testEqual(conf.actualCount, 2u);

    testDiag("prime the pump");
    tester.mon->reportRemoteQueueStatus(conf.actualCount);

    tester.mon->notify();
    tester.testTimeline({Tester::LowWater});

    tester.mon->start();
    tester.testTimeline({Tester::Event});


    testPop(*tester.mon, 10);
    testPop(*tester.mon, 9);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 8);
    testPop(*tester.mon, 7);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    tester.mon->reportRemoteQueueStatus(1);
    tester.testTimeline({}); // nothing happens, watermark not reached

    tester.mon->reportRemoteQueueStatus(1);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 6);
    testPop(*tester.mon, 5);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    tester.mon->reportRemoteQueueStatus(1);
    tester.testTimeline({});
    tester.mon->reportRemoteQueueStatus(1);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 4);
    testPop(*tester.mon, 3);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 2);
    testPop(*tester.mon, 1);
    testEmpty(*tester.mon);
    tester.testTimeline({});

    tester.mon->reportRemoteQueueStatus(2);
    tester.testTimeline({Tester::LowWater, Tester::Event});

    testPop(*tester.mon, 0);
    testEmpty(*tester.mon);
    tester.testTimeline({Tester::Unlisten});

    tester.mon->stop();
    tester.close();
    tester.mon->notify();
    tester.testTimeline({Tester::Close});
}

void checkBadRequest()
{
    testDiag("==== %s ====", CURRENT_FUNCTION);
    pva::MonitorFIFO::Config conf;
    conf.maxCount=4;
    conf.defCount=3;
    Tester tester(pvd::createRequest("field(invalid)"), &conf);

    tester.connect(pvd::pvInt);
    tester.mon->notify();
    tester.testTimeline({Tester::ConnectError});

    // when in Error, all are no-op
    tester.post(15);
    tester.tryPost(4, false);
    tester.tryPost(5, false, true);
    tester.mon->finish();

    tester.mon->notify();
    tester.testTimeline({}); // nothing happens

    tester.close();
    tester.testTimeline({});
}

} // namespace

MAIN(testmonitorfifo)
{
    testPlan(189);
    checkPlain();
    checkAfterClose();
    checkReOpenLost();
    checkTypeChange();
    checkFill();
    checkSaturate();
    checkPipeline();
    checkSpam();
    checkCountdown();
    checkBadRequest();
    return testDone();
}

#else /* c++11 */

MAIN(testmonitorfifo)
{
    testPlan(1);
    testSkip(1, "no c++11");
    return testDone();
}

#endif /* c++11 */
