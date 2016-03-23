#include <iostream>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>

#include <stdio.h>
#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsThread.h>
#include <pv/logger.h>
#include <pv/lock.h>
#include <pv/convert.h>

#include <vector>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <pv/event.h>
#include <epicsExit.h>

#include "pvutils.cpp"

#include <pv/caProvider.h>

using namespace std;
namespace TR1 = std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

//EnumMode enumMode = AutoEnum;

size_t fromString(PVFieldPtr const & pv, StringArray const & from, size_t fromStartIndex);

size_t fromString(PVScalarArrayPtr const &pv, StringArray const & from, size_t fromStartIndex = 0)
{
    int processed = 0;
    size_t fromValueCount = from.size();

    // first get count
    if (fromStartIndex >= fromValueCount)
        throw std::runtime_error("not enough values");

    size_t count;
    istringstream iss(from[fromStartIndex]);
    iss >> count;
    // not fail and entire value is parsed (e.g. to detect 1.2 parsing to 1)
    if (iss.fail() || !iss.eof())
        throw runtime_error("failed to parse element count value (uint) of field '" + pv->getFieldName() + "' from string value '" + from[fromStartIndex] + "'");
    fromStartIndex++;
    processed++;

    if ((fromStartIndex+count) > fromValueCount)
    {
        throw runtime_error("not enough array values for field " + pv->getFieldName());
    }

    PVStringArray::svector valueList(count);
    std::copy(from.begin() + fromStartIndex, from.begin() + fromStartIndex + count, valueList.begin());
    processed += count;

    pv->putFrom<string>(freeze(valueList));

    return processed;
}

size_t fromString(PVStructurePtr const & pvStructure, StringArray const & from, size_t fromStartIndex);

size_t fromString(PVStructureArrayPtr const &pv, StringArray const & from, size_t fromStartIndex = 0)
{
    int processed = 0;
    size_t fromValueCount = from.size();

    // first get count
    if (fromStartIndex >= fromValueCount)
        throw std::runtime_error("not enough values");

    size_t numberOfStructures;
    istringstream iss(from[fromStartIndex]);
    iss >> numberOfStructures;
    // not fail and entire value is parsed (e.g. to detect 1.2 parsing to 1)
    if (iss.fail() || !iss.eof())
        throw runtime_error("failed to parse element count value (uint) of field '" + pv->getFieldName() + "' from string value '" + from[fromStartIndex] + "'");
    fromStartIndex++;
    processed++;

    PVStructureArray::svector pvStructures;
    pvStructures.reserve(numberOfStructures);

    PVDataCreatePtr pvDataCreate = getPVDataCreate();
    for (size_t i = 0; i < numberOfStructures; ++i)
    {
        PVStructurePtr pvStructure = pvDataCreate->createPVStructure(pv->getStructureArray()->getStructure());
        size_t count = fromString(pvStructure, from, fromStartIndex);
        processed += count;
        fromStartIndex += count;
        pvStructures.push_back(pvStructure);
    }

    pv->replace(freeze(pvStructures));

    return processed;
}

size_t fromString(PVUnionArrayPtr const & pvUnionArray, StringArray const & from, size_t fromStartIndex);

size_t fromString(PVUnionPtr const & pvUnion, StringArray const & from, size_t fromStartIndex = 0)
{
    if (pvUnion->getUnion()->isVariant())
        throw std::runtime_error("cannot handle variant unions");

    size_t fromValueCount = from.size();

    if (fromStartIndex >= fromValueCount)
        throw std::runtime_error("not enough values");

    string selector = from[fromStartIndex++];
    PVFieldPtr pv = pvUnion->select(selector);
    if (!pv)
        throw std::runtime_error("invalid union selector value '" + selector + "'");

    size_t processed = fromString(pv, from, fromStartIndex);
    return processed + 1;
}

size_t fromString(PVUnionArrayPtr const &pv, StringArray const & from, size_t fromStartIndex = 0)
{
    int processed = 0;
    size_t fromValueCount = from.size();

    // first get count
    if (fromStartIndex >= fromValueCount)
        throw std::runtime_error("not enough values");

    size_t numberOfUnions;
    istringstream iss(from[fromStartIndex]);
    iss >> numberOfUnions;
    // not fail and entire value is parsed (e.g. to detect 1.2 parsing to 1)
    if (iss.fail() || !iss.eof())
        throw runtime_error("failed to parse element count value (uint) of field '" + pv->getFieldName() + "' from string value '" + from[fromStartIndex] + "'");
    fromStartIndex++;
    processed++;

    PVUnionArray::svector pvUnions;
    pvUnions.reserve(numberOfUnions);

    PVDataCreatePtr pvDataCreate = getPVDataCreate();
    for (size_t i = 0; i < numberOfUnions; ++i)
    {
        PVUnionPtr pvUnion = pvDataCreate->createPVUnion(pv->getUnionArray()->getUnion());
        size_t count = fromString(pvUnion, from, fromStartIndex);
        processed += count;
        fromStartIndex += count;
        pvUnions.push_back(pvUnion);
    }

    pv->replace(freeze(pvUnions));

    return processed;
}

