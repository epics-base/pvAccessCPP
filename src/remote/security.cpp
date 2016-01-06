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

    m_userAndHost->getSubFieldT<PVString>("user")->put(userName);

    //
    // host name
    //

    std::string hostName;
    if (gethostname(buffer, sizeof(buffer)) == 0)
        hostName = buffer;
    // TODO more error handling

    m_userAndHost->getSubFieldT<PVString>("host")->put(buffer);
}


void AuthNZHandler::handleResponse(osiSockAddr* responseFrom,
                                    Transport::shared_pointer const & transport,
                                    epics::pvData::int8 version,
                                    epics::pvData::int8 command,
                                    size_t payloadSize,
                                    epics::pvData::ByteBuffer* payloadBuffer)
{
    AbstractResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

    epics::pvData::PVField::shared_pointer data =
            SerializationHelper::deserializeFull(payloadBuffer, transport.get());

    transport->authNZMessage(data);
}

SecurityPluginRegistry::SecurityPluginRegistry() {
    // install CA client security plugin by default
    installClientSecurityPlugin(CAClientSecurityPlugin::INSTANCE);
}


