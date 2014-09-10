/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#include <osiProcess.h>

#define epicsExportSharedSymbols
#include <pv/security.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

NoSecurityPlugin::shared_pointer NoSecurityPlugin::INSTANCE(new NoSecurityPlugin());

CAClientSecurityPlugin::shared_pointer CAClientSecurityPlugin::INSTANCE(new CAClientSecurityPlugin());

CAClientSecurityPlugin::CAClientSecurityPlugin()
{
    StructureConstPtr userAndHostStructure =
            getFieldCreate()->createFieldBuilder()->
            add("user", pvString)->
            add("host", pvString)->
            createStructure();

    m_userAndHost = getPVDataCreate()->createPVStructure(userAndHostStructure);

    //
    // user name
    //

    char buffer[256];

    std::string userName;
    if (osiGetUserName(buffer, sizeof(buffer)) == osiGetUserNameSuccess)
        userName = buffer;
    // TODO more error handling

    m_userAndHost->getSubField<PVString>("user")->put(userName);

    //
    // host name
    //

    std::string hostName;
    if (gethostname(buffer, sizeof(buffer)) == 0)
        hostName = buffer;
    // TODO more error handling

    m_userAndHost->getSubField<PVString>("host")->put(buffer);
}

