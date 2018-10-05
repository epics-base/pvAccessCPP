/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>

#define epicsExportSharedSymbols
#include <pv/pvaVersion.h>
#include <pv/pvaConstants.h>

using std::stringstream;
using std::string;

namespace epics {
namespace pvAccess {

const std::string PVACCESS_DEFAULT_PROVIDER("local");
const std::string PVACCESS_ALL_PROVIDERS("<all>");
const std::string PVACCESS_DEBUG("EPICS_PVA_DEBUG");

Version::Version(std::string const & productName,
                 std::string const & implementationLangugage,
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

const std::string Version::getProductName() const {
    return _productName;
}

const std::string Version::getImplementationLanguage() const {
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

const string Version::getVersionString() const {
    stringstream ret;
    ret<<getProductName()<<" v"<<getMajorVersion()<<'.'<<getMinorVersion()<<'.'<<getMaintenanceVersion();
    if (isDevelopmentVersion())
        ret<<"-SNAPSHOT";

    return ret.str();
}

std::ostream& operator<<(std::ostream& o, const Version& v) {
    return o << v.getVersionString();
}

}
}
