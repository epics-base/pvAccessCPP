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

#include <compilerDependencies.h>

#include <pv/sharedPtr.h>

#include <shareLib.h>

namespace epics { namespace pvAccess {


        /**
         * @brief Instance declaring destroy method.
         */
        class Destroyable {
        public:
            POINTER_DEFINITIONS(Destroyable);
            /**
             * Destroy this instance.
             */
            virtual void destroy() {};
            
        protected:
            virtual ~Destroyable() {}
        public:

            /** for use with shared_ptr<> when wrapping
             *
             @code
               shared_ptr<foo> inner(new foo),
                               outer(inner.get, Destroyable::cleaner(inner));
             @endcode

             @warning Do not use this trick in combination with enable_shared_from_this
                      as it is _undefined_ whether the hidden weak_ptr will be the original
                      or wrapped reference.
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
namespace epics { namespace pvData {
    typedef ::epics::pvAccess::Destroyable Destroyable EPICS_DEPRECATED;
}}
#endif  /* DESTROYABLE_H */
