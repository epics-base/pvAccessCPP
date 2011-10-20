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

        using epics::pvData::String;

        class Version : public epics::pvData::NoDefaultMethods {
        public:
            /**
             * Default constructor.
             * @param productName	product name.
             * @param implementationLangugage	implementation language.
             * @param majorVersion	major version.
             * @param minorVersion	minor version.
             * @param maintenanceVersion	maintenance version.
             * @param developmentVersion	development version.
             */
            Version(String productName, String implementationLangugage,
                    int majorVersion, int minorVersion,
                    int maintenanceVersion, int developmentVersion) :
                _productName(productName),
                _implementationLanguage(implementationLangugage),
                _majorVersion(majorVersion),
                _minorVersion(minorVersion),
                _maintenanceVersion(maintenanceVersion),
                _developmentVersion(developmentVersion)
            {
            }

            /** The name of the product */
            inline const String getProductName() const {
                return _productName;
            }

            /** Implementation Language: C++
             */
            inline const String getImplementationLanguage() const {
                return _implementationLanguage;
            }

            /**
             * Major version number. This changes only when there is a
             * significant, externally apparent enhancement from the previous release.
             * 'n' represents the n'th version.
             *
             * Clients should carefully consider the implications of new versions as
             * external interfaces and behaviour may have changed.
             */
            inline int getMajorVersion() const {
                return _majorVersion;
            }

            /**
             * Minor vesion number. This changes when:
             * <ul>
             * <li>a new set of functionality is to be added</li>
             * <li>API or behaviour change</li>
             * <li>its designated as a reference release</li>
             * </ul>
             */
            inline int getMinorVersion() const {
                return _minorVersion;
            }

            /**
             * Maintenance version number. Optional identifier used to designate
             * maintenance drop applied to a specific release and contains fixes for
             * defects reported. It maintains compatibility with the release and
             * contains no API changes. When missing, it designates the final and
             * complete development drop for a release.
             */
            inline int getMaintenanceVersion() const {
                return _maintenanceVersion;
            }

            /**
             * Development drop number. Optional identifier designates development drop
             * of a specific release. D01 is the first development drop of a new
             * release.
             *
             * Development drops are works in progress towards a compeleted, final
             * release. A specific development drop may not completely implement all
             * aspects of a new feature, which may take several development drops to
             * complete. At the point of the final drop for the release, the D suffix
             * will be omitted.
             *
             * Each 'D' drops can contain functional enhancements as well as defect
             * fixes. 'D' drops may not be as stable as the final releases.
             */
            inline int getDevelopmentVersion() const {
                return _developmentVersion;
            }

            /**
             * Get the long version string. Version String formatted like <BR/><CODE>
             * "<B>ProductName </B> \[<B>ImplementationLanguage</B>\] 'v'v.r[.dd|<B>D</B>nn]"
             * </CODE> <BR/>e.g. <BR/><CODE>"<B>CAJ </B> [<B>Java</B>] v1.0.1"</CODE>
             * <BR/>
             *
             * @return String denoting current version
             */
            const String getLongVersionString() const;

            /**
             * Get the basic version string. Version String formatted like <BR/><CODE>
             * "<B>ProductName </B> 'v'v.r[.dd|<B>D</B>nn]"
             * </CODE> <BR/>e.g. <BR/><CODE>"<B>CAJ </B> v1.0.1"</CODE>
             * <BR/>
             *
             * @return String denoting current version
             */
            const String getVersionString() const;

        private:
            String _productName;
            String _implementationLanguage;
            int _majorVersion;
            int _minorVersion;
            int _maintenanceVersion;
            int _developmentVersion;
        };
}
}

#endif /* VERSION_H_ */
