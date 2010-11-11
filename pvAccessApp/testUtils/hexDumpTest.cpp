/*
 * HexDumpTest.cpp
 *
 *  Created on: Nov 8, 2010
 *      Author: Miha Vitorovic
 */

#include "hexDump.h"

#include <iostream>

using namespace epics::pvAccess;
using std::cout;
using std::endl;

int main(int argc, char *argv[]) {
    char TO_DUMP[] = "pvAccess dump test\0\1\2\3\4\5\6\254\255\256";

    hexDump("test", (int8*)TO_DUMP, 18+9);

    hexDump("only text", (int8*)TO_DUMP, 18);

    hexDump("22 byte test", (int8*)TO_DUMP, 22);

    cout<<endl;

    return 0;
}
