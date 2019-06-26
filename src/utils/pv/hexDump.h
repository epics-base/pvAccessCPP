/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef HEXDUMP_H_
#define HEXDUMP_H_

#include <ostream>

#ifdef epicsExportSharedSymbols
#   define hexDumpEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvType.h>

#ifdef	hexDumpEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   undef hexDumpEpicsExportSharedSymbols
#endif

#include <shareLib.h>

namespace epics {
namespace pvData {
class ByteBuffer;
}
namespace pvAccess {

class epicsShareClass HexDump {
    const char* buf;
    size_t buflen;
    size_t _limit;
    unsigned _groupBy;
    unsigned _perLine;
public:
    HexDump(const char* buf, size_t len);
    explicit HexDump(const pvData::ByteBuffer& buf, size_t size=(size_t)-1, size_t offset=0u);
    ~HexDump();

    //! safety limit on max bytes printed
    inline HexDump& limit(size_t n=(size_t)-1)        { _limit = n; return *this; }
    //! insert a space after this many bytes
    inline HexDump& bytesPerGroup(size_t n=(size_t)-1) { _groupBy = n; return *this; }
    //! start a new line after this many bytes
    inline HexDump& bytesPerLine(size_t n=(size_t)-1) { _perLine = n; return *this; }

    epicsShareFunc
    friend std::ostream& operator<<(std::ostream& strm, const HexDump& hex);
};

epicsShareFunc
std::ostream& operator<<(std::ostream& strm, const HexDump& hex);

}
}

#endif /* HEXDUMP_H_ */
