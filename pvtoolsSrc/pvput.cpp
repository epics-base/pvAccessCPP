/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <iostream>
#include <vector>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <stdio.h>
#include <epicsExit.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>

#include <pv/logger.h>
#include <pv/lock.h>
#include <pv/convert.h>
#include <pv/pvdVersion.h>
#include <pv/json.h>

#include <pva/client.h>

#include <pv/pvaDefs.h>
#include <pv/event.h>

#include <pv/caProvider.h>

#include "pvutils.h"

namespace {

void usage (bool details=false)
{
    fprintf (stderr,
             "Usage: pvput [options] <PV name> <value>\n"
             "       pvput [options] <PV name> <size/ignored> <value> [<value> ...]\n"
             "       pvput [options] <PV name> <field>=<value> ...\n"
             "       pvput [options] <PV name> <json_array>\n"
             "       pvput [options] <PV name> <json_map>\n"
             "\n"
             COMMON_OPTIONS
             " Deprecated options:\n"
             "  default: Auto - try value as enum string, then as index number\n"
             "  -n, -s, -F, -t: ignored\n"
             "  -f <input file>: error"
             , request.c_str(), timeout, defaultProvider.c_str());
    if(details) {
        fprintf (stderr,
                 "\n JSON support is present\n"
                 "\nExamples:\n"
                 "\n"
                 "  pvput double01 1.234       # shorthand\n"
                 "  pvput double01 value=1.234\n"
                 "\n"
                 "  pvput arr:pv X 1.0 2.0  # shorthand  (X is arbitrary and ignored)\n"
                 "  pvput arr:pv \"[1.0, 2.0]\"            # shorthand\n"
                 "  pvput arr:pv value=\"[1.0, 2.0]\"\n"
                 "\n"
                 "Field values may be given with JSON syntax.\n"
                 "\n"
                 "Complete structure\n"
                 "\n"
                 "  pvput double01 '{\"value\":1.234}'\n"
                 "\n"
                 "Sub-structure(s)\n"
                 "\n"
                 "  pvput group:pv some='{\"value\":1.234}' other='{\"value\":\"a string\"}'\n"
                 "\n"
                 );
    }
}

void printValue(std::string const & channelName, pvd::PVStructure::const_shared_pointer const & pv)
{
    std::cout<<pv->stream().format(outmode);
    std::cout.flush();
}

struct Putter : public pvac::ClientChannel::PutCallback
{
    epicsEvent wait;
    epicsMutex lock;
    bool done;
    pvac::PutEvent::event_t result;
    std::string message;

    Putter() :done(false) {}

    std::vector<std::string> values;

    pvd::shared_vector<std::string> jarr;

