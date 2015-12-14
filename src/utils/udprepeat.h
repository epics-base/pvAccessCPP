#ifndef UDPREPEAT_H
#define UDPREPEAT_H

#include <vector>
#include <string>

#include <osiSock.h>

#include "byteBuffer.h"
#include "sharedPtr.h"

#define epicsExportSharedSymbols

namespace epics {namespace pvAccess {

class UDPReceiver
{
    POINTER_DEFINITIONS(UDPReceiver);

    virtual ~UDPReceiver() {}

    virtual void recv(const osiSockAddr& src,
                      const char *buf, size_t buflen) =0;
};

class UDPFanout
{
    struct Pvt;
    friend struct Pvt;
    Pvt *pvt;
    UDPFanout(Pvt *pvt) :pvt(pvt) {}
    UDPFanout(const UDPFanout&);
    UDPFanout& operator=(const UDPFanout&);
public:
    POINTER_DEFINITIONS(UDPFanout);

    ~UDPFanout();

    void bind(const UDPReceiver::shared_pointer, const std::string& iname);
    void bind(const UDPReceiver::shared_pointer, const osiSockAddr& iface);
    void unbind(const UDPReceiver::shared_pointer);

    typedef std::vector<std::string> name_list;
    typedef std::vector<osiSockAddr> addr_list;

    const name_list& getNames();
    const addr_list& getAddresses();

    static UDPFanout::shared_pointer getFanoutPort(unsigned port);
};

}}

#endif // UDPREPEAT_H
