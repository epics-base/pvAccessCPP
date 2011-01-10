/*
 * hexDump.h
 *
 *  Created on: Nov 3, 2010
 *      Author: user
 */

#ifndef HEXDUMP_H_
#define HEXDUMP_H_

#include <pvType.h>

namespace epics {
    namespace pvAccess {

        /**
         * Output a buffer in hex format.
         * @param name  name (description) of the message.
         * @param bs    buffer to dump
         * @param len   first bytes (length) to dump.
         */
        void hexDump(const epics::pvData::String name, const epics::pvData::int8 *bs, int len);

        /**
         * Output a buffer in hex format.
         * @param[in] name  name (description) of the message.
         * @param[in] bs    buffer to dump
         * @param[in] start dump message using given offset.
         * @param[in] len   first bytes (length) to dump.
         */
        void hexDump(const epics::pvData::String name, const epics::pvData::int8 *bs, int start, int len);

        /**
         * Output a buffer in hex format.
         * @param[in] prologue string to prefixed to debug output, can be <code>null</code>
         * @param[in] name  name (description) of the message.
         * @param[in] bs    buffer to dump
         * @param[in] start dump message using given offset.
         * @param[in] len   first bytes (length) to dump.
         */
        void hexDump(const epics::pvData::String prologue, const epics::pvData::String name, const epics::pvData::int8 *bs,
                int start, int len);

    }
}

#endif /* HEXDUMP_H_ */
