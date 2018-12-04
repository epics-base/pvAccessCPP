#ifndef CLIENTPVT_H
#define CLIENTPVT_H

#include <utility>

#include <epicsEvent.h>
#include <epicsThread.h>

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
 * then Derived::cancel() is called, but the object isn't free'd
 * until all internal references are dropped as well.
 */
template<typename Derived>
struct wrapped_shared_from_this {
private:
    // const after build()
    std::tr1::weak_ptr<Derived> myselfptr;

    struct canceller {
        std::tr1::shared_ptr<Derived> ptr;
        canceller(const std::tr1::shared_ptr<Derived>& ptr) :ptr(ptr) {}

        void operator()(Derived *) {
            std::tr1::shared_ptr<Derived> P;
            P.swap(ptr);
            P->cancel();
        }
    };

public:
    std::tr1::shared_ptr<Derived> internal_shared_from_this() {
        std::tr1::shared_ptr<Derived> ret(myselfptr);
        if(!ret)
            throw std::tr1::bad_weak_ptr();
        return ret;
    }

#if __cplusplus>=201103L
    template<class ...Args>
    static
    std::tr1::shared_ptr<Derived> build(Args... args) {
        std::tr1::shared_ptr<Derived> inner(new Derived(std::forward<Args>(args)...)),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }
#else
    static
    std::tr1::shared_ptr<Derived> build() {
        std::tr1::shared_ptr<Derived> inner(new Derived),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }

    template<typename A>
    static
    std::tr1::shared_ptr<Derived> build(A a) {
        std::tr1::shared_ptr<Derived> inner(new Derived(a)),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }

    template<typename A, typename B>
    static
    std::tr1::shared_ptr<Derived> build(A a, B b) {
        std::tr1::shared_ptr<Derived> inner(new Derived(a, b)),
                                   ret(inner.get(), canceller(inner));
        inner->myselfptr = inner;
        return ret;
    }
#endif
};

/** Safe use of raw callback pointer while unlocked.
 * clear pointer and then call CallbackGuard::wait() to ensure that concurrent
 * callback have completed.
 *
 * Prototype usage
 @code
 * struct mycb : public CallbackStorage {
 *      void (*ptr)();
 * };
 * // make a callback
 * void docb(mycb& cb) {
 *     CallbackGuard G(cb); // lock
 *     // decide whether to make CB
 *     if(ptr){
 *          void (*P)() = ptr; // copy for use while unlocked
 *          CallbackUse U(G); // unlock
 *          (*P)();
 *          // automatic re-lock
 *     }
 *     // automatic final unlock
 * }
 * void cancelop(mycb& cb) {
 *     CallbackGuard G(cb);
 *     ptr = 0;  // prevent further callbacks from starting
 *     G.wait(); // wait for inprogress callbacks to complete
 * }
 @endcode
 */
struct CallbackStorage {
    mutable epicsMutex mutex;
    epicsEvent wakeup;
    size_t nwaitcb;
    epicsThreadId incb;
    CallbackStorage() :nwaitcb(0u), incb(0) {}
};

// analogous to epicsGuard
struct CallbackGuard {
    CallbackStorage& store;
    epicsThreadId self;
    explicit CallbackGuard(CallbackStorage& store) :store(store), self(0) {
        store.mutex.lock();
    }
    ~CallbackGuard() {
        bool notify = store.nwaitcb!=0;
        store.mutex.unlock();
        if(notify)
            store.wakeup.signal();
    }
    void ensureself() {
        if(!self)
            self = epicsThreadGetIdSelf();
    }
    // unlock and block until no in-progress callbacks
    void wait() {
        if(!store.incb) return;
        ensureself();
        store.nwaitcb++;
        while(store.incb && store.incb!=self) {
            store.mutex.unlock();
            store.wakeup.wait();
            store.mutex.lock();
        }
        store.nwaitcb--;
    }
};

// analogous to epicsGuardRelease
struct CallbackUse {
    CallbackGuard& G;
    explicit CallbackUse(CallbackGuard& G) :G(G) {
        G.wait(); // serialize callbacks
        G.ensureself();
        G.store.incb=G.self;
        G.store.mutex.unlock();
    }
    ~CallbackUse() {
        G.store.mutex.lock();
        G.store.incb=0;
    }
};


void registerRefTrack();
void registerRefTrackGet();
void registerRefTrackPut();
void registerRefTrackMonitor();
void registerRefTrackRPC();
void registerRefTrackInfo();

}} // namespace pvac::detail

#endif // CLIENTPVT_H
