/*
 * logger.h
 *
 *  Created on: Dec 10, 2010
 *      Author: Miha Vitorovic
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <pvType.h>

using epics::pvData::String;

namespace epics {
    namespace pvAccess {

        /**
         * Create a logger that will write to file indicated by the <tt>fname</tt>.
         * After creation you are free to use standard EPICSv3 functions from
         * <tt>errlog.h</tt>.
         *
         * @param[in] fname The file to write to. If the file exists, it
         * is opened for append.
         */
        void createFileLogger( String fname );

    }
}

#endif /* LOGGER_H_ */
