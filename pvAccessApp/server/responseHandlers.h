/*
 * responseHandlers.h
 *
 *  Created on: Jan 5, 2011
 *      Author: user
 */

#ifndef RESPONSEHANDLERS_H_
#define RESPONSEHANDLERS_H_

#include "remote.h"

namespace epics {
    namespace pvAccess {

        /**
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: AbstractServerResponseHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class AbstractServerResponseHandler : public AbstractResponseHandler {
        public:
            /**
             * @param context
             * @param description
             */
            AbstractServerResponseHandler(ServerContextImpl* context,
                    String description) :
                AbstractResponseHandler(description), _context(context) {
            }

            virtual ~AbstractServerResponseHandler() {
            }
        protected:
            ServerContextImpl* _context;
        };

        /**
         * Bad request handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: BadResponse.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class BadResponse : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            BadResponse(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Bad request") {
            }

            virtual ~BadResponse() {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * CAS request handler - main handler which dispatches requests to appropriate handlers.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ServerResponseHandler.java,v 1.1 2010/05/03 14:45:48 mrkraimer Exp $
         */
        class ServerResponseHandler : public ResponseHandler {
        public:
            ServerResponseHandler(ServerContextImpl* context);

            virtual ~ServerResponseHandler();

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
            static const int HANDLER_TABLE_LENGTH = 28;
            /**
             * Table of response handlers for each command ID.
             */
            ResponseHandler** _handlerTable;

            /**
             * Context instance.
             */
            ServerContextImpl* _context;

        };



    }
}


#endif /* RESPONSEHANDLERS_H_ */
