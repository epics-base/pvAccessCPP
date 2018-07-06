/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdio.h>

#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <epicsEvent.h>

#include <pv/pvData.h>
#include <pv/serverContext.h>
#include <pva/server.h>
#include <pva/sharedstate.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

namespace {

epicsEvent done;

#ifdef USE_SIGNAL
void alldone(int num)
{
    (void)num;
    done.signal();
}
#endif

static pvd::StructureConstPtr string_type(pvd::getFieldCreate()->createFieldBuilder()
                                          ->add("value", pvd::pvString)
                                          ->createStructure());

static pvd::StructureConstPtr int_type(pvd::getFieldCreate()->createFieldBuilder()
                                          ->add("value", pvd::pvInt)
                                          ->createStructure());

static pvd::StructureConstPtr real_type(pvd::getFieldCreate()->createFieldBuilder()
                                          ->add("value", pvd::pvDouble)
                                          ->createStructure());

}//namespace

int main(int argc, char *argv[])
{
    try {
        if(argc<=1) {
            fprintf(stderr, "Usage: %s <pvname[=type]> ...\n  type: string, int, real", argv[0]);
            return 1;
        }

        // container for PVs
        pvas::StaticProvider provider("mailbox"); // provider name "mailbox" is arbitrary

        for(int i=1; i<argc; i++) {

            std::string name(argv[i]), type("string");

            size_t sep = name.find('=');
            if(sep != name.npos) {
                if(sep==0 || sep==name.size()) {
                    fprintf(stderr, "Invalid: '%s'\n", argv[i]);
                    return 1;
                }
                type = name.substr(sep+1);
                name = name.substr(0, sep-1);
            }

            pvas::SharedPV::shared_pointer pv(pvas::SharedPV::buildMailbox());

            // open() the PV, associates type
            if(type=="string") {
                pv->open(string_type);
            } else if(type=="int") {
                pv->open(int_type);
            } else if(type=="real") {
                pv->open(real_type);
            } else {
                fprintf(stderr, "Unknown type '%s'\n", type.c_str());
                return 1;
            }

            // add to container
            provider.add(argv[1], pv);
        }

        // create and run network server
        pva::ServerContext::shared_pointer server(pva::ServerContext::create(
                                                      pva::ServerContext::Config()
                                                      // use default config from environment
                                                      .provider(provider.provider())
                                                      ));

#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
        server->printInfo();

        printf("Running with mailbox '%s'\n", argv[1]);

        done.wait();

    } catch(std::exception& e){
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
    return 0;
}
