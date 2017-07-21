/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
// The simplest possible PVA monitor

#include <iostream>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsEvent.h>

#include <pva/client.h>

static volatile bool done;

#ifdef USE_SIGNAL
static pvac::MonitorSync * volatile subscription;
static
void handler(int num)
{
    (void)num;
    done = true;
    pvac::MonitorSync *mon = subscription;
    if(mon)
        mon->wake();
}
#endif

int main(int argc, char *argv[])
{
    try {
        if(argc<=1) {
            std::cerr<<"Usage: "<<argv[0]<<" <pvname>\n";
            return 1;
        }

        pvac::ClientProvider provider("pva");

        pvac::ClientChannel channel(provider.connect(argv[1]));

        pvac::MonitorSync mon(channel.monitor());

#ifdef USE_SIGNAL
        subscription = &mon;
        signal(SIGINT, handler);
        signal(SIGTERM, handler);
        signal(SIGQUIT, handler);
#endif

        int ret = 0;

        while(!done) {
            if(!mon.wait()) // updates mon.event
                continue;

            switch(mon.event.event) {
            // Subscription network/internal error
            case pvac::MonitorEvent::Fail:
                std::cerr<<mon.name()<<" : Error : "<<mon.event.message<<"\n";
                ret = 1;
                done = true;
                break;
            // explicit call of 'mon.cancel' or subscription dropped
            case pvac::MonitorEvent::Cancel:
                std::cout<<mon.name()<<" <Cancel>\n";
                done = true;
                break;
            // Underlying channel becomes disconnected
            case pvac::MonitorEvent::Disconnect:
                std::cout<<mon.name()<<" <Disconnect>\n";
                break;
            // Data queue becomes not-empty
            case pvac::MonitorEvent::Data:
                // We drain event FIFO completely
                while(mon.poll()) {
                    std::cout<<mon.name()<<" : "<<mon.root;
                }
                // check to see if more events might be sent
                if(mon.complete()) {
                    done = true;
                    std::cout<<mon.name()<<" : <Complete>\n";
                }
                break;
            }
        }

#ifdef USE_SIGNAL
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        subscription = 0;
#endif

        return ret;
    }catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}
