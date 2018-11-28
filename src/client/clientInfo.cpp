/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsEvent.h>
#include <epicsThread.h>

#include <pv/current_function.h>
#include <pv/pvData.h>
#include <pv/bitSet.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "clientpvt.h"
#include "pv/pvAccess.h"

namespace {
using pvac::detail::CallbackGuard;
using pvac::detail::CallbackUse;

struct Infoer : public pvac::detail::CallbackStorage,
                public pva::GetFieldRequester,
                public pvac::Operation::Impl,
                public pvac::detail::wrapped_shared_from_this<Infoer>
{
    pvac::ClientChannel::InfoCallback *cb;
    const pva::Channel::shared_pointer channel;

    static size_t num_instances;

    explicit Infoer(pvac::ClientChannel::InfoCallback *cb, const pva::Channel::shared_pointer& channel)
        :cb(cb), channel(channel)
    {REFTRACE_INCREMENT(num_instances);}
    virtual ~Infoer() {
        CallbackGuard G(*this);
        cb = 0;
        G.wait(); // paranoia
        REFTRACE_DECREMENT(num_instances);
    }


    virtual std::string getRequesterName() OVERRIDE FINAL
    {
        Guard G(mutex);
        return channel->getChannelName();
    }

    virtual void getDone(
        const pvd::Status& status,
        pvd::FieldConstPtr const & field) OVERRIDE FINAL
    {
        CallbackGuard G(*this);
        pvac::ClientChannel::InfoCallback *C(cb);
        cb = 0;
        if(C) {
            pvac::InfoEvent evt;
            evt.event = status.isSuccess() ? pvac::InfoEvent::Success : pvac::InfoEvent::Fail;
            evt.message = status.getMessage();
            evt.type = field;
            CallbackUse U(G);
            C->infoDone(evt);
        }
        pvac::InfoEvent evt;
    }

    virtual std::string name() const OVERRIDE FINAL { return channel->getChannelName(); }

    virtual void cancel() OVERRIDE FINAL {
        CallbackGuard G(*this);
        // we can't actually cancel a getField
        pvac::ClientChannel::InfoCallback *C(cb);
        cb = 0;
        if(C) {
            pvac::InfoEvent evt;
            evt.event = pvac::InfoEvent::Cancel;
            CallbackUse U(G);
            C->infoDone(evt);
        }
        G.wait();
    }

    virtual void show(std::ostream& strm) const OVERRIDE FINAL {
        strm << "Operation(Info"
                "\"" << name() <<"\""
             ")";
    }
};

size_t Infoer::num_instances;

} // namespace

namespace pvac {

Operation ClientChannel::info(InfoCallback *cb, const std::string& subfld)
{
    if(!impl) throw std::logic_error("Dead Channel");

    std::tr1::shared_ptr<Infoer> ret(Infoer::build(cb, getChannel()));

    {
        Guard G(ret->mutex);
        getChannel()->getField(ret, subfld);
        // getField is an oddity as it doesn't have an associated Operation class,
        // and is thus largely out of our control.  (eg. can't cancel)
    }

    return Operation(ret);
}

namespace detail {

void registerRefTrackInfo()
{
    epics::registerRefCounter("pvac::Infoer", &Infoer::num_instances);
}

}

} // namespace pvac
