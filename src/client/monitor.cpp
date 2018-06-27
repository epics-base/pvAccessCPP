/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <sstream>
#include <stdexcept>

#include <epicsGuard.h>
#include <epicsMath.h>

#define epicsExportSharedSymbols
#include <pv/monitor.h>
#include <pv/pvAccess.h>
#include <pv/reftrack.h>

namespace pvd = epics::pvData;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace epics {namespace pvAccess {

static const MonitorFIFO::Config default_conf = {4, 4, 0};

size_t MonitorFIFO::num_instances;

MonitorFIFO::Source::~Source() {}

MonitorFIFO::MonitorFIFO(const std::tr1::shared_ptr<MonitorRequester> &requester,
                         const pvData::PVStructure::const_shared_pointer &pvRequest,
                         const Source::shared_pointer &source, Config *inconf)
    :conf(inconf ? *inconf : default_conf)
    ,requester(requester)
    ,upstream(source)
    ,pipeline(false)
    ,opened(false)
    ,running(false)
    ,finished(false)
    ,needConnected(false)
    ,needEvent(false)
    ,needUnlisten(false)
    ,needClosed(false)
    ,freeHighLevel(0u)
    ,flowCount(0)
{
    REFTRACE_INCREMENT(num_instances);

    if(conf.maxCount==0)
        conf.maxCount = 1;

    if(conf.defCount==0)
        conf.defCount = 1;

    pvd::PVScalar::const_shared_pointer O(pvRequest->getSubField<pvd::PVScalar>("record._options.queueSize"));
    if(O && conf.actualCount==0) {
        try {
            conf.actualCount = O->getAs<pvd::uint32>();
        } catch(std::exception& e) {
            std::ostringstream strm;
            strm<<"invalid queueSize : "<<e.what();
            requester->message(strm.str());
        }
    }

    if(conf.actualCount==0)
        conf.actualCount = conf.defCount;

    if(conf.actualCount > conf.maxCount)
        conf.actualCount = conf.maxCount;

    O = pvRequest->getSubField<pvd::PVScalar>("record._options.pipeline");
    if(O) {
        try {
            pipeline = O->getAs<pvd::boolean>();
        } catch(std::exception& e) {
            std::ostringstream strm;
            strm<<"invalid pipeline : "<<e.what();
            requester->message(strm.str());
        }
    }

    setFreeHighMark(0.00);

    if(inconf)
        *inconf = conf;
}

MonitorFIFO::~MonitorFIFO() {
    REFTRACE_DECREMENT(num_instances);
}

void MonitorFIFO::destroy()
{}

void MonitorFIFO::show(std::ostream& strm) const
{
    // const (after ctor) bits
    strm<<"MonitorFIFO"
          " pipeline="<<pipeline
        <<" size="<<conf.actualCount
        <<" freeHighLevel="<<freeHighLevel
        <<"\n";

    Guard G(mutex);

    strm<<"  open="<<opened<<" running="<<running<<" finished="<<finished<<"\n";
    strm<<"  #empty="<<empty.size()<<" #returned="<<returned.size()<<" #inuse="<<inuse.size()<<" flowCount="<<flowCount<<"\n";
    strm<<"  events "<<(needConnected?'C':'_')<<(needEvent?'E':'_')<<(needUnlisten?'U':'_')<<(needClosed?'X':'_')
        <<"\n";
}

void MonitorFIFO::setFreeHighMark(double level)
{
    level = std::max(0.0, std::min(level, 1.0));
    pvd::uint32 lvl = std::max(size_t(0), std::min(size_t(conf.actualCount * level), conf.actualCount-1));

    Guard G(mutex);

    freeHighLevel = lvl;
}

void MonitorFIFO::open(const pvd::StructureConstPtr& type)
{
    Guard G(mutex);

    if(opened)
        throw std::logic_error("Monitor already open.  Must close() before re-openning");
    else if(needClosed)
        throw std::logic_error("Monitor needs notify() between close() and open().");
    else if(finished)
        throw std::logic_error("Monitor finished.  re-open() not possible");

    // keep the code simpler.
    // never try to re-use elements, even on re-open w/o type change.
    empty.clear();
    inuse.clear();
    returned.clear();

    // fill up empty.
    pvd::PVDataCreatePtr create(pvd::getPVDataCreate());
    while(empty.size() < conf.actualCount+1) {
        MonitorElementPtr elem(new MonitorElement(create->createPVStructure(type)));
        empty.push_back(elem);
    }

    opened = true;
    needConnected = true;

    assert(inuse.empty());
    assert(empty.size()>=2);
    assert(returned.empty());
    assert(conf.actualCount>=1);
}

void MonitorFIFO::close()
{
    Guard G(mutex);
    if(!opened)
        return; // no-op

    opened = false;
    needClosed = true;
}

void MonitorFIFO::finish()
{
    Guard G(mutex);
    if(!opened)
            throw std::logic_error("Can not finish() a closed Monitor");
    else if(finished)
        return; // no-op

    finished = true;
    if(inuse.empty() && running)
        needUnlisten = true;
}

bool MonitorFIFO::tryPost(const pvData::PVStructure& value,
                          const pvd::BitSet& changed,
                          const pvd::BitSet& overrun,
                          bool force)
{
    Guard G(mutex);

    assert(opened && !finished);
    assert(!empty.empty() || !inuse.empty());

    const bool havefree = _freeCount()>0u;

    MonitorElementPtr elem;
    if(havefree) {
        // take an empty element
        elem = empty.front();
        empty.pop_front();
    } else if(force) {
        // allocate an extra element
        elem.reset(new MonitorElement(pvd::getPVDataCreate()->createPVStructure(inuse.back()->pvStructurePtr->getStructure())));
    }

    if(elem) {
        try {
            assert(value.getStructure() == elem->pvStructurePtr->getStructure());
            elem->pvStructurePtr->copyUnchecked(value, changed);
            *elem->changedBitSet = changed;
            *elem->overrunBitSet = overrun;

            if(inuse.empty() && running)
                needEvent = true;
            inuse.push_back(elem);
        }catch(...){
            if(havefree) {
                empty.push_front(elem);
            }
            throw;
        }
        if(pipeline)
            flowCount--;
    }

    return _freeCount()>0u;
}


void MonitorFIFO::post(const pvData::PVStructure& value,
                       const pvd::BitSet& changed,
                       const pvd::BitSet& overrun)
{
    Guard G(mutex);

    assert(opened && !finished);
    assert(!empty.empty() || !inuse.empty());

    const bool use_empty = !empty.empty();

    MonitorElementPtr elem;

    if(use_empty) {
        // space in window, or entering overflow, fill an empty element

        assert(!empty.empty());

        elem = empty.front();

    } else {
        // window full and already in overflow
        // squash with last element
        assert(!inuse.empty());
        elem = inuse.back();
    }

    assert(value.getStructure() == elem->pvStructurePtr->getStructure());
    elem->pvStructurePtr->copyUnchecked(value, changed);

    if(use_empty) {
        *elem->changedBitSet = changed;
        *elem->overrunBitSet = overrun;

        if(inuse.empty() && running)
            needEvent = true;

        inuse.push_back(elem);
        empty.pop_front();
        if(pipeline)
            flowCount--;

    } else {
        // in overflow
        // squash
        elem->overrunBitSet->or_and(*elem->changedBitSet, changed);
        *elem->overrunBitSet |= overrun;
        *elem->changedBitSet |= changed;

        // leave as inuse.back()
    }
}

void MonitorFIFO::notify()
{
    Monitor::shared_pointer self;
    MonitorRequester::shared_pointer req;
    pvd::StructureConstPtr type;
    bool conn = false,
         evt = false,
         unl = false,
         clo = false;

    {
        Guard G(mutex);

        std::swap(conn, needConnected);
        std::swap(evt, needEvent);
        std::swap(unl, needUnlisten);
        std::swap(clo, needClosed);

        if(conn | evt | unl | clo) {
            req = requester.lock();
            self = shared_from_this();
        }
        if(conn)
            type = (!inuse.empty() ? inuse.front() : empty.back())->pvStructurePtr->getStructure();
    }

    if(!req)
        return;
    if(conn)
        req->monitorConnect(pvd::Status(), self, type);
    if(evt)
        req->monitorEvent(self);
    if(unl)
        req->unlisten(self);
    if(clo)
        req->channelDisconnect(false);
}

pvd::Status MonitorFIFO::start()
{
    Monitor::shared_pointer self;
    MonitorRequester::shared_pointer req;

    {
        Guard G(mutex);

        if(!opened)
            throw std::logic_error("Monitor can't start() before open()");

        if(running)
            return pvd::Status();

        if(!inuse.empty()) {
            self = shared_from_this();
            req = requester.lock();
        }

        running = true;
    }

    if(req)
        req->monitorEvent(self);

    return pvd::Status();
}

pvd::Status MonitorFIFO::stop()
{
    Guard G(mutex);

    running = false;

    return pvd::Status();
}

MonitorElementPtr MonitorFIFO::poll()
{
    MonitorElementPtr ret;
    Monitor::shared_pointer self;
    MonitorRequester::shared_pointer req;

    {
        Guard G(mutex);

        if(!inuse.empty() && inuse.size() + empty.size() > 1) {
            ret = inuse.front();
            inuse.pop_front();
            if(inuse.empty() && finished) {
                self = shared_from_this();
                req = requester.lock();
            }
        }

        assert(!inuse.empty() || !empty.empty());
    }

    if(req) {
        req->unlisten(self);
    }

    return ret;
}

void MonitorFIFO::release(MonitorElementPtr const & elem)
{
    size_t nempty;
    {
        Guard G(mutex);

        assert(!inuse.empty() || !empty.empty());

        const pvd::StructureConstPtr& type((!inuse.empty() ? inuse.front() : empty.back())->pvStructurePtr->getStructure());

        if(elem->pvStructurePtr->getStructure() != type // return of old type
                || empty.size()+returned.size()>=conf.actualCount+1) // return of force'd
            return; // ignore it

        if(pipeline) {
            // work done during reportRemoteQueueStatus()
            returned.push_back(elem);
            return;
        }

        bool below = _freeCount() <= freeHighLevel;

        empty.push_front(elem);

        bool above = _freeCount() > freeHighLevel;

        if(!below || !above || !upstream)
            return;

        nempty = _freeCount();
    }

    upstream->freeHighMark(this, nempty);
    notify();
}

void MonitorFIFO::getStats(Stats& s) const
{
    Guard G(mutex);
    s.nempty = empty.size() + returned.size();
    s.nfilled = inuse.size();
    s.noutstanding = conf.actualCount - s.nempty - s.nfilled;
}

void MonitorFIFO::reportRemoteQueueStatus(pvd::int32 nfree)
{
    if(nfree<=0 || !pipeline)
        return; // paranoia

    size_t nempty;
    {
        Guard G(mutex);

        bool below = _freeCount() <= freeHighLevel;

        size_t nack = std::min(size_t(nfree), returned.size());
        flowCount += nfree;

        buffer_t::iterator end(returned.begin());
        std::advance(end, nack);

        // remove[0, nack) from returned and append to empty
        empty.splice(empty.end(), returned, returned.begin(), end);

        bool above = _freeCount() > freeHighLevel;

        if(!below || !above || empty.size()<=1 || !upstream)
            return;

        nempty = _freeCount();
    }

    upstream->freeHighMark(this, nempty);
    notify();
}

size_t MonitorFIFO::freeCount() const
{
    Guard G(mutex);
    return _freeCount();
}

// caller must hold lock
size_t MonitorFIFO::_freeCount() const
{
    if(pipeline) {
        return std::max(0, std::min(flowCount, epicsInt32(empty.size())));
    } else {
        return empty.empty() ? 0 : empty.size()-1;
    }
}

}} // namespace epics::pvAccess
