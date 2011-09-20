/*
 * configurationTest.cpp
 *
 */

#include <pv/configuration.h>
#include <pv/CDRMonitor.h>

#include <epicsAssert.h>
#include <epicsExit.h>
#include <iostream>
#include <string>
#include <stdlib.h>

#ifdef _WIN32
void setenv(char * a, char * b, int c)
{
    char buf[1024]; 
    sprintf(buf, "%s=%s", a, b); 
    _putenv(buf);
}
#endif

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

int main(int argc, char *argv[])
{
	SystemConfigurationImpl* configuration = new SystemConfigurationImpl();
	bool boolProperty = configuration->getPropertyAsBoolean("boolProperty", true);
	assert(boolProperty == true);

	int32 intProperty = configuration->getPropertyAsInteger("intProperty", 1);
	assert(intProperty == 1);

	float floatProperty = configuration->getPropertyAsFloat("floatProperty", 3);
	assert(floatProperty == 3);

	double doubleProperty = configuration->getPropertyAsDouble("doubleProperty", -3);
	assert(doubleProperty == -3);

	string stringProperty = configuration->getPropertyAsString("stringProperty", "string");
	assert(stringProperty == string("string"));

	ConfigurationProviderImpl* configProvider = ConfigurationFactory::getProvider();
	configProvider->registerConfiguration("conf1",static_cast<Configuration*>(configuration));

	SystemConfigurationImpl* configurationOut = static_cast<SystemConfigurationImpl*>(configProvider->getConfiguration("conf1"));
	assert(configurationOut == configuration);

	intProperty = configuration->getPropertyAsInteger("intProperty", 2);
	assert(intProperty == 1);

	floatProperty = configuration->getPropertyAsFloat("floatProperty", 4);
	assert(floatProperty == 3);

	doubleProperty = configuration->getPropertyAsDouble("doubleProperty", -4);
	assert(doubleProperty == -3);

	stringProperty = configuration->getPropertyAsString("stringProperty", "string1");
	assert(stringProperty == string("string"));

	setenv("boolProperty1", "1", 1);
	boolProperty = configuration->getPropertyAsInteger("boolProperty1", 0);
	assert(boolProperty == true);

	setenv("intProperty1", "45", 1);
	intProperty = configuration->getPropertyAsInteger("intProperty1", 2);
	assert(intProperty == 45);

	setenv("floatProperty1", "22", 1);
	floatProperty = configuration->getPropertyAsFloat("floatProperty1", 3);
	assert(floatProperty == 22);

	setenv("dobuleProperty1", "42", 1);
	doubleProperty = configuration->getPropertyAsDouble("dobuleProperty1", -3);
	assert(doubleProperty == 42);

	if(configProvider) delete configProvider;
        epicsExitCallAtExits();
        CDRMonitor::get().show(stdout, true);
	return 0;
}



