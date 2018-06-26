/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
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
 * The thread that calls start, or a ca auxillary thread, are the only threads
 * that can call the ca_* functions.
 *
 * Note the monitor callbacks are made from a separate thread that must NOT call any ca_* function.
 * 
 */
class epicsShareClass CAClientFactory
{
public:
    /** @brief start provider ca
     *
     */
    static void start();
    /** @brief get the ca_client_context
     *
     * This can be called by an application specific auxiliary thread.
     * See ca documentation. Not for casual use.
     */
    static struct ca_client_context * get_ca_client_context();
    /** @brief stop provider ca
     *
     * This does nothing since epicsAtExit is used to destroy the instance.
     */
    static void stop();
};

}}}

#endif  /* CAPROVIDER_H */
