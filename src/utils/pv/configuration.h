/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <set>

#ifdef epicsExportSharedSymbols
#   define configurationEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvType.h>
#include <pv/noDefaultMethods.h>
#include <pv/lock.h>
#include <pv/sharedPtr.h>

#include <envDefs.h>
#ifdef configurationEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   undef configurationEpicsExportSharedSymbols
#endif

#include <shareLib.h>

union osiSockAddr; // defined in osiSock;

namespace epics {
namespace pvAccess {

class ConfigurationStack;

/**
 * Configuration
 */
class epicsShareClass Configuration
{
    EPICS_NOT_COPYABLE(Configuration)
public:
    POINTER_DEFINITIONS(Configuration);

    Configuration() {}
    virtual ~Configuration() =0;
    /**
     * Get the environment variable specified by name or return default value
     * if it does not exist.
     *
     * @param name name of the environment variable to return.
     * @param defualtValue default value to return if environment variable does not exists.
     *
     * @return environment variable value as bool or default value if it does not exist.
     */
    bool getPropertyAsBoolean(const std::string &name, const bool defaultValue) const;
    /**
     * Get the environment variable specified by name or return default value
     * if it does not exist.
     *
     * @param name name of the environment variable to return.
     * @param defualtValue default value to return if environment variable does not exists.
     *
     * @return environment variable value as int32 or default value if it does not exist.
     */
    epics::pvData::int32 getPropertyAsInteger(const std::string &name, const epics::pvData::int32 defaultValue) const;
    /**
     * Get the environment variable specified by name or return default value
     * if it does not exist.
     *
     * @param name name of the environment variable to return.
     * @param defualtValue default value to return if environment variable does not exists.
     *
     * @return environment variable value as float or default value if it does not exist.
     */
    float getPropertyAsFloat(const std::string &name, const float defaultValue) const;
    /**
     * Get the environment variable specified by name or return default value
     * if it does not exist.
     *
     * @param name name of the environment variable to return.
     * @param defualtValue default value to return if environment variable does not exists.
     *
     * @return environment variable value as double or default value if it does not exist.
     */
    double getPropertyAsDouble(const std::string &name, const double defaultValue) const;
    /**
     * Get the environment variable specified by name or return default value
     * if it does not exist.
     *
     * @param name name of the environment variable to return.
     * @param defualtValue default value to return if environment variable does not exists.
     *
     * @return environment variable value as std::string or default value if it does not exist.
     */
    std::string getPropertyAsString(const std::string &name, const std::string &defaultValue) const;
    /**
    * Fetch and parse as a socket address and port number (address family set accordingly).
    * At present only numeric addresses are parsed (eg. "127.0.0.1:4242").
    *
    * The storage pointed to be addr should be initialized with a default value, or zeroed.
    *
    * @param name name of the environment variable to return.
    * @pram addr pointer to the address struct to be filled in
    * @return true if addr now contains an address, false otherwise
    */
    bool getPropertyAsAddress(const std::string& name, osiSockAddr* addr) const;

    bool hasProperty(const std::string &name) const;

