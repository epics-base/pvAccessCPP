/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef INC_caContext_H
#define INC_caContext_H

#include <cadef.h>
#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {
namespace ca {


class Attach;
class CAContext;
typedef std::tr1::shared_ptr<CAContext> CAContextPtr;

class CAContext
{
public:
    CAContext();
    ~CAContext();
private:
    ca_client_context* ca_context;

private:    // Internal API
    friend class Attach;
    ca_client_context* attach();
    void detach(ca_client_context* restore);
};

class Attach
{
public:
    explicit Attach(const CAContextPtr & to) :
        context(*to), saved_context(context.attach()) {}
    ~Attach() {
        context.detach(saved_context);
    }
private:
    CAContext & context;
    ca_client_context* saved_context;
};

}}}

#endif // INC_caContext_H
