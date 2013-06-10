/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/configuration.h>
#include <pv/epicsException.h>

#include <algorithm>

#if defined(__GNUC__) && __GNUC__ < 3
#define OLDGCC
#define NO_STREAM_EXCEPTIONS
#endif

namespace epics {
namespace pvAccess {

using namespace epics::pvData;
using namespace std;

Properties::Properties()
{
	_fileName = "";
	_infile.reset(new ifstream());
	_infile->exceptions (ifstream::failbit | ifstream::badbit );
	_outfile.reset(new ofstream());
	_outfile->exceptions (ofstream::failbit | ofstream::badbit );
}

Properties::Properties(const string &fileName)
{
	_fileName = fileName;
	_infile.reset(new ifstream());
	_infile->exceptions (ifstream::failbit | ifstream::badbit );
	_outfile.reset(new ofstream());
	_outfile->exceptions (ofstream::failbit | ofstream::badbit );
}

Properties::~Properties()
{
}

void Properties::setProperty(const string &key, const string &value)
{
	string oldValue;
	std::map<std::string,std::string>::iterator propertiesIterator = _properties.find(key);

	if(propertiesIterator != _properties.end()) //found in map
	{
		_properties.erase(propertiesIterator);
	}
	_properties[key] = value;
}

string Properties::getProperty(const string &key)
{
	std::map<std::string,std::string>::iterator propertiesIterator = _properties.find(key);
	if(propertiesIterator != _properties.end()) //found in map
	{
		return string(propertiesIterator->second);
	}
	else
	{
		string errMsg = "Property not found in the map: " + key;
		THROW_BASE_EXCEPTION(errMsg.c_str());
	}
}

string Properties::getProperty(const string &key, const string &defaultValue)
{
	std::map<std::string,std::string>::iterator propertiesIterator = _properties.find(key);
	if(propertiesIterator != _properties.end()) //found in map
	{
		return string(propertiesIterator->second);
	}

	_properties[key] = defaultValue;
	return defaultValue;
}

bool Properties::hasProperty(const string &key)
{
    return (_properties.find(key) != _properties.end());
}

void Properties::load()
{
	_properties.clear();

#ifdef NO_STREAM_EXCEPTIONS
	_infile->open(_fileName.c_str(),ifstream::in);
	if (_infile->fail())
#else
	try
	{
		_infile->open(_fileName.c_str(),ifstream::in);
	}
	catch (ifstream::failure& e)
#endif
	{
		string errMsg = "Error opening file: " + string(_fileName.c_str());
		THROW_BASE_EXCEPTION(errMsg.c_str());
	}

	string line;
	string property;
	string key;
#ifndef NO_STREAM_EXCEPTIONS
	try
	{
#endif
		while(!_infile->eof())
		{
			line.erase();
			std::getline(*_infile,line);

#ifdef NO_STREAM_EXCEPTIONS
			if (_infile->fail())
			{
				_infile->close();
				if(_infile->eof())
				{
					return; //end of file
				}
				string errMsg = "Error reading file: " + _fileName;
				THROW_BASE_EXCEPTION(errMsg.c_str());
			}
#endif

			//remove trailing spaces
			truncate(line);

			//empty line
			if(line.length() == 0)
			{
				continue;
			}
			// comment
			if(line[0] == '#')
			{
				continue;
			}

			//line is in format: propertyName=propertyValue
			size_t pos = line.find_first_of('=',0);
			if(pos == string::npos) //bad value (= not found)
			{
				string errMsg = "Bad property line found: " + line;
				THROW_BASE_EXCEPTION(errMsg.c_str());
			}

			key = line.substr(0,pos);
			truncate(key);
			property = line.substr(pos + 1,line.length());
			truncate(property);
			_properties[key] = property;
		}
#ifndef NO_STREAM_EXCEPTIONS
	}
	catch (ifstream::failure& e)
	{
		_infile->close();
		if(_infile->eof())
		{
			return; //end of file
		}
		string errMsg = "Error reading file: " + _fileName;
		THROW_BASE_EXCEPTION(errMsg.c_str());
	}
#endif
	_infile->close();
}

void Properties::load(const string &fileName)
{
	_fileName = fileName;
	load();
}

void Properties::store()
{
#ifdef NO_STREAM_EXCEPTIONS
	_outfile->open(_fileName.c_str(),ifstream::trunc);
	if (_outfile->fail())
#else
	try
	{
		_outfile->open(_fileName.c_str(),ifstream::trunc);
	}
	catch (ofstream::failure& e)
#endif
	{
		string errMsg = "Error opening file: " + string(_fileName.c_str());
		THROW_BASE_EXCEPTION(errMsg.c_str());
	}


	for (std::map<std::string,std::string>::iterator propertiesIterator = _properties.begin();
			propertiesIterator != _properties.end();
			propertiesIterator++ )
	{
#ifndef NO_STREAM_EXCEPTIONS
		try
		{
#endif
			string line = string(propertiesIterator->first) + string("=") + string(propertiesIterator->second) + string("\n");
			_outfile->write(line.c_str(),line.length());
#ifdef NO_STREAM_EXCEPTIONS
			if(_outfile->fail())
#else
		}
		catch (ofstream::failure& e)
#endif
		{
			_outfile->close();
			string errMsg = "Error writing to file: " + string(_fileName.c_str());
			THROW_BASE_EXCEPTION(errMsg.c_str());
		}
	}
	_outfile->close();
}

void Properties::store(const string &fileName)
{
	_fileName = fileName;
	store();
}

void Properties::list()
{
	for (std::map<std::string,std::string>::iterator propertiesIterator =  _properties.begin() ;
			propertiesIterator != _properties.end();
			propertiesIterator++ )
	{
		cout << "Key:" << propertiesIterator->first << ",Value: " << propertiesIterator->second << endl;
	}
}

SystemConfigurationImpl::SystemConfigurationImpl() :
		_properties(new Properties())
{
	_envParam.name = new char[256];
	_envParam.pdflt = NULL;
	// no exception, default value is taken
	//_ibuffer.exceptions ( ifstream::failbit | ifstream::badbit );
	//_obuffer.exceptions ( ifstream::failbit | ifstream::badbit );
}

SystemConfigurationImpl::~SystemConfigurationImpl()
{
	if(_envParam.name) delete[] _envParam.name;
}

bool SystemConfigurationImpl::getPropertyAsBoolean(const string &name, const bool defaultValue)
{
	/*
	bool retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name,_obuffer.str()));
	_ibuffer >> retval;
	if (_ibuffer.fail() || _ibuffer.bad())
        return defaultValue;
    else
        return retval;
        */

	string value = getPropertyAsString(name, defaultValue ? "1" : "0");
	std::transform(value.begin(), value.end(), value.begin(), ::tolower);

	bool isTrue = (value == "1") || (value == "true") || (value == "yes");
	bool isFalse = (value == "0") || (value == "false") || (value == "no");

	// invalid value
	if (!(isTrue || isFalse))
		return defaultValue;
	else
		return isTrue == true;
}

int32 SystemConfigurationImpl::getPropertyAsInteger(const string &name, const int32 defaultValue)
{
	int32 retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	if (_ibuffer.fail() || _ibuffer.bad())
        return defaultValue;
    else
        return retval;
}

float SystemConfigurationImpl::getPropertyAsFloat(const string &name, const float defaultValue)
{
	float retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	if (_ibuffer.fail() || _ibuffer.bad())
        return defaultValue;
    else
        return retval;
}

float SystemConfigurationImpl::getPropertyAsDouble(const string &name, const double defaultValue)
{
	float retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	if (_ibuffer.fail() || _ibuffer.bad())
        return defaultValue;
    else
        return retval;
}

string SystemConfigurationImpl::getPropertyAsString(const string &name, const string &defaultValue)
{
	strncpy(_envParam.name,name.c_str(),name.length() + 1);
	const char* val = envGetConfigParamPtr(&_envParam);
	if(val != NULL)
	{
		return _properties->getProperty(name, string(val));
	}
	return _properties->getProperty(name,defaultValue);
}

bool SystemConfigurationImpl::hasProperty(const string &key)
{
    return _properties->hasProperty(key);
}

ConfigurationProviderImpl::ConfigurationProviderImpl()
{

}

ConfigurationProviderImpl::~ConfigurationProviderImpl()
{
}

void ConfigurationProviderImpl::registerConfiguration(const string &name, Configuration::shared_pointer const & configuration)
{
	Lock guard(_mutex);
	std::map<std::string,Configuration::shared_pointer>::iterator configsIter = _configs.find(name);
	if(configsIter != _configs.end())
	{
		string msg = "configuration with name " + name + " already registered";
		THROW_BASE_EXCEPTION(msg.c_str());
	}
	_configs[name] = configuration;
}

Configuration::shared_pointer ConfigurationProviderImpl::getConfiguration(const string &name)
{
	std::map<std::string,Configuration::shared_pointer>::iterator configsIter = _configs.find(name);
	if(configsIter != _configs.end())
	{
		return configsIter->second;
	}
	return Configuration::shared_pointer();
}

ConfigurationProvider::shared_pointer configurationProvider;
Mutex conf_factory_mutex;

ConfigurationProvider::shared_pointer ConfigurationFactory::getProvider()
{
	Lock guard(conf_factory_mutex);
	if(configurationProvider.get() == NULL)
	{
		configurationProvider.reset(new ConfigurationProviderImpl());
		// default
		Configuration::shared_pointer systemConfig(new SystemConfigurationImpl());
		configurationProvider->registerConfiguration("system", systemConfig);
	}
	return configurationProvider;
}

}}