size_t fromString(PVStructurePtr const & pvStructure, StringArray const & from, size_t fromStartIndex = 0)
{
    // handle enum in a special way
    if (pvStructure->getStructure()->getID() == "enum_t")
    {
        int32 index = -1;
        PVInt::shared_pointer pvIndex = pvStructure->getSubField<PVInt>("index");
        if (!pvIndex)
            throw std::runtime_error("enum_t structure does not have 'int index' field");

        PVStringArray::shared_pointer pvChoices = pvStructure->getSubField<PVStringArray>("choices");
        if (!pvChoices)
            throw std::runtime_error("enum_t structure does not have 'string choices[]' field");
        PVStringArray::const_svector choices(pvChoices->view());

        if (enumMode == AutoEnum || enumMode == StringEnum)
        {
            shared_vector<string>::const_iterator it = std::find(choices.begin(), choices.end(), from[fromStartIndex]);
            if (it != choices.end())
                index = static_cast<int32>(it - choices.begin());
            else if (enumMode == StringEnum)
                throw runtime_error("enum string value '" + from[fromStartIndex] + "' invalid");
        }

        if ((enumMode == AutoEnum && index == -1) || enumMode == NumberEnum)
        {
            istringstream iss(from[fromStartIndex]);
            iss >> index;
            // not fail and entire value is parsed (e.g. to detect 1.2 parsing to 1)
            if (iss.fail() || !iss.eof())
                throw runtime_error("enum value '" + from[fromStartIndex] + "' invalid");

            if (index < 0 || index >= static_cast<int32>(choices.size()))
                throw runtime_error("index '" + from[fromStartIndex] + "' out of bounds");
        }

        pvIndex->put(index);
        return 1;
    }

    size_t processed = 0;

    PVFieldPtrArray const & fieldsData = pvStructure->getPVFields();
    if (fieldsData.size() != 0) {
        size_t length = pvStructure->getStructure()->getNumberFields();
        for(size_t i = 0; i < length; i++) {
            size_t count = fromString(fieldsData[i], from, fromStartIndex);
            processed += count;
            fromStartIndex += count;
        }
    }

    return processed;
}

size_t fromString(PVFieldPtr const & fieldField, StringArray const & from, size_t fromStartIndex)
{
    try
    {
        switch (fieldField->getField()->getType())
        {
        case scalar:
        {
            if (fromStartIndex >= from.size())
                throw std::runtime_error("not enough values");

            PVScalarPtr pv = TR1::static_pointer_cast<PVScalar>(fieldField);
            getConvert()->fromString(pv, from[fromStartIndex]);
            return 1;
        }

        case scalarArray:
            return fromString(TR1::static_pointer_cast<PVScalarArray>(fieldField), from, fromStartIndex);

        case structure:
            return fromString(TR1::static_pointer_cast<PVStructure>(fieldField), from, fromStartIndex);

        case structureArray:
            return fromString(TR1::static_pointer_cast<PVStructureArray>(fieldField), from, fromStartIndex);

        case union_:
            return fromString(TR1::static_pointer_cast<PVUnion>(fieldField), from, fromStartIndex);

        case unionArray:
            return fromString(TR1::static_pointer_cast<PVUnionArray>(fieldField), from, fromStartIndex);

        default:
            std::ostringstream oss;
            oss << "fromString unsupported fieldType " << fieldField->getField()->getType();
            throw std::logic_error(oss.str());
        }
    }
    catch (std::exception &ex)
    {
        std::ostringstream os;
        os << "failed to parse '" << fieldField->getField()->getID() << ' '
           << fieldField->getFieldName() << "'";
        os << ": " << ex.what();
        throw std::runtime_error(os.str());
    }
}


#define DEFAULT_TIMEOUT 3.0
#define DEFAULT_REQUEST "field(value)"
#define DEFAULT_PROVIDER "pva"

