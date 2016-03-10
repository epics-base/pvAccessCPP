#include <pv/event.h>
#include <pv/pvData.h>
#include <pv/pvAccess.h>

/// terse mode functions
void convertStructure(std::string* buffer, epics::pvData::PVStructure *data, int notFirst);
void convertArray(std::string*, epics::pvData::PVScalarArray * pv, int notFirst);
void convertStructureArray(std::string*, epics::pvData::PVStructureArray * pvdata, int notFirst);

void terseSeparator(char c);
void terseArrayCount(bool flag);
std::ostream& terse(std::ostream& o, epics::pvData::PVField::shared_pointer const & pv);
std::ostream& terseUnion(std::ostream& o, epics::pvData::PVUnion::shared_pointer const & pvUnion);
std::ostream& terseStructure(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvStructure);
std::ostream& terseScalarArray(std::ostream& o, epics::pvData::PVScalarArray::shared_pointer const & pvArray);
std::ostream& terseStructureArray(std::ostream& o, epics::pvData::PVStructureArray::shared_pointer const & pvArray);
std::ostream& terseUnionArray(std::ostream& o, epics::pvData::PVUnionArray::shared_pointer const & pvArray);

enum EnumMode { AutoEnum, NumberEnum, StringEnum };
void setEnumPrintMode(EnumMode mode);

void formatTTypes(bool flag);
bool isTType(epics::pvData::PVStructure::shared_pointer const & pvStructure);
bool formatTType(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvStructure);

void printUserTag(bool flag);

std::ostream& printEnumT(std::ostream& o, epics::pvData::PVStructure const & pvEnumT);
std::ostream& printEnumT(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvEnumT);
std::ostream& printTimeT(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvTimeT);

bool starts_with(const std::string& str, const std::string& part);

/* Converts a hex character to its integer value */
char from_hex(char ch);

/* Converts an integer value to its hex character*/
char to_hex(char code);

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(const char *str);

#include <string>

struct URI {
public:
    static bool parse(const std::string& uri, URI& result);
public:
    std::string protocol, host, path, query, fragment;
    bool query_indicated;
};

class RequesterImpl :
    public epics::pvData::Requester
{
public:
    RequesterImpl(std::string const & requesterName);
    virtual std::string getRequesterName();
    virtual void message(std::string const & message, epics::pvData::MessageType messageType);

private:
    std::string m_requesterName;
};

class ChannelRequesterImpl :
    public epics::pvAccess::ChannelRequester
{
private:
    epics::pvData::Event m_event;
    bool printOnlyErrors;
    bool showDisconnectMsg;

public:

    ChannelRequesterImpl(bool printOnlyErrors = false);

    virtual std::string getRequesterName();
    virtual void message(std::string const & message, epics::pvData::MessageType messageType);

    virtual void channelCreated(const epics::pvData::Status& status, epics::pvAccess::Channel::shared_pointer const & channel);
    virtual void channelStateChange(epics::pvAccess::Channel::shared_pointer const & channel, epics::pvAccess::Channel::ConnectionState connectionState);

    bool waitUntilConnected(double timeOut);
    void showDisconnectMessage(bool show = true);
};

class GetFieldRequesterImpl :
    public epics::pvAccess::GetFieldRequester
{
private:
    epics::pvAccess::Channel::shared_pointer m_channel;
    epics::pvData::FieldConstPtr m_field;
    epics::pvData::Event m_event;
    epics::pvData::Mutex m_pointerMutex;

public:

    GetFieldRequesterImpl(epics::pvAccess::Channel::shared_pointer channel);

    virtual std::string getRequesterName();
    virtual void message(std::string const & message, epics::pvData::MessageType messageType);

    virtual void getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field);

    epics::pvData::FieldConstPtr getField();

    bool waitUntilFieldGet(double timeOut);
};


struct dump_stack_only_on_debug
{
    const epics::pvData::Status &status;

    dump_stack_only_on_debug(const epics::pvData::Status &s) : status(s) {}
};

std::ostream& operator<<(std::ostream& os, const dump_stack_only_on_debug& d);




#include <ostream>
#include <iostream>

// usage: pvutil_ostream myos(std::cout.rdbuf());

class pvutil_ostream : private std::ostream
{
public:
    pvutil_ostream(std::streambuf* sb)
        : std::ostream(sb)
    {}

    template <typename T>
    friend pvutil_ostream& operator<<(pvutil_ostream&, const T&);

    friend pvutil_ostream& dumpPVStructure(pvutil_ostream&, const epics::pvData::PVStructure &, bool);

