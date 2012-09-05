/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef VERSION_H_
#define VERSION_H_

#include <pv/pvType.h>
#include <pv/noDefaultMethods.h>

namespace epics {
namespace pvAccess {

        class Version : public epics::pvData::NoDefaultMethods {
        public:
            /**
             * Default constructor.
             * @param productName	product name.
             * @param implementationLangugage	implementation language.
             * @param majorVersion	major version.
             * @param minorVersion	minor version.
             * @param maintenanceVersion	maintenance version.
             * @param developmentFlag	development indicator flag.
             */
            Version(epics::pvData::String const & productName,
            		epics::pvData::String const & implementationLangugage,
                    int majorVersion, int minorVersion,
                    int maintenanceVersion, bool developmentFlag);

            /** The name of the product */
            const epics::pvData::String getProductName() const;

            /** Implementation Language: C++
             */
            const epics::pvData::String getImplementationLanguage() const;

            /**
             * Major version number. This changes only when there is a
             * significant, externally apparent enhancement from the previous release.
             * 'n' represents the n'th version.
             *
             * Clients should carefully consider the implications of new versions as
             * external interfaces and behaviour may have changed.
             */
            int getMajorVersion() const;

            /**
             * Minor version number. This changes when:
             * <ul>
             * <li>a new set of functionality is to be added</li>
             * <li>API or behaviour change</li>
             * <li>its designated as a reference release</li>
             * </ul>
             */
            int getMinorVersion() const;

            /**
             * Maintenance version number. Optional identifier used to designate
             * maintenance drop applied to a specific release and contains fixes for
             * defects reported. It maintains compatibility with the release and
             * contains no API changes. When missing, it designates the final and
             * complete development drop for a release.
             */
            int getMaintenanceVersion() const;

            /**
             * Development flag.
             *
             * Development drops are works in progress towards a completed, final
             * release. A specific development drop may not completely implement all
             * aspects of a new feature, which may take several development drops to
             * complete. At the point of the final drop for the release, the -SNAPSHOT suffix
             * will be omitted.
             */
            bool isDevelopmentVersion() const;

            /**
             * Get the long version string.
             * @return epics::pvData::String denoting current version
             */
            const epics::pvData::String getLongVersionString() const;

            /**
             * Get the basic version string.
             * @return epics::pvData::String denoting current version
             */
            const epics::pvData::String getVersionString() const;

        private:
            epics::pvData::String _productName;
            epics::pvData::String _implementationLanguage;
            int _majorVersion;
            int _minorVersion;
            int _maintenanceVersion;
            bool _developmentFlag;
        };

        std::ostream& operator<<(std::ostream& o, const Version& v);
}
}

#endif /* VERSION_H_ */
