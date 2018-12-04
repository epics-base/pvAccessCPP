#ifndef SECURITYIMPL_H
#define SECURITYIMPL_H

#include <pv/remote.h>

#include "security.h"

namespace epics {
namespace pvAccess {


class AuthNZHandler :
    public ResponseHandler
{
    EPICS_NOT_COPYABLE(AuthNZHandler)
public:
    AuthNZHandler(Context* context) :
        ResponseHandler(context, "authNZ message")
    {
    }

    virtual ~AuthNZHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport,
                                epics::pvData::int8 version,
                                epics::pvData::int8 command,
                                size_t payloadSize,
                                epics::pvData::ByteBuffer* payloadBuffer);
};

}}

#endif // SECURITYIMPL_H
