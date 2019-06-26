/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <algorithm>

#include <pv/byteBuffer.h>

#define epicsExportSharedSymbols
#include <pv/hexDump.h>

namespace epics {
namespace pvAccess {

HexDump::HexDump(const char* buf, size_t len)
    :buf(buf)
    ,buflen(len)
    ,_limit(1024u)
    ,_groupBy(4u)
    ,_perLine(16u)
{}

HexDump::HexDump(const pvData::ByteBuffer& bb,
                 size_t size, size_t offset)
    :buf(bb.getBuffer() + bb.getPosition())
    ,buflen(bb.getRemaining())
    ,_limit((size_t)-1)
    ,_groupBy(4u)
    ,_perLine(16u)
{
    if(offset > buflen)
        offset = buflen;
    buf    += offset;
    buflen -= offset;
    if(buflen > size)
        buflen = size;
}

HexDump::~HexDump() {}

static
size_t ilog2(size_t val)
{
    size_t ret = 0;
    while(val >>= 1)
        ret++;
    return ret;
}

static
size_t bits2bytes(size_t val)
{
    // round up to next multiple of 8
    val -= 1u;
    val |= 7u;
    val += 1u;
    // bits -> bytes
    val /= 8u;
    return val;
}

epicsShareFunc
std::ostream& operator<<(std::ostream& strm, const HexDump& hex)
{
    size_t len = std::min(hex.buflen, hex._limit);
    // find address width in hex chars
    // find bit width, rounded up to 8 bits, divide down to bytes
    size_t addrwidth = bits2bytes(ilog2(len))*2u;

    for(size_t i=0; i<len; i++)
    {
        unsigned val = hex.buf[i]&0xff;
        size_t col = i%hex._perLine;

        if(col==0) {
            // first address of this row
            strm<<"0x"<<std::hex<<std::setw(addrwidth)<<std::setfill('0')<<i;
        }
        if(col%hex._groupBy == 0) {
            strm<<' ';
        }
        strm<<std::hex<<std::setw(2)<<std::setfill('0')<<val;
        if(col+1u==hex._perLine && i+1u!=len)
            strm<<'\n';
    }
    if(len==hex._limit && len<hex.buflen)
        strm<<" ...";
    strm<<'\n';
    return strm;
}

}
}
