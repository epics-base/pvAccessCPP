/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author msekoranja
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <shareLib.h>
#include <pv/pvAccess.h>

struct ca_client_context;

namespace epics {
namespace pvAccess {
namespace ca {

/**
 * @brief  CAClientFactory is a channel provider for the ca network provider.
 *
 * A single instance is created the first time CAClientFactory::start is called.
 * epicsAtExit is used to destroy the instance.
 *
 * The single instance calls:
 *      ca_context_create(ca_enable_preemptive_callback);
 *
 * The thread that calls start, or a ca auxillary thread, are the only threads
 * that can call the ca_* functions.
 *
 * NOTE: callbacks for monitor, get, and put are made from a separate thread.
 *    This is done to prevent a deadly embrace that can occur
 *         when rapid gets, puts, and monitor events are happening.
 *    The callbacks should not call any pvAccess method.
 *    If any such call is made the separate thread becomes a ca auxillary thread.
 * 
 */
class epicsShareClass CAClientFactory
{
public:
    /** @brief start provider ca
     *
     */
    static void start();
    /** @brief stop provider ca
     *
     * This does nothing since epicsAtExit is used to destroy the instance.
     */
    static void stop();
};

}}}

#endif  /* CAPROVIDER_H */
