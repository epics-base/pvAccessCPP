
#include <map>
#include <memory>

#include <epicsGuard.h>
#include <fdManager.h>

#include <pv/thread.h>

#include "udprepeat.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {
static epicsMutex repeatlistlock;
typedef std::map<unsigned short, UDPFanout::weak_pointer> repeaters_t;
static repeaters_t repeaters;

struct Receiver : public pvd::Runnable
{
    const SOCKET sock;
    pva::UDPFanout::Pvt * const pvt;
    std::vector<char> buf;
    std::vector<std::tr1::shared_ptr<pva::UDPReceiver> > recvs;
    bool run;
    pvd::Thread runner;
    epicsEvent evt;

    Receiver(SOCKET sock, const std::string& name, pva::UDPFanout::Pvt *pvt)
        :sock(sock), pvt(pvt), buf(8196), run(true)
        ,runner(pvd::Thread::Config(this)
                .name(name)
                .prio(epicsThreadPriorityCAServerLow-2))
    {
        evt.wait();
    }
    ~Receiver()
    {
        epicsSocketDestroy(sock);
    }
    virtual void run()
    {
        evt.signal();
        Guard G(pvt->lock);
        while(run) {
            ssize_t ret;
            {
                UnGuard U(G);
                ret = ::recv(sock, &buf[0], buf.size(), 0);
            }
            if(ret<0) {
                int err = errno;

            }
        }
    }
    void shutdown();
};

}

namespace epics {namespace pvAccess {

struct UDPFanout::Pvt
{
    const unsigned port;
    epicsMutex lock;

    UDPFanout::addr_list iface, bcase;
    UDPFanout::name_list names;

    Pvt(unsigned port)
        :port(port)
    {

    }
};

}} // namespace

namespace {

void Receiver::shutdown()
{
    {
        Guard G(pvt->lock);
        run = false;
    }
    switch(epicsSocketSystemCallInterruptMechanismQuery())
    {
    case esscimqi_socketBothShutdownRequired:
        ::shutdown(sock, SHUT_RDWR);
        break;
    default:
        break;
    }
    runner.exitWait();
}

}

namespace epics {namespace pvAccess {


UDPFanout::~UDPFanout()
{
    std::auto_ptr<UDPFanout::Pvt> pvt(this->pvt);

    epicsGuard<epicsMutex> G(repeatlistlock);

    repeaters_t::iterator it = repeaters.find(pvt->port);
    if(it!=repeaters.end()) {
        UDPFanout::shared_pointer self(it->second.lock());
        if(self && self.get()==this)
            repeaters.erase(it);
    }
}

void
UDPFanout::bind(const UDPReceiver::shared_pointer, const std::string& iname)
{

}

void
UDPFanout::bind(const UDPReceiver::shared_pointer, const osiSockAddr& iface)
{

}

void
UDPFanout::unbind(const UDPReceiver::shared_pointer)
{

}

const UDPFanout::name_list&
UDPFanout::getNames()
{

}

const UDPFanout::addr_list&
UDPFanout::getAddresses()
{

}

UDPFanout::shared_pointer
UDPFanout::getFanoutPort(unsigned port)
{
    epicsGuard<epicsMutex> G(repeatlistlock);

    repeaters_t::const_iterator it = repeaters.find(port);
    if(it!=repeaters.end()) {
        UDPFanout::shared_pointer R(it->second.lock());
        if(R)
            return R;
    }

    std::auto_ptr<UDPFanout::Pvt> pvt(new UDPFanout::Pvt(port));
    UDPFanout::shared_pointer ret(new UDPFanout(pvt.get()));
    repeaters[port] = ret;
    pvt.release();
    return ret;
}

}} // namespace
