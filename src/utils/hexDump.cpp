/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <sstream>
#include <string>

#define epicsExportSharedSymbols
#include <pv/hexDump.h>

using namespace epics::pvData;
using std::string;
using std::stringstream;
using std::endl;
using std::cout;

namespace epics {
namespace pvAccess {

/// Byte to hexchar mapping.
static const char lookup[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

/// Get hex representation of byte.
string toHex(int8 b) {
    string sb;

    int upper = (b>>4)&0x0F;
    sb += lookup[upper];

    int lower = b&0x0F;
    sb += lookup[lower];

    sb += ' ';

    return sb;
}

/// Get ASCII representation of byte, dot if non-readable.
char toAscii(int8 b) {
    if(b>(int8)31&&b<(int8)127)
        return (char)b;
    else
        return '.';
}

void hexDump(std::string const & name, const int8 *bs, int len) {
    hexDump(name, bs, 0, len);
}

void hexDump(std::string const & name, const int8 *bs, int start, int len) {
    hexDump("", name, bs, start, len);
}

void hexDump(std::string const & prologue, string const & name, const int8 *bs,
             int start, int len) {

    stringstream header;

    header<<prologue<<endl<<"Hexdump ["<<name<<"] size = "<<len;

    string out(header.str());

    string chars;

    for(int i = start; i<(start+len); i++) {
        if(((i-start)%16)==0) {
            out += chars;
            out += '\n';
            chars.erase();
        }

        chars += toAscii(bs[i]);

        out += toHex(bs[i]);

        if(((i-start)%4)==3) {
            chars += ' ';
            out += ' ';
        }
    }

    if(len%16!=0) {
        int pad = 0;
        int delta_bytes = 16-(len%16);

        //rest of line (no of bytes)
        //each byte takes two chars plus one ws
        pad = delta_bytes*3;

        //additional whitespaces after four bytes
        pad += (delta_bytes/4);
        pad++;

        for(int i = 0; i<pad; i++)
            chars.insert(0, " ");
    }

    out += chars;
    cout<<out<<endl;
}

}
}
