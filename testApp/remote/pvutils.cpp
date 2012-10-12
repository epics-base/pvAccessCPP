#include "pvutils.h"

#include <iostream>

#include <string>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;
using namespace std::tr1;
using namespace epics::pvData;
using namespace epics::pvAccess;

RequesterImpl::RequesterImpl(String const & requesterName) :
		m_requesterName(requesterName)
{
}

String RequesterImpl::getRequesterName()
{
	return "RequesterImpl";
}

void RequesterImpl::message(String const & message, MessageType messageType)
{
	std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
}


char separator = ' ';
void terseSeparator(char c)
{
	separator = c;
}

char arrayCountFlag = true;
void terseArrayCount(bool flag)
{
	arrayCountFlag = flag;
}


std::ostream& terse(std::ostream& o, PVField::shared_pointer const & pv)
{
    Type type = pv->getField()->getType();
    switch (type)
    {
    case scalar:
    	o << *(pv.get());
    	return o;
    case structure:
    	return terseStructure(o, static_pointer_cast<PVStructure>(pv));
    	break;
    case scalarArray:
    	return terseScalarArray(o, static_pointer_cast<PVScalarArray>(pv));
    	break;
    case structureArray:
    	return terseStructureArray(o, static_pointer_cast<PVStructureArray>(pv));
    	break;
    default:
    	throw logic_error("unknown Field type: " + type);
    }
}

std::ostream& terseStructure(std::ostream& o, PVStructure::shared_pointer const & pvStructure)
{
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

std::ostream& terseScalarArray(std::ostream& o, PVScalarArray::shared_pointer const & pvArray)
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

std::ostream& terseStructureArray(std::ostream& o, PVStructureArray::shared_pointer const & pvArray)
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

    StructureArrayData data = StructureArrayData();
    pvArray->get(0, length, data);
    bool first = true;
    for (size_t i = 0; i < length; i++) {
		if (first)
			first = false;
		else
			o << separator;

		terseStructure(o, data.data[i]);
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



String ChannelRequesterImpl::getRequesterName()
{
	return "ChannelRequesterImpl";
}

void ChannelRequesterImpl::message(String const & message, MessageType messageType)
{
	std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
}

void ChannelRequesterImpl::channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
{
	if (status.isSuccess())
	{
		// show warning
		if (!status.isOK())
		{
			std::cerr << "[" << channel->getChannelName() << "] channel create: " << status.toString() << std::endl;
		}
	}
	else
	{
		std::cerr << "[" << channel->getChannelName() << "] failed to create a channel: " << status.toString() << std::endl;
	}
}

void ChannelRequesterImpl::channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState connectionState)
{
	if (connectionState == Channel::CONNECTED)
	{
		m_event.signal();
	}
	/*
	else if (connectionState != Channel::DESTROYED)
	{
		std::cerr << "[" << channel->getChannelName() << "] channel state change: "  << Channel::ConnectionStateNames[connectionState] << std::endl;
	}
	*/
}
    
bool ChannelRequesterImpl::waitUntilConnected(double timeOut)
{
	return m_event.wait(timeOut);
}



GetFieldRequesterImpl::GetFieldRequesterImpl(epics::pvAccess::Channel::shared_pointer channel) :
		m_channel(channel)
{

}

String GetFieldRequesterImpl::getRequesterName()
{
	return "GetFieldRequesterImpl";
}

void GetFieldRequesterImpl::message(String const & message, MessageType messageType)
{
	std::cerr << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
}

void GetFieldRequesterImpl::getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field)
{
	if (status.isSuccess())
	{
		// show warning
		if (!status.isOK())
		{
			std::cerr << "[" << m_channel->getChannelName() << "] getField create: " << status.toString() << std::endl;
		}

		// assign smart pointers
		{
		    Lock lock(m_pointerMutex);
		    m_field = field;
		}
	}
	else
	{
		// do not complain about missing field
		//std::cerr << "[" << m_channel->getChannelName() << "] failed to get channel introspection data: " << status.toString() << std::endl;
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
