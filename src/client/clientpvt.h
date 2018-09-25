#ifndef CLIENTPVT_H
#define CLIENTPVT_H

#include <pv/sharedPtr.h>

#include <pva/client.h>

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;
typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace pvac{namespace detail{
/* Like std::tr1::enable_shared_from_this
 * with the notion of internal vs. external references.
 * External references wrap an internal reference.
 * When the last external reference is dropped,
 * then Base::cancel() is called, but the object isn't free'd
 * until all internal references are dropped as well.
 */
template<typename Base>
struct wrapped_shared_from_this {
private:
    // const after build()
    std::tr1::weak_ptr<Base> myselfptr;

    struct canceller {
        std::tr1::shared_ptr<Base> ptr;
        canceller(const std::tr1::shared_ptr<Base>& ptr) :ptr(ptr) {}

        void operator()(Base *) {
            std::tr1::shared_ptr<Base> P;
            P.swap(ptr);
            P->cancel();
        }
    };

public:
    std::tr1::shared_ptr<Base> internal_shared_from_this() {
        std::tr1::shared_ptr<Base> ret(myselfptr);
        if(!ret)
            throw std::tr1::bad_weak_ptr();
        return ret;
    }

    static
    std::tr1::shared_ptr<Base> build() {
        std::tr1::shared_ptr<Base> inner(new Base),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }

    template<typename A>
    static
    std::tr1::shared_ptr<Base> build(A a) {
        std::tr1::shared_ptr<Base> inner(new Base(a)),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }

    template<typename A, typename B>
    static
    std::tr1::shared_ptr<Base> build(A a, B b) {
        std::tr1::shared_ptr<Base> inner(new Base(a, b)),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }
};

void registerRefTrack();
void registerRefTrackGet();
void registerRefTrackPut();
void registerRefTrackMonitor();
void registerRefTrackRPC();

}} // namespace pvac::detail

#endif // CLIENTPVT_H
