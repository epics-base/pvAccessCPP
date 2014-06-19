/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#ifndef RPCSERVICE_H
#define RPCSERVICE_H

#include <stdexcept>

#ifdef epicsExportSharedSymbols
#   define rpcServiceEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/sharedPtr.h>
#include <pv/status.h>

#ifdef rpcServiceEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef rpcServiceEpicsExportSharedSymbols
#endif

#include <pv/pvAccess.h>

#include <shareLib.h>

namespace epics { namespace pvAccess { 

class epicsShareClass RPCRequestException : public std::runtime_error {
public:
    
    RPCRequestException(epics::pvData::Status::StatusType status, std::string const & message) :
       std::runtime_error(message), m_status(status)
    {
    }

    epics::pvData::Status::StatusType getStatus() const {
        return m_status;
    }
    
private:
    epics::pvData::Status::StatusType m_status;
};


class epicsShareClass RPCService {
    public:
    POINTER_DEFINITIONS(RPCService);
   
    virtual ~RPCService() {};
 
    virtual epics::pvData::PVStructure::shared_pointer request(
        epics::pvData::PVStructure::shared_pointer const & args
    ) throw (RPCRequestException) = 0;
};

}}

#endif  /* RPCSERVICE_H */
