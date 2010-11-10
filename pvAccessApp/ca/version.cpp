/*
 * version.cpp
 *
 *  Created on: Oct 8, 2010
 *      Author: Miha Vitorovic
 */

#include "version.h"

namespace epics {
    namespace pvAccess {

        using epics::pvData::String;

        const String Version::getLongVersionString() const {
            String ret;
            ret += getProductName();
            ret += " [";
            ret += getImplementationLanguage();
            ret += "] v";
            ret += getMajorVersion();
            ret += ".";
            ret += getMinorVersion();
            ret += ".";
            if(getDevelopmentVersion()>0) {
                ret += "D";
                ret += getDevelopmentVersion();
            } else
                ret += getMaintenanceVersion();

            return ret;
        }

        const String Version::getVersionString() const {
            String ret;
            ret += getProductName();
            ret += " v";
            ret += getMajorVersion();
            ret += ".";
            ret += getMinorVersion();
            ret += ".";
            if(getDevelopmentVersion()>0) {
                ret += "D";
                ret += getDevelopmentVersion();
            } else
                ret += getMaintenanceVersion();

            return ret;
        }

    }
}
