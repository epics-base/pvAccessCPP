/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/pvData.h>
#include <pv/pipelineServer.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static Structure::const_shared_pointer dataStructure =
    getFieldCreate()->createFieldBuilder()->
    add("count", pvInt)->
    createStructure();

class PipelineSessionImpl :
    public PipelineSession
{
public:

    PipelineSessionImpl(
        epics::pvData::PVStructure::shared_pointer const & pvRequest
    ) :
        m_counter(0),
        m_max(0)
    {
        PVStructure::shared_pointer pvOptions = pvRequest->getSubField<PVStructure>("record._options");
        if (pvOptions) {
            PVString::shared_pointer pvString = pvOptions->getSubField<PVString>("limit");
            if (pvString)
            {
                // note: this throws an exception if conversion fails
                m_max = pvString->getAs<int32>();
            }
        }

    }

    size_t getMinQueueSize() const {
        return 16; //1024;
    }

    Structure::const_shared_pointer getStructure() const {
        return dataStructure;
    }

    virtual void request(PipelineControl::shared_pointer const & control, size_t elementCount) {
        // blocking in this call is not a good thing
        // but generating a simple counter data is fast
        // we will generate as much elements as we can
        size_t count = control->getFreeElementCount();
        for (size_t i = 0; i < count; i++) {
            MonitorElement::shared_pointer element = control->getFreeElement();
            element->pvStructurePtr->getSubField<PVInt>(1 /*"count"*/)->put(m_counter++);
            control->putElement(element);

            // we reached the limit, no more data
            if (m_max != 0 && m_counter == m_max)
            {
                control->done();
                break;
            }
        }
    }

    virtual void cancel() {
        // noop, no need to clean any data-source resources
    }

private:
    // NOTE: all the request calls will be made from the same thread, so we do not need sync m_counter
    int32 m_counter;
    int32 m_max;
};

class PipelineServiceImpl :
    public PipelineService
{
    PipelineSession::shared_pointer createPipeline(
        epics::pvData::PVStructure::shared_pointer const & pvRequest
    )
    {
        return PipelineSession::shared_pointer(new PipelineSessionImpl(pvRequest));
    }
};

int main()
{
    PipelineServer server;

    server.registerService("counterPipe", PipelineService::shared_pointer(new PipelineServiceImpl()));
    // you can register as many services as you want here ...

    server.printInfo();
    server.run();

    return 0;
}
