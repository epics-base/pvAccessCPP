#include "pvutils.h"

#include <iostream>

#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

#include <pv/logger.h>
#include <pv/pvTimeStamp.h>

using namespace std;
namespace TR1 = std::tr1;

using namespace epics::pvData;
using namespace epics::pvAccess;

std::ostream& operator<<(std::ostream& o, const dump_stack_only_on_debug& d)
{
    const Status &s = d.status;
    o << '[' << Status::StatusTypeName[s.getType()] << "] ";
    string msg = s.getMessage();
    if (!msg.empty())
    {
        o << msg;
    }
    else
    {
        o << "(no error message)";
    }
    // dump stack trace only if on debug mode
    if (IS_LOGGABLE(logLevelDebug))
    {
        string sd = s.getStackDump();
        if (!sd.empty())
        {
            o << std::endl << sd;
        }
    }
    return o;
}

char separator = ' ';
void terseSeparator(char c)
{
    separator = c;
}

char arrayCountFlag = true;

EnumMode enumMode = AutoEnum;

bool formatTTypesFlag = true;
bool printUserTagFlag = true;

std::ostream& terse(std::ostream& o, PVField::const_shared_pointer const & pv)
{
    Type type = pv->getField()->getType();
    switch (type)
    {
    case scalar:
        o << *(pv.get());
        return o;
    case structure:
        return terseStructure(o, TR1::static_pointer_cast<const PVStructure>(pv));
        break;
    case scalarArray:
        return terseScalarArray(o, TR1::static_pointer_cast<const PVScalarArray>(pv));
        break;
    case structureArray:
        return terseStructureArray(o, TR1::static_pointer_cast<const PVStructureArray>(pv));
        break;
    case union_:
        return terseUnion(o, TR1::static_pointer_cast<const PVUnion>(pv));
        break;
    case unionArray:
        return terseUnionArray(o, TR1::static_pointer_cast<const PVUnionArray>(pv));
        break;
    default:
        std::ostringstream msg("unknown Field type: ");
        msg << type;
        throw std::logic_error(msg.str());
    }
}

std::ostream& printEnumT(std::ostream& o, epics::pvData::PVStructure const & pvEnumT)
{
    PVInt::const_shared_pointer pvIndex = pvEnumT.getSubField<PVInt>("index");
    if (!pvIndex)
        throw std::runtime_error("enum_t structure does not have 'int index' field");

    PVStringArray::const_shared_pointer pvChoices = pvEnumT.getSubField<PVStringArray>("choices");
    if (!pvChoices)
        throw std::runtime_error("enum_t structure does not have 'string choices[]' field");

    if (enumMode == AutoEnum || enumMode == StringEnum)
    {
        int32 ix = pvIndex->get();
        if (ix < 0 || ix >= static_cast<int32>(pvChoices->getLength()))
            o << ix;
        else
            pvChoices->dumpValue(o, ix);
    }
    else
        o << pvIndex->get();

    return o;
}

std::ostream& printEnumT(std::ostream& o, epics::pvData::PVStructure::const_shared_pointer const & pvEnumT)
{
    return printEnumT(o, *pvEnumT);
}

std::ostream& printTimeT(std::ostream& o, epics::pvData::PVStructure::const_shared_pointer const & pvTimeT)
{
    char timeText[32];
    epicsTimeStamp epicsTS;

    PVScalar::const_shared_pointer secf(pvTimeT->getSubField<PVScalar>("secondsPastEpoch")),
                                   nsecf(pvTimeT->getSubField<PVScalar>("nanoseconds")),
                                   tagf(pvTimeT->getSubField<PVScalar>("userTag"));

    epicsTS.secPastEpoch = secf ? secf->getAs<int64>() : 0;
    epicsTS.nsec = nsecf ? nsecf->getAs<int32>() : 0;

    epicsTS.secPastEpoch -= POSIX_TIME_AT_EPICS_EPOCH;

    epicsTimeToStrftime(timeText, sizeof(timeText), "%Y-%m-%dT%H:%M:%S.%03f", &epicsTS);
    o << timeText;
    if (printUserTagFlag && tagf)
        o << separator << tagf->getAs<int32>();

    return o;
}

