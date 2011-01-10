/*
 * configuration.cpp
 */

#include "configuration.h"
#include <epicsException.h>

namespace epics { namespace pvAccess {

using namespace epics::pvData;
using namespace std;

Properties::Properties()
{
	_fileName = "";
	_infile = new ifstream();
	_infile->exceptions (ifstream::failbit | ifstream::badbit );
	_outfile = new ofstream();
	_outfile->exceptions (ofstream::failbit | ofstream::badbit );
}

Properties::Properties(const string fileName)
{
	_fileName = fileName;
	_infile = new ifstream();
	_infile->exceptions (ifstream::failbit | ifstream::badbit );
	_outfile = new ofstream();
	_outfile->exceptions (ofstream::failbit | ofstream::badbit );
}

Properties::~Properties()
{
	delete _infile;
	delete _outfile;
	//clear map
	for(_propertiesIterator = _properties.begin() ;
			_propertiesIterator != _properties.end();
			_propertiesIterator++ )
	{
		delete [] _propertiesIterator->first;
		delete [] _propertiesIterator->second;
	}
	_properties.clear();
}

void Properties::setProperty(const string key,const  string value)
{
	string oldValue;
	_propertiesIterator = _properties.find(key.c_str());

	if(_propertiesIterator != _properties.end()) //found in map
	{
		delete[] _propertiesIterator->first;
		delete[] _propertiesIterator->second;
		_properties.erase(_propertiesIterator);
	}

	char* chKey = new char[key.length() + 1];
	strncpy(chKey,key.c_str(),key.length()+ 1);
	char* chValue = new char[value.length() + 1];
	strncpy(chValue,value.c_str(),value.length() + 1);
	_properties[chKey] = chValue;
}

string Properties::getProperty(const string key)
{
	_propertiesIterator = _properties.find(key.c_str());
	if(_propertiesIterator != _properties.end()) //found in map
	{
		return string(_propertiesIterator->second);
	}
	else
	{
		string errMsg = "Property not found in the map: " + key;
		throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
	}
}

string Properties::getProperty(const string key, const string defaultValue)
{
	_propertiesIterator = _properties.find(key.c_str());
	if(_propertiesIterator != _properties.end()) //found in map
	{
		return string(_propertiesIterator->second);
	}

	char* chKey = new char[key.length() + 1];
	strncpy(chKey,key.c_str(),key.length()+ 1);
	char* chValue = new char[defaultValue.length() + 1];
	strncpy(chValue,defaultValue.c_str(),defaultValue.length() + 1);
	_properties[chKey] = chValue;
	return defaultValue;
}

void Properties::load()
{
	for (_propertiesIterator = _properties.begin() ;
			_propertiesIterator != _properties.end();
			_propertiesIterator++ )
	{
		delete [] _propertiesIterator->first;
		delete [] _propertiesIterator->second;
	}
	_properties.clear();

	try
	{
		_infile->open(_fileName.c_str(),ifstream::in);
	}
	catch (ifstream::failure& e) {
		string errMsg = "Error opening file: " + string(_fileName.c_str());
		throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
	}

	string line;
	string property;
	string key;
	try
	{
		while(!_infile->eof())
		{
			line.clear();
			std::getline(*_infile,line);

			//remove trailing spaces
			truncate(line);

			//empty line
			if(line.length() == 0)
			{
				continue;
			}
			// comment
			if(line.at(0) == '#')
			{
				continue;
			}

			//line is in format: propertyName=propertyValue
			size_t pos = line.find_first_of('=',0);
			if(pos == string::npos) //bad value (= not found)
			{
				string errMsg = "Bad property line found: " + line;
				throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
			}

			key = line.substr(0,pos);
			truncate(key);
			property = line.substr(pos + 1,line.length());
			truncate(property);

			char* chKey = new char[key.length() + 1];
			strncpy(chKey,key.c_str(),key.length()+ 1);
			char* chProperty = new char[property.length() +1];
			strncpy(chProperty,property.c_str(),property.length() + 1);
			_properties[chKey] = chProperty;
		}
	}
	catch (ifstream::failure& e)
	{
		_infile->close();
		if(_infile->eof())
		{
			return; //end of file
		}
		string errMsg = "Error reading file: " + _fileName;
		throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
	}
	_infile->close();
}

void Properties::load(const string fileName)
{
	_fileName = fileName;
	load();
}

void Properties::store()
{
	try
	{
		_outfile->open(_fileName.c_str(),ifstream::trunc);
	}
	catch (ofstream::failure& e) {
		string errMsg = "Error opening file: " + string(_fileName.c_str());
		throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
	}


	for (_propertiesIterator = _properties.begin() ;
			_propertiesIterator != _properties.end();
			_propertiesIterator++ )
	{
		try
		{
			string line = string(_propertiesIterator->first) + string("=") + string(_propertiesIterator->second) + string("\n");
			_outfile->write(line.c_str(),line.length());
		}
		catch (ofstream::failure& e) {
			_outfile->close();
			string errMsg = "Error writing to file: " + string(_fileName.c_str());
			throw BaseException(errMsg.c_str(), __FILE__, __LINE__);
		}
	}
	_outfile->close();
}

void Properties::store(const string fileName)
{
	_fileName = fileName;
	store();
}

void Properties::list()
{
	for (_propertiesIterator = _properties.begin() ;
			_propertiesIterator != _properties.end();
			_propertiesIterator++ )
	{
		cout << "Key:" << _propertiesIterator->first << ",Value: " << _propertiesIterator->second << endl;
	}
}

SystemConfigurationImpl::SystemConfigurationImpl()
{
	_envParam.name = new char[MAX_NAME_LENGHT];
	_envParam.pdflt = NULL;
	_ibuffer.exceptions ( ifstream::failbit | ifstream::badbit );
	_obuffer.exceptions ( ifstream::failbit | ifstream::badbit );
	_properties = new Properties();
}

SystemConfigurationImpl::~SystemConfigurationImpl()
{
	if(_envParam.name) delete[] _envParam.name;
	if(_properties) delete _properties;
}

bool SystemConfigurationImpl::getPropertyAsBoolean(const string name, const bool defaultValue)
{
	bool retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name,_obuffer.str()));
	_ibuffer >> retval;
	return retval;
}

