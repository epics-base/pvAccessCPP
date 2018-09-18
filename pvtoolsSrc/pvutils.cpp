/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <stdio.h>
#if !defined(_WIN32)
#include <signal.h>
#define USE_SIGNAL
#endif

#include <iostream>

#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

#include <pv/logger.h>
#include <pv/pvTimeStamp.h>

#include "pvutils.h"

double timeout = 5.0;
bool debugFlag = false;

pvd::PVStructure::Formatter::format_t outmode = pvd::PVStructure::Formatter::NT;
int verbosity;

std::string request("");
std::string defaultProvider("pva");

epicsMutex Tracker::doneLock;
epicsEvent Tracker::doneEvt;
Tracker::inprog_t Tracker::inprog;
bool Tracker::abort = false;

#ifdef USE_SIGNAL
static
void alldone(int num)
{
    (void)num;
    Tracker::abort = true;
    Tracker::doneEvt.signal();
}
#endif

void Tracker::prepare()
{
#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
}

static
void early(const char *inp, unsigned pos)
{
    fprintf(stderr, "Unexpected end of input: %s\n", inp);
    throw std::runtime_error("Unexpected end of input");
}

// rudimentory parser for json array
// needed as long as Base < 3.15 is supported.
// for consistency, used with all version
void jarray(pvd::shared_vector<std::string>& out, const char *inp)
{
    assert(inp[0]=='[');
    const char * const orig = inp;
    inp++;

    while(true) {
        // starting a new token

        for(; *inp==' '; inp++) {} // skip leading whitespace

        if(*inp=='\0') early(inp, inp-orig);

        if(isalnum(*inp) || *inp=='+' || *inp=='-') {
            // number

            const char *start = inp;

            while(isalnum(*inp) || *inp=='.' || *inp=='+' || *inp=='-')
                inp++;

            if(*inp=='\0') early(inp, inp-orig);

            // inp points to first char after token

            out.push_back(std::string(start, inp-start));

        } else if(*inp=='"') {
            // quoted string

            const char *start = ++inp; // skip quote

            while(*inp!='\0' && *inp!='"')
                inp++;

            if(*inp=='\0') early(inp, inp-orig);

            // inp points to trailing "

            out.push_back(std::string(start, inp-start));

            inp++; // skip trailing "

        } else if(*inp==']') {
            // no-op
        } else {
            fprintf(stderr, "Unknown token '%c' in \"%s\"", *inp, inp);
            throw std::runtime_error("Unknown token");
        }

        for(; *inp==' '; inp++) {} // skip trailing whitespace

        if(*inp==',') inp++;
        else if(*inp==']') break;
        else {
            fprintf(stderr, "Unknown token '%c' in \"%s\"", *inp, inp);
            throw std::runtime_error("Unknown token");
        }
    }

}
