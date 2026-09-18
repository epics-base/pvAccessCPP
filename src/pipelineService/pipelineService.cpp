/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/pipelineService.h>

namespace epics {
namespace pvAccess {

// Out-of-line destructors anchor each interface's vtable and type_info
// to this translation unit.
PipelineControl::~PipelineControl() {}
PipelineSession::~PipelineSession() {}
PipelineService::~PipelineService() {}

}
}