double timeOut = DEFAULT_TIMEOUT;
string request(DEFAULT_REQUEST);
string defaultProvider(DEFAULT_PROVIDER);
const string noAddress;

enum PrintMode { ValueOnlyMode, StructureMode, TerseMode };
PrintMode mode = ValueOnlyMode;

char fieldSeparator = ' ';

void usage (void)
{
    fprintf (stderr, "\nUsage: pvput [options] <PV name> <values>...\n\n"
             "  -h: Help: Print this message\n"
             "  -v: Print version and exit\n"
             "\noptions:\n"
             "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n"
             "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
             "  -t:                Terse mode - print only successfully written value, without names\n"
             "  -p <provider>:     Set default provider name, default is '%s'\n"
             "  -q:                Quiet mode, print only error messages\n"
             "  -d:                Enable debug output\n"
             "  -F <ofs>:          Use <ofs> as an alternate output field separator\n"
             "  -f <input file>:   Use <input file> as an input that provides a list PV name(s) to be read, use '-' for stdin\n"
             " enum format:\n"
             "  default: Auto - try value as enum string, then as index number\n"
             "  -n: Force enum interpretation of values as numbers\n"
             "  -s: Force enum interpretation of values as strings\n"
             "\nexample: pvput double01 1.234\n\n"
             , DEFAULT_REQUEST, DEFAULT_TIMEOUT, DEFAULT_PROVIDER);
}


void printValue(std::string const & channelName, PVStructure::shared_pointer const & pv)
{
    if (mode == ValueOnlyMode)
    {
        PVField::shared_pointer value = pv->getSubField("value");
        if (value.get() == 0)
        {
            std::cerr << "no 'value' field" << std::endl;
            std::cout << std::endl << *(pv.get()) << std::endl << std::endl;
        }
        else
        {
            Type valueType = value->getField()->getType();
            if (valueType != scalar && valueType != scalarArray)
            {
                // special case for enum
                if (valueType == structure)
                {
                    PVStructurePtr pvStructure = TR1::static_pointer_cast<PVStructure>(value);
                    if (pvStructure->getStructure()->getID() == "enum_t")
                    {
                        if (fieldSeparator == ' ')
                            std::cout << std::setw(30) << std::left << channelName;
                        else
                            std::cout << channelName;

                        std::cout << fieldSeparator;

                        printEnumT(std::cout, pvStructure);

                        std::cout << std::endl;

                        return;
                    }
                }

                // switch to structure mode
                std::cout << channelName << std::endl << *(pv.get()) << std::endl << std::endl;
            }
            else
            {
                if (fieldSeparator == ' ' && value->getField()->getType() == scalar)
                    std::cout << std::setw(30) << std::left << channelName;
                else
                    std::cout << channelName;

                std::cout << fieldSeparator;

                terse(std::cout, value) << std::endl;
            }
        }
    }
    else if (mode == TerseMode)
        terseStructure(std::cout, pv) << std::endl;
    else
        std::cout << std::endl << *(pv.get()) << std::endl << std::endl;
}

struct AtomicBoolean_null_deleter
{
    void operator()(void const *) const {}
};

// standard performance on set/clear, use of TR1::shared_ptr lock-free counter for get
// alternative is to use boost::atomic
class AtomicBoolean
{
public:
    AtomicBoolean() : counter(static_cast<void*>(0), AtomicBoolean_null_deleter()) {};

    void set() {
        mutex.lock();
        setp = counter;
        mutex.unlock();
    }
    void clear() {
        mutex.lock();
        setp.reset();
        mutex.unlock();
    }

    bool get() const {
        return counter.use_count() == 2;
    }
private:
    TR1::shared_ptr<void> counter;
    TR1::shared_ptr<void> setp;
    epics::pvData::Mutex mutex;
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
private:
    PVStructure::shared_pointer m_pvStructure;
    BitSet::shared_pointer m_bitSet;
    Mutex m_pointerMutex;
    Mutex m_eventMutex;
    auto_ptr<Event> m_event;
    string m_channelName;
    AtomicBoolean m_done;

public:

    ChannelPutRequesterImpl(std::string channelName) : m_channelName(channelName)
    {
        resetEvent();
    }