int32 SystemConfigurationImpl::getPropertyAsInteger(const string name, const int32 defaultValue)
{
	int32 retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	return retval;
}

float SystemConfigurationImpl::getPropertyAsFloat(const string name, const float defaultValue)
{
	float retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	return retval;
}

float SystemConfigurationImpl::getPropertyAsDouble(const string name, const double defaultValue)
{
	float retval;
	_ibuffer.clear();
	_obuffer.clear();
	_obuffer.str("");
	_obuffer << defaultValue;
	_ibuffer.str(getPropertyAsString(name, _obuffer.str()));
	_ibuffer >> retval;
	return retval;
}

string SystemConfigurationImpl::getPropertyAsString(const string name, const string defaultValue)
{
	strncpy(_envParam.name,name.c_str(),name.length() + 1);
	const char* val = envGetConfigParamPtr(&_envParam);
	if(val != NULL)
	{
		return _properties->getProperty(name, string(val));
	}
	return _properties->getProperty(name,defaultValue);
}

ConfigurationProviderImpl::ConfigurationProviderImpl()
{

}

ConfigurationProviderImpl::~ConfigurationProviderImpl()
{
	for(_configsIter = _configs.begin() ;
			_configsIter != _configs.end();
			_configsIter++ )
	{
		delete [] _configsIter->first;
	}
	_configs.clear();
}

void ConfigurationProviderImpl::registerConfiguration(const string name, const Configuration* configuration)
{
	Lock guard(&_mutex);
	_configsIter = _configs.find(name.c_str());
	if(_configsIter != _configs.end())
	{
		string msg = "configuration with name " + name + " already registered";
		throw BaseException(msg.c_str(), __FILE__, __LINE__);
	}
	char* chKey = new char[name.length() + 1];
	strncpy(chKey,name.c_str(),name.length()+ 1);
	_configs[chKey] = configuration;
}

Configuration* ConfigurationProviderImpl::getConfiguration(const string name)
{
	_configsIter = _configs.find(name.c_str());
	if(_configsIter != _configs.end())
	{
		return const_cast<Configuration*>(_configsIter->second);
	}
	return NULL;
}

ConfigurationProviderImpl* ConfigurationFactory::_configurationProvider = NULL;
Mutex ConfigurationFactory::_conf_factory_mutex = Mutex();

ConfigurationProviderImpl* ConfigurationFactory::getProvider()
{
	Lock guard(&_conf_factory_mutex);
	if(_configurationProvider == NULL)
	{
		_configurationProvider = new ConfigurationProviderImpl();
	}
	return _configurationProvider;
}

}}