    typedef std::set<std::string> keys_t;
    /** Return a (partial) list of available key names.
     *  Does not include key names from the ConfigurationEnviron
     */
    keys_t keys() const
    {
        keys_t ret;
        addKeys(ret);
        return ret;
    }

protected:
    friend class ConfigurationStack;
    virtual bool tryGetPropertyAsString(const std::string& name, std::string* val) const = 0;
    virtual void addKeys(keys_t&) const {}
};

//! Lookup configuration strings from an in memory store
class epicsShareClass ConfigurationMap: public Configuration
{
    EPICS_NOT_COPYABLE(ConfigurationMap)
public:
    typedef std::map<std::string, std::string> properties_t;
    properties_t properties;
    ConfigurationMap() {}
    ConfigurationMap(const properties_t& p) :properties(p) {}
    virtual ~ConfigurationMap() {}
private:
    virtual bool tryGetPropertyAsString(const std::string& name, std::string* val) const;
    virtual void addKeys(keys_t&) const;
};

//! Lookup configuration strings from the process environment
class epicsShareClass ConfigurationEnviron: public Configuration
{
    EPICS_NOT_COPYABLE(ConfigurationEnviron)
public:
    ConfigurationEnviron() {}
    virtual ~ConfigurationEnviron() {}
private:
    virtual bool tryGetPropertyAsString(const std::string& name, std::string* val) const;
};

typedef ConfigurationEnviron SystemConfigurationImpl;

//! Lookup configuration strings from a heap of sub-Configurations.
//! Most recently push'd is checked first.
class epicsShareClass ConfigurationStack : public Configuration
{
    EPICS_NOT_COPYABLE(ConfigurationStack)
    typedef std::vector<std::tr1::shared_ptr<Configuration> > confs_t;
    confs_t confs;
    virtual bool tryGetPropertyAsString(const std::string& name, std::string* val) const;
    virtual void addKeys(keys_t&) const;
public:
    ConfigurationStack() {}
    virtual ~ConfigurationStack() {}
    inline void push_back(const confs_t::value_type& conf) {
        confs.push_back(conf);
    }
    inline confs_t::value_type pop_back() {
        if(confs.empty())
            throw std::runtime_error("Stack empty");
        confs_t::value_type ret(confs.back());
        confs.pop_back();
        return ret;
    }
    inline size_t size() const {
        return confs.size();
    }
};

struct epicsShareClass ConfigurationBuilder
{
    ConfigurationBuilder();
    ConfigurationBuilder& push_env();
    ConfigurationBuilder& push_map();
    ConfigurationBuilder& push_config(const Configuration::shared_pointer&);
    template<typename V>
    ConfigurationBuilder& add(const std::string& name, const V& val)
    {
        std::ostringstream strm;
        strm<<val;
        return _add(name, strm.str());
    }
    Configuration::shared_pointer build();
private:
    ConfigurationBuilder& _add(const std::string& name, const std::string& val);
    ConfigurationMap::properties_t mymap;
    std::tr1::shared_ptr<ConfigurationStack> stack;
    friend ConfigurationBuilder& operator<<(ConfigurationBuilder&, const std::string& s);
};

/**
 * Configuration provider.
 */
class epicsShareClass ConfigurationProvider
{
    EPICS_NOT_COPYABLE(ConfigurationProvider)
public:
    POINTER_DEFINITIONS(ConfigurationProvider);
    ConfigurationProvider() {}
    virtual ~ConfigurationProvider() {}
    /**
     * Return configuration specified by name.
     *
     * @param name name of the configuration to return.
     *
     * @return configuration specified by name or NULL if it does not exists.
     */
    virtual Configuration::shared_pointer getConfiguration(const std::string &name) = 0;
    /**
     * Register configuration.
     *
     * @param name name of the configuration to register.
     * @param configuration configuration to register.
     */
    virtual void registerConfiguration(const std::string &name, Configuration::shared_pointer const & configuration) = 0;
};

class ConfigurationProviderImpl: public ConfigurationProvider
{
    EPICS_NOT_COPYABLE(ConfigurationProviderImpl)
public:
    ConfigurationProviderImpl() {}
    /**
     * Destructor. Note: Registered configurations will be deleted!!
     */
    virtual ~ConfigurationProviderImpl() {}
    Configuration::shared_pointer getConfiguration(const std::string &name);
    void registerConfiguration(const std::string &name, Configuration::shared_pointer const & configuration);
private:
    epics::pvData::Mutex _mutex;
    std::map<std::string,Configuration::shared_pointer> _configs;
};

/**
 * Configuration factory.
 */
class epicsShareClass ConfigurationFactory
{
    EPICS_NOT_COPYABLE(ConfigurationFactory)
public:
    POINTER_DEFINITIONS(ConfigurationFactory);

    /**
     * Lazily creates configuration provider.
     *
     * @param name name of the configuration to register.
     * @param configuration configuration to register.
     *
     * @return configuration provider
     */
    static ConfigurationProvider::shared_pointer getProvider();
    static void registerConfiguration(const std::string &name, Configuration::shared_pointer const & configuration)
    {
        getProvider()->registerConfiguration(name, configuration);
    }
    static Configuration::shared_pointer getConfiguration(const std::string& name)
    {
        return getProvider()->getConfiguration(name);
    }

private:
    ConfigurationFactory() {};
};

}
}

#endif  /* CONFIGURATION_H */