    virtual void putBuild(const epics::pvData::StructureConstPtr& build, Args& args)
    {
        if(debugFlag) std::cerr<<"Server defined structure\n"<<build;
        pvd::PVStructurePtr root(pvd::getPVDataCreate()->createPVStructure(build));

        typedef pvd::shared_vector<std::string> bare_t;
        bare_t bare;

        typedef std::pair<std::string, std::string> KV_t;
        typedef std::vector<KV_t> pairs_t;
        pairs_t pairs;

        for(size_t i=0, N=values.size(); i<N; i++) {
            // Try to break the input value into a field=value pair
            size_t sep = values[i].find_first_of('=');
            if(sep==std::string::npos) {
                // If the value does not contain a "=", it is a bare value
                bare.push_back(values[i]);
            } else {
                // Verify if the "field" exists in the PV structure
                std::string fname(values[i].substr(0, sep));
                pvd::PVFieldPtr fld(root->getSubField(fname));
                if(fld) {
                    // The "field" exist. Treat this input as a filed=value pair.
                    pairs.push_back(std::make_pair(fname, values[i].substr(sep+1)));
                } else {
                    // If the "field" does not exist, this could be a bare value containing a "=" char.
                    // The ".value" field must exist and be of type "string". Otherwise, the field was
                    // incorrect and we ignore it.
                    pvd::PVFieldPtr fldv(root->getSubFieldT("value"));
                    if (fldv) {
                        if ((pvd::scalar==fldv->getField()->getType()) && (0==fldv->getField()->getID().compare("string")))
                            // Tread it as a bare value
                            bare.push_back(values[i]);
                        else
                            // Ignore it
                            fprintf(stderr, "%s : Warning: no such field. Ignoring it.\n", fname.c_str());
                    }
                }
            }
        }

        if(!bare.empty() && !pairs.empty()) {
            throw std::runtime_error("Can't mix bare values and field=value pairs");
        } else if(bare.empty() && pairs.empty()) {
            // We could have ignored all the value at this point
            throw std::runtime_error("No valid value(s) specified");
        } else if(bare.size()==1 && bare[0][0]=='[') {
            // treat plain "[...]" as "value=[...]"
            pairs.push_back(std::make_pair("value", bare[0]));
            bare.clear();
        }

        if(bare.size()==1 && bare[0][0]=='{') {
            if(debugFlag) fprintf(stderr, "In JSON top mode\n");
            std::istringstream strm(bare[0]);
            parseJSON(strm, root, &args.tosend);

        } else if(pairs.empty()) {
            if(debugFlag) fprintf(stderr, "In plain value mode\n");

            pvd::PVFieldPtr fld(root->getSubField("value"));
            if(!fld)
                throw std::runtime_error("Structure has no .value");
            pvd::Type ftype = fld->getField()->getType();

            if(ftype==pvd::scalar) {
                if(bare.size()!=1) {
                    throw std::runtime_error("Can't assign multiple values to scalar");
                }
                pvd::PVScalar* sfld(static_cast<pvd::PVScalar*>(fld.get()));
                sfld->putFrom(bare[0]);
                args.tosend.set(sfld->getFieldOffset());

            } else if(ftype==pvd::scalarArray) {
                pvd::PVScalarArray* sfld(static_cast<pvd::PVScalarArray*>(fld.get()));

                // first element is "length" which we ignore for compatibility
                bare.slice(1);

                sfld->putFrom(freeze(bare));
                args.tosend.set(sfld->getFieldOffset());

            } else if(ftype==pvd::structure && fld->getField()->getID()=="enum_t") {
                if(bare.size()!=1) {
                    throw std::runtime_error("Can't assign multiple values to enum");
                }
                pvd::PVStructure* sfld(static_cast<pvd::PVStructure*>(fld.get()));

                assert(!!args.previous); // ensure by calling put(..., true) below
                pvd::PVScalar* idxfld(sfld->getSubFieldT<pvd::PVScalar>("index").get());
                pvd::PVStringArray::const_svector choices(args.previous->getSubFieldT<pvd::PVStringArray>("value.choices")->view());

                bool found=false;
                for(size_t i=0; i<choices.size(); i++) {
                    if(bare[0]==choices[i]) {
                        idxfld->putFrom<pvd::int64>(i);
                        found=true;
                        break;
                    }
                }

                if(!found) {
                    // try to parse as integer
                    idxfld->putFrom(bare[0]);
                }

                args.tosend.set(idxfld->getFieldOffset());
            } else {
                throw std::runtime_error("Don't know how to set field .value");
            }

        } else {
            if(debugFlag) fprintf(stderr, "In field=value mode\n");

            for(pairs_t::const_iterator it=pairs.begin(), end=pairs.end(); it!=end; ++it)
            {
                pvd::PVFieldPtr fld(root->getSubField(it->first));
                if(!fld) {
                    fprintf(stderr, "%s : Warning: no such field\n", it->first.c_str());
                    // ignore

                } else if(it->second[0]=='[') {
                    pvd::shared_vector<std::string> arr;
                    jarray(arr, it->second.c_str());

                    pvd::PVScalarArray* afld(dynamic_cast<pvd::PVScalarArray*>(fld.get()));
                    if(!afld) {
                        fprintf(stderr, "%s : Error not a scalar array field\n", it->first.c_str());
                        throw std::runtime_error("Not a scalar array field");
                    }
                    afld->putFrom(freeze(arr));
                    args.tosend.set(afld->getFieldOffset());

                } else if(it->second[0]=='{' || it->second[0]=='[') {
                    std::istringstream strm(it->second);
                    parseJSON(strm, fld, &args.tosend);

                } else {
                    pvd::PVScalarPtr sfld(std::tr1::dynamic_pointer_cast<pvd::PVScalar>(fld));
                    if(!sfld) {
                        fprintf(stderr, "%s : Error: need a scalar field\n", it->first.c_str());
                    } else {
                        sfld->putFrom(it->second);
                        args.tosend.set(sfld->getFieldOffset());
                    }
                }
            }
        }

        args.root = root;
        if(debugFlag)
            std::cout<<"To be sent: "<<args.tosend<<"\n"<<args.root;
    }

    virtual void putDone(const pvac::PutEvent& evt)
    {
        {
            epicsGuard<epicsMutex> G(lock);
            result = evt.event;
            message = evt.message;
            done = true;
        }

        wait.signal();
    }
};

} // namespace