    virtual string getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    }

    virtual void message(std::string const & message, MessageType messageType)
    {
        std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,
                                   ChannelPut::shared_pointer const & channelPut,
                                   epics::pvData::Structure::const_shared_pointer const & /*structure*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel put create: " << dump_stack_only_on_debug(status) << std::endl;
            }

            // get immediately old value
            channelPut->get();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to create channel put: " << dump_stack_only_on_debug(status) << std::endl;
            m_event->signal();
        }
    }

    virtual void getDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const & /*channelPut*/,
                         epics::pvData::PVStructure::shared_pointer const & pvStructure,
                         epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel get: " << dump_stack_only_on_debug(status) << std::endl;
            }

            m_done.set();

            {
                Lock lock(m_pointerMutex);
                m_pvStructure = pvStructure;
                // we always put all, so current bitSet is OK
                m_bitSet = bitSet;
            }

        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to get: " << dump_stack_only_on_debug(status) << std::endl;
        }

        m_event->signal();
    }

    virtual void putDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const & /*channelPut*/)
    {
        if (status.isSuccess())
        {
            // show warning
            if (!status.isOK())
            {
                std::cerr << "[" << m_channelName << "] channel put: " << dump_stack_only_on_debug(status) << std::endl;
            }

            m_done.set();
        }
        else
        {
            std::cerr << "[" << m_channelName << "] failed to put: " << dump_stack_only_on_debug(status) << std::endl;
        }

        m_event->signal();
    }

    PVStructure::shared_pointer getStructure()
    {
        Lock lock(m_pointerMutex);
        return m_pvStructure;
    }

    BitSet::shared_pointer getBitSet()
    {
        Lock lock(m_pointerMutex);
        return m_bitSet;
    }

    void resetEvent()
    {
        Lock lock(m_eventMutex);
        m_event.reset(new Event());
        m_done.clear();
    }

    bool waitUntilDone(double timeOut)
    {
        Event* event;
        {
            Lock lock(m_eventMutex);
            event = m_event.get();
        }

        bool signaled = event->wait(timeOut);
        if (!signaled)
        {
            std::cerr << "[" << m_channelName << "] timeout" << std::endl;
            return false;
        }

        return m_done.get();
    }

};

