/*
 * version.cpp
 *
 *  Created on: Oct 8, 2010
 *      Author: Miha Vitorovic
 */

#include <sstream>

#include <pv/version.h>

using std::stringstream;
using epics::pvData::String;

namespace epics {
    namespace pvAccess {


        const String Version::getLongVersionString() const {
            stringstream ret;
            ret<<getProductName()<<" ["<<getImplementationLanguage();
            ret<<"] v"<<getMajorVersion()<<"."<<getMinorVersion()<<".";
            if(getDevelopmentVersion()>0) {
                ret<<"D"<<getDevelopmentVersion();
            } else
                ret<<getMaintenanceVersion();

            return ret.str();
        }

        const String Version::getVersionString() const {
            stringstream ret;
            ret<<getProductName()<<" v"<<getMajorVersion()<<".";
            ret<<getMinorVersion()<<".";
            if(getDevelopmentVersion()>0) {
                ret<<"D"<<getDevelopmentVersion();
            } else
                ret<<getMaintenanceVersion();

            return ret.str();
        }

    }
}