int main (int argc, char *argv[])
{
    try {
        int opt;                    /* getopt() current option */
        bool quiet = false;

        setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */
        putenv(const_cast<char*>("POSIXLY_CORRECT="));            /* Behave correct on GNU getopt systems; e.g. handle negative numbers */

        while ((opt = getopt(argc, argv, ":hvVM:r:w:tp:qdF:f:ns")) != -1) {
            switch (opt) {
            case 'h':               /* Print usage */
                usage(true);
                return 0;
            case 'v':
                outmode = pvd::PVStructure::Formatter::Raw;
                break;
            case 'V':               /* Print version */
            {
                fprintf(stdout, "pvAccess %u.%u.%u%s\n",
                        EPICS_PVA_MAJOR_VERSION,
                        EPICS_PVA_MINOR_VERSION,
                        EPICS_PVA_MAINTENANCE_VERSION,
                        (EPICS_PVA_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
                fprintf(stdout, "pvData %u.%u.%u%s\n",
                        EPICS_PVD_MAJOR_VERSION,
                        EPICS_PVD_MINOR_VERSION,
                        EPICS_PVD_MAINTENANCE_VERSION,
                        (EPICS_PVD_DEVELOPMENT_FLAG)?"-SNAPSHOT":"");
                fprintf(stdout, "Base %s\n", EPICS_VERSION_FULL);
                return 0;
            }
            case 'M':
                if(strcmp(optarg, "raw")==0) {
                    outmode = pvd::PVStructure::Formatter::Raw;
                } else if(strcmp(optarg, "nt")==0) {
                    outmode = pvd::PVStructure::Formatter::NT;
                } else if(strcmp(optarg, "json")==0) {
                    outmode = pvd::PVStructure::Formatter::JSON;
                } else {
                    fprintf(stderr, "Unknown output mode '%s'\n", optarg);
                    outmode = pvd::PVStructure::Formatter::Raw;
                }
                break;
            case 'w':               /* Set PVA timeout value */
            {
                double temp;
                if((epicsScanDouble(optarg, &temp)) != 1)
                {
                    fprintf(stderr, "'%s' is not a valid timeout value "
                                    "- ignored. ('pvput -h' for help.)\n", optarg);
                } else {
                    timeout = temp;
                }
            }
                break;
            case 'r':               /* Set PVA timeout value */
                request = optarg;
                break;
            case 't':               /* Terse mode */
                // deprecated
                break;
            case 'd':               /* Debug log level */
                debugFlag = true;
                break;
            case 'p':               /* Set default provider */
                defaultProvider = optarg;
                break;
            case 'q':               /* Quiet mode */
                quiet = true;
                break;
            case 'F':               /* Store this for output formatting */
                break;
            case 'f':               /* Use input stream as input */
                fprintf(stderr, "Unsupported option -f\n");
                return 1;
            case 'n':
                break;
            case 's':
                break;
            case '?':
                fprintf(stderr,
                        "Unrecognized option: '-%c'. ('pvput -h' for help.)\n",
                        optopt);
                return 1;
            case ':':
                fprintf(stderr,
                        "Option '-%c' requires an argument. ('pvput -h' for help.)\n",
                        optopt);
                return 1;
            default :
                usage();
                return 1;
            }
        }

        if (argc <= optind)
        {
            fprintf(stderr, "No pv name specified. ('pvput -h' for help.)\n");
            return 1;
        }
        std::string pvName(argv[optind++]);

        std::string providerName(defaultProvider);

        int nVals = argc - optind;       /* Remaining arg list are PV names */
        if (nVals < 1)
        {
            fprintf(stderr, "No value(s) specified. ('pvput -h' for help.)\n");
            return 1;
        }

        Putter thework;

        // copy values from command line
        for (int n = 0; optind < argc; n++, optind++)
            thework.values.push_back(argv[optind]);

        pvd::PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = pvd::createRequest(request);
        } catch(std::exception& e){
            fprintf(stderr, "failed to parse request string: %s\n", e.what());
            return 1;
        }

        SET_LOG_LEVEL(debugFlag ? pva::logLevelDebug : pva::logLevelError);

        std::cout << std::boolalpha;

        epics::pvAccess::ca::CAClientFactory::start();

        pvac::ClientProvider ctxt(providerName);

        pvac::ClientChannel chan(ctxt.connect(pvName));

        if (!quiet) {
            std::cout << "Old : ";
            printValue(pvName, chan.get(timeout, pvRequest));
        }

        {
            pvac::Operation op(chan.put(&thework, pvRequest, true));

            epicsGuard<epicsMutex> G(thework.lock);
            while(!thework.done) {
                epicsGuardRelease<epicsMutex> U(G);
                if(!thework.wait.wait(timeout)) {
                    fprintf(stderr, "Put timeout\n");
                    return 1;
                }
            }
        }

        if(thework.result==pvac::PutEvent::Fail) {
            fprintf(stderr, "Error: %s\n", thework.message.c_str());
        }

        if (!quiet) {
            std::cout << "New : ";
        }
        printValue(pvName, chan.get(timeout, pvRequest));

        return thework.result!=pvac::PutEvent::Success;
    } catch(std::exception& e) {
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}
