/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#ifndef RPCSERVICE_H
#define RPCSERVICE_H

#include <stdexcept>
#include <pv/sharedPtr.h>
#include <pv/pvAccess.h>
#include <pv/status.h>

namespace epics { namespace pvAccess { 

class RPCRequestException : public std::runtime_error {
public:
    
    RPCRequestException(epics::pvData::Status::StatusType status, epics::pvData::String const & message) :
       std::runtime_error(message), m_status(status)
    {
    }

    epics::pvData::Status::StatusType getStatus() const {
        return m_status;
    }
    
private:
    epics::pvData::Status::StatusType m_status;
};


class RPCService {
    public:
    POINTER_DEFINITIONS(RPCService);
    
    virtual epics::pvData::PVStructure::shared_pointer request(
        epics::pvData::PVStructure::shared_pointer const & args
    ) throw (RPCRequestException) = 0;
};

}}

#endif  /* RPCSERVICE_H */