const char* severityNames[] = {
    "NO_ALARM",    // 0
    "MINOR",
    "MAJOR",
    "INVALID",
    "UNDEFINED" // 4
};

const char* statusNames[] = {
    "NO_STATUS",  // 0
    "DEVICE",
    "DRIVER",
    "RECORD",
    "DB",
    "CONF",
    "UNDEFINED",
    "CLIENT"  // 7
};

std::ostream& printAlarmT(std::ostream& o, epics::pvData::PVStructure::const_shared_pointer const & pvAlarmT)
{
    PVInt::const_shared_pointer pvSeverity = pvAlarmT->getSubField<PVInt>("severity");
    if (!pvSeverity)
        throw std::runtime_error("alarm_t structure does not have 'int severity' field");

    PVInt::const_shared_pointer pvStatus = pvAlarmT->getSubField<PVInt>("status");
    if (!pvStatus)
        throw std::runtime_error("alarm_t structure does not have 'int status' field");

    PVString::const_shared_pointer pvMessage = pvAlarmT->getSubField<PVString>("message");
    if (!pvMessage)
        throw std::runtime_error("alarm_t structure does not have 'string message' field");

    int32 v = pvSeverity->get();
    if (v < 0 || v > 4)
        o << v;
    else
        o << severityNames[v];
    o << separator;

    v = pvStatus->get();
    if (v < 0 || v > 7)
        o << v;
    else
        o << statusNames[v];
    o << separator;
    if (pvMessage->get().empty())
        o << "<no message>";
    else
        o << pvMessage->get();
    return o;
}


bool isTType(epics::pvData::PVStructure::const_shared_pointer const & pvStructure)
{
    // "forget" about Ttype if disabled
    if (!formatTTypesFlag)
        return false;

    string id = pvStructure->getStructure()->getID();
    return (id == "enum_t" ||
            id == "time_t" ||
            id == "alarm_t");
}

bool formatTType(std::ostream& o, epics::pvData::PVStructure::const_shared_pointer const & pvStructure)
{
    // special t-types support (enum_t and time_t, etc.)
    if (formatTTypesFlag)
    {
        string id = pvStructure->getStructure()->getID();
        if (id == "enum_t")
        {
            printEnumT(o, pvStructure);
            return true;
        }
        else if (id == "time_t")
        {
            printTimeT(o, pvStructure);
            return true;
        }
        else if (id == "alarm_t")
        {
            printAlarmT(o, pvStructure);
            return true;
        }
    }

    return false;
}


std::ostream& terseStructure(std::ostream& o, PVStructure::const_shared_pointer const & pvStructure)
{
    if (!pvStructure)
    {
        o << "(null)";
        return o;
    }

    // special t-types support (enum_t and time_t, etc.)
    if (formatTType(o, pvStructure))
        return o;

    PVFieldPtrArray fieldsData = pvStructure->getPVFields();
    size_t length = pvStructure->getStructure()->getNumberFields();
    bool first = true;
    for (size_t i = 0; i < length; i++) {
        if (first)
            first = false;
        else
            o << separator;

        terse(o, fieldsData[i]);
    }
    return o;
}

std::ostream& terseUnion(std::ostream& o, PVUnion::const_shared_pointer const & pvUnion)
{
    if (!pvUnion || !pvUnion->get())
    {
        o << "(null)";
        return o;
    }

    return terse(o, pvUnion->get());
}

std::ostream& terseScalarArray(std::ostream& o, const PVScalarArray::const_shared_pointer &pvArray)
{
    size_t length = pvArray->getLength();
    if (arrayCountFlag)
    {
        if (length<=0)
        {
            o << '0';
            return o;
        }
        o << length << separator;
    }

    bool first = true;
    for (size_t i = 0; i < length; i++) {
        if (first)
            first = false;
        else
            o << separator;

        pvArray->dumpValue(o, i);
    }
    return o;

    // avoid brackets
    /*
    o << *(pvArray.get());
    return o;
    */
}

