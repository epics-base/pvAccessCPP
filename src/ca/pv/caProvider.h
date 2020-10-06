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

namespace epics {
namespace pvAccess {
namespace ca {

/**
 * @brief CAClientFactory registers a channel provider for operations over the
 * CA network protocol.
 *
 * A single instance is created the first time CAClientFactory::start is called.
 *
 * The single instance calls:
 *      ca_context_create(ca_enable_preemptive_callback);
 *
 * The thread that calls start, or a ca auxillary thread, are the only threads
 * that can call the ca_* functions.
 *
 * NOTE: Notifications for connection changes and monitor, get, and put events
 * are made from separate threads to prevent deadlocks.
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
     * This does nothing.
     */
    static void stop();
};

}}}

#endif  /* CAPROVIDER_H */
