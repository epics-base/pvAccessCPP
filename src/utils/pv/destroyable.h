/* destroyable.h */
/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/**
 *  @author mse
 */
#ifndef DESTROYABLE_H
#define DESTROYABLE_H

#include <pv/sharedPtr.h>

#include <shareLib.h>

namespace epics { namespace pvData { 


        /**
         * @brief Instance declaring destroy method.
         * 
         * @author mse
         */
        class epicsShareClass Destroyable {
        public:
            POINTER_DEFINITIONS(Destroyable);
            /**
             * Destroy this instance.
             */
            virtual void destroy() = 0;
            
        protected:
            /**
             * Do not allow delete on this instance and derived classes, destroy() must be used instead.
             */ 
            virtual ~Destroyable() {};
        public:

            /** for use with shared_ptr<> when wrapping
             *
             @code
               shared_ptr<foo> inner(new foo),
                               outer(inner.get, Destroyable::cleaner(inner));
             @endcode
             */
            class cleaner {
                Destroyable::shared_pointer ptr;
            public:
                cleaner(const Destroyable::shared_pointer& ptr) :ptr(ptr) {}
                void operator()(Destroyable*) {
                    Destroyable::shared_pointer P;
                    P.swap(ptr);
                    P->destroy();
                }
            };
        };

}}
#endif  /* DESTROYABLE_H */
