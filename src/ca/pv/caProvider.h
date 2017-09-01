/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CAPROVIDER_H
#define CAPROVIDER_H

#include <shareLib.h>
#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {
class Configuration;
namespace ca {
/**
 * @brief  CAClientFactory is a channel provider for the ca network provider.
 *
 * 
 */
class epicsShareClass CAClientFactory
{
public:
    /** @brief start the provider
     *
     */
    static void start();
    /** @brief stop the provider
     *
     */
    static void stop();
     /** @brief Should debug info be shown?
     *
     * @param value level
     */
    static void setDebug(int value) {debug = value;}
    /** @brief Is debug set?
     *
     * level = (0,1,2,...) means (no messages, constructor/destructor, ??)
     * @return level
     */
    static int getDebug() {return debug;}
private:
    static int debug;
};

}
}
}

#endif  /* CAPROVIDER_H */
