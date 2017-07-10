/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/rpcService.h>

namespace pvd = epics::pvData;

namespace epics{namespace pvAccess{

void RPCService::request(
        pvd::PVStructure::shared_pointer const & args,
        RPCResponseCallback::shared_pointer const & callback)
{
    assert(callback && args);
    pvd::PVStructure::shared_pointer ret;
    pvd::Status sts;
    try {
        ret = request(args);
    }catch(RPCRequestException& e){
        sts = e.asStatus();
        throw;
    }catch(std::exception& e){
        sts = pvd::Status::error(e.what());
    }
    if(!ret) {
        sts = pvd::Status(pvd::Status::STATUSTYPE_FATAL, "RPCService.request(PVStructure) returned null.");
    }
    callback->requestDone(sts, ret);
}

}} // namespace epics::pvAccess
