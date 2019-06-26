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
    const size_t len = std::min(hex.buflen, hex._limit);
    // find address width in hex chars
    // find bit width, rounded up to 8 bits, divide down to bytes
    const size_t addrwidth = bits2bytes(ilog2(len))*2u;
    size_t nlines = len/hex._perLine;

    if(len%hex._perLine)
        nlines++;

    for(size_t l=0; l<nlines; l++)
    {
        size_t start = l*hex._perLine;
        strm<<"0x"<<std::hex<<std::setw(addrwidth)<<std::setfill('0')<<start;

        // print hex chars
        for(size_t col=0; col<hex._perLine; col++)
        {
            if(col%hex._groupBy == 0) {
                strm<<' ';
            }
            if(start+col < len) {
                strm<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(hex.buf[start+col]&0xff);
            } else {
                strm<<"  ";
            }
        }

        strm<<' ';

        // printable ascii
        for(size_t col=0; col<hex._perLine && start+col<len; col++)
        {
            if(col%hex._groupBy == 0) {
                strm<<' ';
            }
            char val = hex.buf[start+col]&0xff;
            if(val>=' ' && val<='~') {
                strm<<val;
            } else {
                strm<<'.';
            }
        }

        strm<<'\n';
    }

    return strm;
}

}
}