    // Additional overload to handle ostream specific io manipulators
    friend pvutil_ostream& operator<<(pvutil_ostream&, std::ostream& (*)(std::ostream&));

    // Accessor function to get a reference to the ostream
    std::ostream& get_ostream() {
        return *this;
    }
};


template <typename T>
inline pvutil_ostream&
operator<<(pvutil_ostream& out, const T& value)
{
    static_cast<std::ostream&>(out) << value;
    return out;
}

//  overload for std::ostream specific io manipulators
inline pvutil_ostream&
operator<<(pvutil_ostream& out, std::ostream& (*func)(std::ostream&))
{
    static_cast<std::ostream&>(out) << func;
    return out;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVField::shared_pointer & fieldField);

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVStructure & value);

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVUnion::shared_pointer & value);

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVStructure::shared_pointer & value)
{
    if (isTType(value))
    {
        o << epics::pvData::format::indent() << value->getStructure()->getID()
          << ' ' << value->getFieldName() << ' '; //" # ";
        formatTType(o, value);
        o << std::endl;
        //dumpPVStructure(o, *value, false);
        return o;
    }

    return o << *value;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVStructureArray::shared_pointer & value)
{
    o << epics::pvData::format::indent() << value->getStructureArray()->getID()
      << ' ' << value->getFieldName() << std::endl;
    size_t length = value->getLength();
    if (length > 0)
    {
        epics::pvData::format::indent_scope s(o);

        epics::pvData::PVStructureArray::const_svector data(value->view());
        for (size_t i = 0; i < length; i++)
            if (data[i].get() == NULL)
                o << epics::pvData::format::indent() << "(none)" << std::endl;
            else
                o << data[i];
    }

    return o;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVUnionArray::shared_pointer & value)
{
    o << epics::pvData::format::indent() << value->getUnionArray()->getID()
      << ' ' << value->getFieldName() << std::endl;
    size_t length = value->getLength();
    if (length > 0)
    {
        epics::pvData::format::indent_scope s(o);

        epics::pvData::PVUnionArray::const_svector data(value->view());
        for (size_t i = 0; i < length; i++)
            if (data[i].get() == NULL)
                o << epics::pvData::format::indent() << "(none)" << std::endl;
            else
                o << data[i];
    }

    return o;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVUnion::shared_pointer & value)
{
    o << epics::pvData::format::indent() << value->getUnion()->getID()
      << ' ' << value->getFieldName() << std::endl;
    {
        epics::pvData::format::indent_scope s(o);

        epics::pvData::PVFieldPtr fieldField = value->get();
        if (fieldField.get() == NULL)
            o << epics::pvData::format::indent() << "(none)" << std::endl;
        else
            o << fieldField;
    }
    return o;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVField::shared_pointer & fieldField)
{
    epics::pvData::Type type = fieldField->getField()->getType();
    if (type == epics::pvData::scalar || type == epics::pvData::scalarArray)
        o << epics::pvData::format::indent() << fieldField->getField()->getID() << ' ' << fieldField->getFieldName() << ' ' << *(fieldField.get()) << std::endl;
    else if (type == epics::pvData::structure)
        o << std::tr1::static_pointer_cast<epics::pvData::PVStructure>(fieldField);
    else if (type == epics::pvData::structureArray)
        o << std::tr1::static_pointer_cast<epics::pvData::PVStructureArray>(fieldField);
    else if (type == epics::pvData::union_)
        o << std::tr1::static_pointer_cast<epics::pvData::PVUnion>(fieldField);
    else if (type == epics::pvData::unionArray)
        o << std::tr1::static_pointer_cast<epics::pvData::PVUnionArray>(fieldField);
    else
        throw std::runtime_error("unsupported type");

    return o;
}

pvutil_ostream&
dumpPVStructure(pvutil_ostream& o, const epics::pvData::PVStructure & value, bool showHeader)
{
    if (showHeader)
    {
        std::string id = value.getStructure()->getID();
        o << epics::pvData::format::indent() << id << ' ' << value.getFieldName();
        o << std::endl;
    }

    {
        epics::pvData::format::indent_scope s(o);

        epics::pvData::PVFieldPtrArray const & fieldsData = value.getPVFields();
        if (fieldsData.size() != 0) {
            size_t length = value.getStructure()->getNumberFields();
            for(size_t i=0; i<length; i++) {
                o << fieldsData[i];
            }
        }
    }
    return o;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& o, const epics::pvData::PVStructure& value)
{
    return dumpPVStructure(o, value, true);
}

