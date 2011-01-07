/*
 * configuration.h
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "pvType.h"
#include "noDefaultMethods.h"
#include "lock.h"
#include "epicsException.h"

#include "envDefs.h"


#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include <map>


using namespace epics::pvData;
using namespace std;

namespace epics { namespace pvAccess {

#define MAX_NAME_LENGHT 300

struct conf_cmp_str
{
	bool operator()(char const *a, char const *b)
	{
		return strcmp(a, b) < 0;
	}
};

/**
 * Properties
 */
class Properties
{
public:
	Properties();
	Properties(const string fileName);
	virtual ~Properties();

	void setProperty(const string key,const  string value);
	string getProperty(const string key);
	string getProperty(const string key, const string defaultValue);

	void store();
	void store(const string fileName);
	void load();
	void load(const string fileName);
	void list();

private:
	map<const char*,const char* , conf_cmp_str> _properties;
	map<const char*,const char* , conf_cmp_str>::iterator _propertiesIterator;
	ifstream* _infile;
	ofstream* _outfile;
	string _fileName;

	inline void	truncate(string& str)
	{
		while(str.length() != 0 && (str.at(0) == ' ' || str.at(0) == '\t'))
		{
			str.erase(0,1);
		}
		while(str.length() != 0 && (str.at(str.length()-1) == ' ' || str.at(str.length()-1) == '\t'))
		{
			str.erase(str.length()-1,1);
		}
	}
};



/**
 * Configuration
 */
class Configuration : private NoDefaultMethods
{
public:
	/*
	 * Get the environment variable specified by name or return default value
	 * if it does not exist.
	 *
	 * @param name name of the environment variable to return.
	 * @param defualtValue default value to return if environment variable does not exists.
	 *
	 * @return environment variable value as bool or default value if it does not exist.
	 */
	virtual bool getPropertyAsBoolean(const string name, const bool defaultValue) = 0;
	/*
	 * Get the environment variable specified by name or return default value
	 * if it does not exist.
	 *
	 * @param name name of the environment variable to return.
	 * @param defualtValue default value to return if environment variable does not exists.
	 *
	 * @return environment variable value as int32 or default value if it does not exist.
	 */
	virtual int32 getPropertyAsInteger(const string name, const int32 defaultValue) = 0;
	/*
	 * Get the environment variable specified by name or return default value
	 * if it does not exist.
	 *
	 * @param name name of the environment variable to return.
	 * @param defualtValue default value to return if environment variable does not exists.
	 *
	 * @return environment variable value as float or default value if it does not exist.
	 */
	virtual float getPropertyAsFloat(const string name, const float defaultValue) = 0;
	/*
	 * Get the environment variable specified by name or return default value
	 * if it does not exist.
	 *
	 * @param name name of the environment variable to return.
	 * @param defualtValue default value to return if environment variable does not exists.
	 *
	 * @return environment variable value as double or default value if it does not exist.
	 */
	virtual float getPropertyAsDouble(const string name, const double defaultValue) = 0;
	/*
	 * Get the environment variable specified by name or return default value
	 * if it does not exist.
	 *
	 * @param name name of the environment variable to return.
	 * @param defualtValue default value to return if environment variable does not exists.
	 *
	 * @return environment variable value as string or default value if it does not exist.
	 */
	virtual string getPropertyAsString(const string name, const string defaultValue) = 0;
};

class SystemConfigurationImpl: public Configuration
{
public:
	SystemConfigurationImpl();
	virtual ~SystemConfigurationImpl();
	bool getPropertyAsBoolean(const string name, const bool defaultValue);
	int32 getPropertyAsInteger(const string name, const int32 defaultValue);
	float getPropertyAsFloat(const string name, const float defaultValue);
	float getPropertyAsDouble(const string name, const double defaultValue);
	string getPropertyAsString(const string name, string defaultValue);
	Properties* _properties;
private:
	ENV_PARAM _envParam;
	istringstream _ibuffer;
	ostringstream _obuffer;

};

/**
 * Configuration provider.
 */
class ConfigurationProvider : private NoDefaultMethods
{
public:
	/*
	 * Return configuration specified by name.
	 *
	 * @param name name of the configuration to return.
	 *
	 * @return configuration specified by name or NULL if it does not exists.
	 */
	virtual Configuration* getConfiguration(const string name) = 0;
	/*
	 * Register configuration.
	 *
	 * @param name name of the configuration to register.
	 * @param configuration configuration to register.
	 */
	virtual void registerConfiguration(const string name, const Configuration* configuration) = 0;
};

class ConfigurationProviderImpl: public ConfigurationProvider
{
public:
	ConfigurationProviderImpl();
	virtual ~ConfigurationProviderImpl();
	Configuration* getConfiguration(const string name);
	void registerConfiguration(const string name, const Configuration* configuration);
private:
	Mutex _mutex;
	map<const char*,const Configuration*, conf_cmp_str> _configs;
	map<const char*,const Configuration*, conf_cmp_str>::iterator _configsIter;
};

/**
 * Configuration factory.
 */
class ConfigurationFactory : private NoDefaultMethods
{
public:
	/*
	 * Lazily creates configuration provider.
	 *
	 * @param name name of the configuration to register.
	 * @param configuration configuration to register.
	 *
	 * @return configuration provider
	 */
	static ConfigurationProviderImpl* getProvider();

private:
	ConfigurationFactory() {};
	static ConfigurationProviderImpl* _configurationProvider;
	static Mutex _conf_factory_mutex;
};

}}

#endif  /* CONFIGURATION_H */
