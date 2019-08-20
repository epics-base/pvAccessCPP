/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <stdio.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>

#include <pv/pvData.h>
#include <pv/json.h>
#include <pva/client.h>

#include "pvutils.h"

#ifndef EXECNAME
#  define EXECNAME "pvcall"
#endif

namespace {

void callusage (void)
{
    fprintf (stderr, "\nUsage: " EXECNAME " [options] <PV name> [<arg1>=<value>]...\n"
             "\n"
             COMMON_OPTIONS
             "  -s <service name>:   legacy form of PV name\n"
             "  -a <service arg=value>:    legacy form of argument\n"
             " deprecated options:\n"
             "  -q, -t, -i, -n, -F: ignored\n"
             "  -f <input file>:   errors\n"
             "\nexample: " EXECNAME " pv:name:add lhs=1 rhs=2\n\n"
             , request.c_str(), timeout, defaultProvider.c_str());
}

typedef std::pair<std::string, pvd::PVFieldPtr> arg_t;
typedef std::vector<arg_t> args_t;

arg_t parseArg(const std::string& raw) {
    size_t equal = raw.find_first_of('=');
    if(equal==raw.npos)
        throw std::runtime_error("Argument missing '='");

    std::string sval(raw.substr(equal+1));

    pvd::PVFieldPtr value;
    if(sval.size()>=2 && sval[0]=='[' && sval[sval.size()-1]==']') {
        pvd::shared_vector<std::string> sarr;

        jarray(sarr, sval.c_str());

        pvd::PVStringArrayPtr V(pvd::getPVDataCreate()->createPVScalarArray<pvd::PVStringArray>());
        V->replace(pvd::freeze(sarr));
        value = V;

    } else if(sval.size()>=2 && sval[0]=='{' && sval[sval.size()-1]=='}') {
        std::istringstream strm(sval);

        value = pvd::parseJSON(strm);

    } else {
        pvd::PVStringPtr V(pvd::getPVDataCreate()->createPVScalar<pvd::PVString>());
        V->put(sval);
        value = V;
    }

    assert(!!value);
    return std::make_pair(raw.substr(0, equal), value);
}

} //namespace

#ifndef MAIN
#  define MAIN main
#endif

int MAIN (int argc, char *argv[])
{
    try {
        int opt;                    /* getopt() current option */
        std::string pv;

        args_t args;

        while ((opt = getopt(argc, argv, ":hvVM:r:w:p:ds:a:")) != -1) {
            switch (opt) {
            case 'h':               /* Print usage */
                callusage();
                return 0;
            case 'v':
                verbosity++;
                break;
            case 'V':               /* Print version */
            {
                pva::Version version(EXECNAME, "cpp",
                                     EPICS_PVA_MAJOR_VERSION,
                                     EPICS_PVA_MINOR_VERSION,
                                     EPICS_PVA_MAINTENANCE_VERSION,
                                     EPICS_PVA_DEVELOPMENT_FLAG);
                fprintf(stdout, "%s\n", version.getVersionString().c_str());
                return 0;
            }
                break;
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
                                    "- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
                } else {
                    timeout = temp;
                }
            }
                break;
            case 'r':               /* Set PVA timeout value */
                request = optarg;
                break;
            case 'p':               /* Set default provider */
                defaultProvider = optarg;
                break;
            case 'd':               /* Debug log level */
                debugFlag = true;
                break;
            case 's':
                pv = optarg;
                break;
            case 'a':
                try {
                args.push_back(parseArg(optarg));
            } catch(std::exception& e){
                    std::cerr<<"Error parsing argument '"<<optarg<<"'\n";
                    return 1;
                }
                break;
            case '?':
                fprintf(stderr,
                        "Unrecognized option: '-%c'. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            case ':':
                fprintf(stderr,
                        "Option '-%c' requires an argument. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            default :
                callusage();
                return 1;
            }
        }

        if(!pv.empty()) {
            // ok
        } else if (argc <= optind) {
            fprintf(stderr, "No pv name specified. ('pvput -h' for help.)\n");
            return 1;
        } else {
            pv = argv[optind++];
        }


        for(int i=optind; i<argc; i++) {
            try {
                args.push_back(parseArg(argv[i]));
            } catch(std::exception& e){
                std::cerr<<"Error parsing argument '"<<optarg<<"'\n";
                return 1;
            }
        }

        pvd::PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = pvd::createRequest(request);
        } catch(std::exception& e){
            fprintf(stderr, "failed to parse request string: %s\n", e.what());
            return 1;
        }

        pvd::PVStructurePtr argument;
        {
            pvd::FieldBuilderPtr builder(pvd::getFieldCreate()->createFieldBuilder());
            builder = builder->setId("epics:nt/NTURI:1.0")
                    ->add("scheme", pvd::pvString)
                    ->add("authority", pvd::pvString)
                    ->add("path", pvd::pvString)
                    ->addNestedStructure("query");

            for(args_t::const_iterator it(args.begin()), end(args.end()); it!=end; ++it) {
                builder = builder->add(it->first, it->second->getField());
            }

            pvd::StructureConstPtr type(builder->endNested()
                                        ->createStructure());
            argument = pvd::getPVDataCreate()->createPVStructure(type);

            argument->getSubFieldT<pvd::PVString>("scheme")->put(defaultProvider);
            argument->getSubFieldT<pvd::PVString>("path")->put(pv);
            pvd::PVStructurePtr query(argument->getSubFieldT<pvd::PVStructure>("query"));

            for(args_t::const_iterator it(args.begin()), end(args.end()); it!=end; ++it) {
                query->getSubFieldT(it->first)->copy(*it->second);
            }
        }

        if(verbosity>=1)
            std::cout<<"# Argument\n"<<argument->stream().format(outmode);

        pvac::ClientProvider prov(defaultProvider);

        pvac::ClientChannel chan(prov.connect(pv));

        pvd::PVStructure::const_shared_pointer ret;
        try {
            ret = chan.rpc(timeout, argument, pvRequest);
        }catch(pvac::Timeout&){
            std::cerr<<"Timeout\n";
            return 1;
        }catch(std::exception& e) {
            std::cerr<<"Error: "<<e.what()<<"\n";
            return 1;
        }
        assert(ret);

        if(verbosity>=1)
            std::cout<<"# Result\n";
        std::cout<<ret->stream().format(outmode);

        return 0;
    } catch(std::exception& e) {
        std::cerr<<"Error: "<<e.what()<<"\n";
        return 1;
    }
}
