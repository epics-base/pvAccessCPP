/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

#include <pv/epicsException.h>
#include <pv/typeCast.h>

#include <osiSock.h>
#include <epicsStdlib.h>

#define epicsExportSharedSymbols
#include <pv/configuration.h>

#if defined(__GNUC__) && __GNUC__ < 3
#define OLDGCC
#define NO_STREAM_EXCEPTIONS
#endif

namespace epics {
namespace pvAccess {

using namespace epics::pvData;
using namespace std;

Configuration::~Configuration() {}

bool Configuration::getPropertyAsBoolean(const std::string &name, const bool defaultValue) const
{
    string value = getPropertyAsString(name, defaultValue ? "1" : "0");
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    bool isTrue = (value == "1") || (value == "true") || (value == "yes");
    if (isTrue)
        return true;

    bool isFalse = (value == "0") || (value == "false") || (value == "no");
    if (isFalse)
        return false;

    // invalid value
    return defaultValue;
}

epics::pvData::int32 Configuration::getPropertyAsInteger(const std::string &name, const epics::pvData::int32 defaultValue) const
{
    try {
        return castUnsafe<epics::pvData::int32>(getPropertyAsString(name, ""));
    } catch(std::runtime_error&) {
        return defaultValue;
    }
}

float Configuration::getPropertyAsFloat(const std::string &name, const float defaultValue) const
{
    try {
        return castUnsafe<float>(getPropertyAsString(name, ""));
    } catch(std::runtime_error&) {
        return defaultValue;
    }
}

double Configuration::getPropertyAsDouble(const std::string &name, const double defaultValue) const
{
    try {
        return castUnsafe<double>(getPropertyAsString(name, ""));
    } catch(std::runtime_error&) {
        return defaultValue;
    }
}

std::string Configuration::getPropertyAsString(const std::string &name, const std::string &defaultValue) const
{
    std::string val;
    if(tryGetPropertyAsString(name, &val))
        return val;
    else
        return defaultValue;
}

bool Configuration::getPropertyAsAddress(const std::string& name, osiSockAddr* addr) const
{
    unsigned short dftport=0;
    if(addr->sa.sa_family==AF_INET)
        dftport = ntohs(addr->ia.sin_port);

    std::string val(getPropertyAsString(name, ""));

    if(val.empty()) return false;

    memset(addr, 0, sizeof(*addr));
    addr->ia.sin_family = AF_INET;
    if(aToIPAddr(val.c_str(), dftport, &addr->ia))
        return false;
    return true;
}

bool Configuration::hasProperty(const std::string &name) const
{
    return tryGetPropertyAsString(name, NULL);
}

bool ConfigurationMap::tryGetPropertyAsString(const std::string& name, std::string* val) const
{
    properties_t::const_iterator it = properties.find(name);
    if(it==properties.end())
        return false;
    if(val)
        *val = it->second;
    return true;
}

void ConfigurationMap::addKeys(keys_t& names) const
{
    for(properties_t::const_iterator it=properties.begin(); it!=properties.end(); ++it)
        names.insert(it->first);
}

static
const ENV_PARAM * findEnvConfigParam( const char * envVarName )
{
    const ENV_PARAM **ppParam = env_param_list;

    /* Find a match with one of the EPICS env vars */
    while (*ppParam) {
        if ( strcmp( envVarName, (*ppParam)->name ) == 0 ) {
            return *ppParam;
        }
    ppParam++;
    }

    return 0;
}


bool ConfigurationEnviron::tryGetPropertyAsString(const std::string& name, std::string* val) const
{
    const char *env = getenv(name.c_str());
    if(!env) {
        const ENV_PARAM *pParam = findEnvConfigParam( name.c_str() );
        if ( pParam )
            env = pParam->pdflt;
    }
    if(!env || !*env)
        return false;
    if(val)
        *val = env;
    return true;
}

bool ConfigurationStack::tryGetPropertyAsString(const std::string& name, std::string* val) const
{
    for(confs_t::const_reverse_iterator it = confs.rbegin(), end = confs.rend();
            it!=end; ++it)
    {
        Configuration& conf = **it;
        if(conf.tryGetPropertyAsString(name, val))
            return true;
    }
    return false;
}

void ConfigurationStack::addKeys(keys_t& names) const
{
    for(confs_t::const_iterator it=confs.begin(); it!=confs.end(); ++it)
        (*it)->addKeys(names);
}

ConfigurationBuilder::ConfigurationBuilder() :stack(new ConfigurationStack) {}

ConfigurationBuilder& ConfigurationBuilder::push_env()
{
    Configuration::shared_pointer env(new ConfigurationEnviron);
    stack->push_back(env);
    return *this;
}

ConfigurationBuilder& ConfigurationBuilder::push_map()
{
    Configuration::shared_pointer env(new ConfigurationMap(mymap));
    stack->push_back(env);
    mymap.clear();
    return *this;
}

ConfigurationBuilder&
ConfigurationBuilder::push_config(const Configuration::shared_pointer& conf)
{
    stack->push_back(conf);
    return *this;
}

ConfigurationBuilder&
ConfigurationBuilder::_add(const std::string& name, const std::string& val)
{
    if(name.find_first_of(" \t\r\n")!=name.npos)
        THROW_EXCEPTION2(std::invalid_argument, "Key name may not contain whitespace");
    mymap[name] = val;
    return *this;
}

Configuration::shared_pointer ConfigurationBuilder::build()
{
    if(!mymap.empty())
        THROW_EXCEPTION2(std::logic_error, "Missing call to .push_map()");
    if(stack->size()==0) {
        return Configuration::shared_pointer(new ConfigurationMap); // empty map
    } else if(stack->size()==1) {
        return stack->pop_back();
    } else {
        return stack;
    }
}

void ConfigurationProviderImpl::registerConfiguration(const string &name, Configuration::shared_pointer const & configuration)
{
    Lock guard(_mutex);
    std::map<std::string,Configuration::shared_pointer>::iterator configsIter = _configs.find(name);
    if(configsIter != _configs.end())
    {
        string msg = "configuration with name " + name + " already registered";
        THROW_BASE_EXCEPTION(msg);
    }
    _configs[name] = configuration;
}

Configuration::shared_pointer ConfigurationProviderImpl::getConfiguration(const string &name)
{
    Lock guard(_mutex);
    std::map<std::string,Configuration::shared_pointer>::iterator configsIter = _configs.find(name);
    if(configsIter != _configs.end())
    {
        return configsIter->second;
    }
    Configuration::shared_pointer env(new ConfigurationEnviron); // default to environment only
    _configs[name] = env; // ensure that a later attempt to define this config will fail
    return env;
}

ConfigurationProvider::shared_pointer configurationProvider;
Mutex conf_factory_mutex;

ConfigurationProvider::shared_pointer ConfigurationFactory::getProvider()
{
    Lock guard(conf_factory_mutex);
    if(configurationProvider.get() == NULL)
    {
        configurationProvider.reset(new ConfigurationProviderImpl());
        Configuration::shared_pointer systemConfig(new ConfigurationEnviron);
        configurationProvider->registerConfiguration("system", systemConfig);
    }
    return configurationProvider;
}

}
}

