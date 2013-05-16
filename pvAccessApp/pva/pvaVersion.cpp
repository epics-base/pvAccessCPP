/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>

#include <pv/pvaVersion.h>

using std::stringstream;
using epics::pvData::String;

namespace epics {
namespace pvAccess {

Version::Version(epics::pvData::String const & productName,
		epics::pvData::String const & implementationLangugage,
        int majorVersion, int minorVersion,
        int maintenanceVersion, bool developmentFlag) :
    _productName(productName),
    _implementationLanguage(implementationLangugage),
    _majorVersion(majorVersion),
    _minorVersion(minorVersion),
    _maintenanceVersion(maintenanceVersion),
    _developmentFlag(developmentFlag)
{
}

const epics::pvData::String Version::getProductName() const {
    return _productName;
}

const epics::pvData::String Version::getImplementationLanguage() const {
    return _implementationLanguage;
}

int Version::getMajorVersion() const {
    return _majorVersion;
}

int Version::getMinorVersion() const {
    return _minorVersion;
}

int Version::getMaintenanceVersion() const {
    return _maintenanceVersion;
}

bool Version::isDevelopmentVersion() const {
    return _developmentFlag;
}

const String Version::getVersionString() const {
    stringstream ret;
    ret<<getProductName()<<" v"<<getMajorVersion()<<"."<<getMinorVersion();

    if (getMaintenanceVersion()>0)
    	ret<<'.'<<getMaintenanceVersion();

    if(isDevelopmentVersion())
        ret<<"-SNAPSHOT";

    return ret.str();
}

std::ostream& operator<<(std::ostream& o, const Version& v) {
	return o << v.getVersionString();
}

}
}
