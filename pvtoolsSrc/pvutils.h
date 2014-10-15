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

std::ostream& printEnumT(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvEnumT);
std::ostream& printTimeT(std::ostream& o, epics::pvData::PVStructure::shared_pointer const & pvTimeT);

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



/*
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

    // Additional overload to handle ostream specific io manipulators
    friend pvutil_ostream& operator<<(pvutil_ostream&, std::ostream& (*)(std::ostream&));

    // Accessor function to get a reference to the ostream
    std::ostream& get_ostream() { return *this; }
};


template <typename T>
inline pvutil_ostream&
operator<<(pvutil_ostream& out, const T& value)
{
    static_cast<std::ostream&>(out) << '.';
    static_cast<std::ostream&>(out) << value;
    return out;
}

//  overload for std::ostream specific io manipulators
inline pvutil_ostream&
operator<<(pvutil_ostream& out, std::ostream& (*func)(std::ostream&))
{
    static_cast<std::ostream&>(out) << '#';
    static_cast<std::ostream&>(out) << func;
    return out;
}

//  overload for PVField
template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& out, const epics::pvData::PVField& value)
{
    static_cast<std::ostream&>(out) << '?';
//    static_cast<std::ostream&>(out) << value;
    value.dumpValue(out);
    return out;
}

template <>
inline pvutil_ostream&
operator<<(pvutil_ostream& out, const epics::pvData::PVStructure& value)
{
    static_cast<std::ostream&>(out) << '!';
//    static_cast<std::ostream&>(out) << value;
    value.dumpValue(out);
    return out;
}
*/