/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	pvput main()
 * 		Evaluate command line options, set up PVA, connect the
 * 		channels, print the data as requested
 *
 * Arg(s) In:	[options] <pv-name> <values>...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool quiet = false;

    istream* inputStream = 0;
    ifstream ifs;
    bool fromStream = false;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */
    putenv(const_cast<char*>("POSIXLY_CORRECT="));            /* Behave correct on GNU getopt systems; e.g. handle negative numbers */

    while ((opt = getopt(argc, argv, ":hvr:w:tp:qdF:f:ns")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'v':               /* Print version */
        {
            Version version("pvput", "cpp",
                    EPICS_PVA_MAJOR_VERSION,
                    EPICS_PVA_MINOR_VERSION,
                    EPICS_PVA_MAINTENANCE_VERSION,
                    EPICS_PVA_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        }
        case 'w':               /* Set PVA timeout value */
            if((epicsScanDouble(optarg, &timeOut)) != 1 || timeOut <= 0.0)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                        "- ignored. ('pvput -h' for help.)\n", optarg);
                timeOut = DEFAULT_TIMEOUT;
            }
            break;
        case 'r':               /* Set PVA timeout value */
            request = optarg;
            // do not override terse mode
            if (mode == ValueOnlyMode) mode = StructureMode;
            break;
        case 't':               /* Terse mode */
            mode = TerseMode;
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'p':               /* Set default provider */
            defaultProvider = optarg;
            break;
        case 'q':               /* Quiet mode */
            quiet = true;
            break;
        case 'F':               /* Store this for output formatting */
            fieldSeparator = (char) *optarg;
            break;
        case 'f':               /* Use input stream as input */
        {
            string fileName = optarg;
            if (fileName == "-")
                inputStream = &cin;
            else
            {
                ifs.open(fileName.c_str(), ifstream::in);
                if (!ifs)
                {
                    fprintf(stderr,
                            "Failed to open file '%s'.\n",
                            fileName.c_str());
                    return 1;
                }
                else
                    inputStream = &ifs;
            }

            fromStream = true;
            break;
        }
        case 'n':
            enumMode = NumberEnum;
            break;
        case 's':
            enumMode = StringEnum;
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
    string pv = argv[optind++];

    URI uri;
    bool validURI = URI::parse(pv, uri);

    TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));

    string providerName(defaultProvider);
    string pvName(pv);
    string address(noAddress);
    bool usingDefaultProvider = true;
    if (validURI)
    {
        if (uri.path.length() <= 1)
        {
            std::cerr << "invalid URI '" << pv << "', empty path" << std::endl;
            return 1;
        }
        providerName = uri.protocol;
        pvName = uri.path.substr(1);
        address = uri.host;
        usingDefaultProvider = false;
    }

    if ((providerName != "pva") && (providerName != "ca"))
    {
        std::cerr << "invalid "
                  << (usingDefaultProvider ? "default provider" : "URI scheme")
                  << " '" << providerName
                  << "', only 'pva' and 'ca' are supported" << std::endl;
        return 1;
    }

    int nVals = argc - optind;       /* Remaining arg list are PV names */
    if (nVals > 0)
    {
        // do not allow reading file and command line specified pvs
        fromStream = false;
    }
    else if (nVals < 1 && !fromStream)
    {
        fprintf(stderr, "No value(s) specified. ('pvput -h' for help.)\n");
        return 1;
    }

    vector<string> values;
    if (fromStream)
    {
        string cn;
        while (true)
        {
            *inputStream >> cn;
            if (!(*inputStream))
                break;
            values.push_back(cn);
        }
    }
    else
    {
        // copy values from command line
        for (int n = 0; optind < argc; n++, optind++)
            values.push_back(argv[optind]);
    }

    Requester::shared_pointer requester(new RequesterImpl("pvput"));

    PVStructure::shared_pointer pvRequest = CreateRequest::create()->createRequest(request);
    if(pvRequest.get()==NULL) {
        fprintf(stderr, "failed to parse request string\n");
        return 1;
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    std::cout << std::boolalpha;
    terseSeparator(fieldSeparator);
    setEnumPrintMode(enumMode);

    ClientFactory::start();
    epics::pvAccess::ca::CAClientFactory::start();

    bool allOK = true;

    try
    {
        do
        {
            // first connect
            TR1::shared_ptr<ChannelRequesterImpl> channelRequesterImpl(new ChannelRequesterImpl(quiet));

            Channel::shared_pointer channel;
            if (address.empty())
                channel = getChannelProviderRegistry()->getProvider(
                              providerName)->createChannel(pvName, channelRequesterImpl);
            else
                channel = getChannelProviderRegistry()->getProvider(
                              providerName)->createChannel(pvName, channelRequesterImpl,
                                      ChannelProvider::PRIORITY_DEFAULT, address);

            if (channelRequesterImpl->waitUntilConnected(timeOut))
            {
                TR1::shared_ptr<ChannelPutRequesterImpl> putRequesterImpl(new ChannelPutRequesterImpl(channel->getChannelName()));
                if (mode != TerseMode && !quiet)
                    std::cout << "Old : ";
                ChannelPut::shared_pointer channelPut = channel->createChannelPut(putRequesterImpl, pvRequest);
                allOK &= putRequesterImpl->waitUntilDone(timeOut);
                if (allOK)
                {
                    if (mode != TerseMode && !quiet)
                        printValue(pvName, putRequesterImpl->getStructure());

                    // convert value from string
                    // since we access structure from another thread, we need to lock
                    {
                        ScopedLock lock(channelPut);
                        fromString(putRequesterImpl->getStructure(), values);
                    }

                    // we do a put
                    putRequesterImpl->resetEvent();
                    // note on bitSet: we get all, we set all
                    channelPut->put(putRequesterImpl->getStructure(), putRequesterImpl->getBitSet());
                    allOK &= putRequesterImpl->waitUntilDone(timeOut);

                    if (allOK)
                    {
                        // and than a get again to verify put
                        if (mode != TerseMode && !quiet) std::cout << "New : ";
                        putRequesterImpl->resetEvent();
                        channelPut->get();
                        allOK &= putRequesterImpl->waitUntilDone(timeOut);
                        if (allOK && !quiet)
                            printValue(pvName, putRequesterImpl->getStructure());
                    }
                }
            }
            else
            {
                allOK = false;
                std::cerr << "[" << channel->getChannelName() << "] connection timeout" << std::endl;
            }
            channel->destroy();
        }
        while (false);
    } catch (std::out_of_range& oor) {
        allOK = false;
        std::cerr << "parse error: not enough values" << std::endl;
    } catch (std::exception& ex) {
        allOK = false;
        std::cerr << ex.what() << std::endl;
    } catch (...) {
        allOK = false;
        std::cerr << "unknown exception caught" << std::endl;
    }

    epics::pvAccess::ca::CAClientFactory::stop();
    ClientFactory::stop();

    return allOK ? 0 : 1;
}