std::ostream& terseStructureArray(std::ostream& o, PVStructureArray::const_shared_pointer const & pvArray)
{
    size_t length = pvArray->getLength();
    if (arrayCountFlag)
    {
        if (length<=0)
        {
            o << '0';
            return o;
        }
        o << length << separator;
    }

    PVStructureArray::const_svector data = pvArray->view();
    bool first = true;
    for (size_t i = 0; i < length; i++) {
        if (first)
            first = false;
        else
            o << separator;

        terseStructure(o, data[i]);
    }
    return o;
}

std::ostream& terseUnionArray(std::ostream& o, PVUnionArray::const_shared_pointer const & pvArray)
{
    size_t length = pvArray->getLength();
    if (arrayCountFlag)
    {
        if (length<=0)
        {
            o << '0';
            return o;
        }
        o << length << separator;
    }

    PVUnionArray::const_svector data = pvArray->view();
    bool first = true;
    for (size_t i = 0; i < length; i++) {
        if (first)
            first = false;
        else
            o << separator;

        terseUnion(o, data[i]);
    }
    return o;
}










/* Converts a hex character to its integer value */
char from_hex(char ch) {
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
char to_hex(char code) {
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(const char *str) {
    const char *pstr = str;
    char *buf = (char*)malloc(strlen(str) * 3 + 1), *pbuf = buf;
    bool firstEquals = true;
    while (*pstr) {
        if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~')
            *pbuf++ = *pstr;
        else if (*pstr == ' ')
            *pbuf++ = '+';
        else if (*pstr == '=' && firstEquals)
        {
            firstEquals = false;
            *pbuf++ = '=';
        }
        else
            *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), *pbuf++ = to_hex(*pstr & 15);
        pstr++;
    }
    *pbuf = '\0';
    return buf;
}

GetFieldRequesterImpl::GetFieldRequesterImpl(epics::pvAccess::Channel::shared_pointer channel) :
    m_channel(channel)
{

}

string GetFieldRequesterImpl::getRequesterName()
{
    return "GetFieldRequesterImpl";
}

void GetFieldRequesterImpl::getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field)
{
    if (status.isSuccess())
    {
        // show warning
        if (!status.isOK())
        {
            std::cerr << "[" << m_channel->getChannelName() << "] getField: " << dump_stack_only_on_debug(status) << std::endl;
        }

        // assign smart pointers
        {
            Lock lock(m_pointerMutex);
            m_field = field;
        }
    }
    else
    {
        std::cerr << "[" << m_channel->getChannelName() << "] failed to get channel introspection data: " << dump_stack_only_on_debug(status) << std::endl;
    }

    m_event.signal();
}

bool GetFieldRequesterImpl::waitUntilFieldGet(double timeOut)
{
    return m_event.wait(timeOut);
}

epics::pvData::FieldConstPtr GetFieldRequesterImpl::getField()
{
    Lock lock(m_pointerMutex);
    return m_field;
}



// TODO invalid characters check, etc.
bool URI::parse(const string& uri, URI& result)
{
    const string prot_end("://");
    string::const_iterator prot_i = search(uri.begin(), uri.end(),
                                           prot_end.begin(), prot_end.end());
    if( prot_i == uri.end() || prot_i == uri.begin() )
        return false;

    result.protocol.reserve(distance(uri.begin(), prot_i));
    transform(uri.begin(), prot_i,
              back_inserter(result.protocol),
              ::tolower); // protocol is icase

    advance(prot_i, prot_end.length());
    if ( prot_i == uri.end() )
        return false;

    string::const_iterator path_i = find(prot_i, uri.end(), '/');
    result.host.assign(prot_i, path_i);

    string::const_iterator fragment_i = find(path_i, uri.end(), '#');
    if ( fragment_i != uri.end() )
        result.fragment.assign(fragment_i+1, uri.end());

    string::const_iterator query_i = find(path_i, fragment_i, '?');
    result.path.assign(path_i, query_i);
    result.query_indicated = (query_i != fragment_i);
    if ( result.query_indicated )
        result.query.assign(++query_i, fragment_i);

    return true;
}

bool starts_with(const string& s1, const string& s2) {
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}
