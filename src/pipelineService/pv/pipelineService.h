/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PIPELINESERVICE_H
#define PIPELINESERVICE_H

#include <stdexcept>

#ifdef epicsExportSharedSymbols
#   define pipelineServiceEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/sharedPtr.h>
#include <pv/status.h>

#ifdef pipelineServiceEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pipelineServiceEpicsExportSharedSymbols
#endif

#include <pv/pvAccess.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {

class epicsShareClass PipelineControl
{
public:
    POINTER_DEFINITIONS(PipelineControl);

    virtual ~PipelineControl() {};

    /// Number of free elements in the local queue.
    /// A service can (should) full up the entire queue.
    virtual size_t getFreeElementCount() = 0;

    /// Total count of requested elements.
    /// This is the minimum element count that a service should provide.
    virtual size_t getRequestedCount() = 0;

    /// Grab next free element.
    /// A service should take this element, populate it with the data
    /// and return it back by calling putElement().
    virtual MonitorElement::shared_pointer getFreeElement() = 0;

    /// Put element on the local queue (an element to be sent to a client).
    virtual void putElement(MonitorElement::shared_pointer const & element) = 0;

    /// Call to notify that there is no more data to pipelined.
    /// This call destroyes corresponding pipeline session.
    virtual void done() = 0;

};


class epicsShareClass PipelineSession
{
public:
    POINTER_DEFINITIONS(PipelineSession);

    virtual ~PipelineSession() {};

    /// Returns (minimum) local queue size.
    /// Actual local queue size = max( getMinQueueSize(), client queue size );
    virtual size_t getMinQueueSize() const = 0;

    /// Description of the structure used by this session.
    virtual epics::pvData::Structure::const_shared_pointer getStructure() const = 0;

    /// Request for additional (!) elementCount elements.
    /// The service should eventually call PipelineControl.getFreeElement() and PipelineControl.putElement()
    /// to provide [PipelineControl.getRequestedCount(), PipelineControl.getFreeElementCount()] elements.
    virtual void request(PipelineControl::shared_pointer const & control, size_t elementCount) = 0;

    /// Cancel the session (called by the client).
    virtual void cancel() = 0;
};


class epicsShareClass PipelineService
{
public:
    POINTER_DEFINITIONS(PipelineService);

    virtual ~PipelineService() {};

    virtual PipelineSession::shared_pointer createPipeline(
        epics::pvData::PVStructure::shared_pointer const & pvRequest
    ) = 0;

};


}
}

#endif  /* PIPELINESERVICE_H */
