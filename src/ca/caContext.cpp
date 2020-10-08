/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cadef.h>

#define epicsExportSharedSymbols
#include "caContext.h"

namespace epics {
namespace pvAccess {
namespace ca {

CAContext::CAContext()
{
    int result = ca_context_create(ca_enable_preemptive_callback);
    if (result != ECA_NORMAL)
        throw std::runtime_error("Can't create CA context");

    ca_context = ca_current_context();
}

ca_client_context* CAContext::attach()
{
    ca_client_context *thread_context = ca_current_context();
    if (thread_context != ca_context) {
        if (thread_context)
            ca_detach_context();

        int result = ca_attach_context(ca_context);
        if (result != ECA_NORMAL)
            throw std::runtime_error("Can't attach to CA context");
    }
    return thread_context;
}

void CAContext::detach(ca_client_context* restore) \
{
    ca_client_context *thread_context = ca_current_context();
    if (thread_context != ca_context)
        std::cerr << "CA context was changed!" << std::endl;

    ca_detach_context();

    if (restore) {
        int result = ca_attach_context(restore);
        if (result != ECA_NORMAL)
            std::cerr << "Can't re-attach to CA context" << std::endl;
    }
}

CAContext::~CAContext()
{
    ca_client_context *thread_context = attach();
    ca_context_destroy();
    if (thread_context != ca_context) {
        int result = ca_attach_context(ca_context);
        if (result != ECA_NORMAL)
            std::cerr << "Can't re-attach to CA context" << std::endl;
    }
}

}}}
