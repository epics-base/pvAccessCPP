/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef FAIRQUEUE_H
#define FAIRQUEUE_H

#include <shareLib.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <ellLib.h>
#include <dbDefs.h>

#include <pv/sharedPtr.h>

namespace epics {namespace pvAccess {


/** @brief An intrusive, loss-less, unbounded, round-robin queue
 *
 * The parameterized type 'T' must be a sub-class of @class fair_queue<T>::entry
 *
 * @li Intrusive.  Entries in the queue must derive from @class entry
 *
 * @li Loss-less.  An entry will be returned by pop_front() corresponding to
 *     each call to push_back().
 *
 * @li Un-bounded.  There is no upper limit to the number of times an entry
 *     may be queued other than machine constraints.
 *
 * @li Round robin.  The order that entries are returned may not match
 *     the order they were added in.  "Fairness" is achived by returning
 *     entries in a rotating fashion based on the order in which they were
 *     first added.  Re-adding the same entry before it is popped does not change
 *     this order.
 *     Adding [A, A, B, A, C, C] would give out [A, B, C, A, C, A].
 *
 * @warning Only one thread should call pop_front()
 *   as push_back() does not broadcast (only wakes up one waiter)
 */
template<typename T>
class epicsShareClass fair_queue
{
    typedef epicsGuard<epicsMutex> guard_t;
public:
    typedef std::tr1::shared_ptr<T> value_type;

    class epicsShareClass entry {
        ELLNODE node;
        unsigned Qcnt;
        value_type holder;
#ifndef NDEBUG
        fair_queue *owner;
#endif

        friend class fair_queue;

        entry(const entry&);
        entry& operator=(const entry&);
    public:
        entry() :node(), Qcnt(0), holder()
#ifndef NDEBUG
          , owner(NULL)
#endif
        {
            node.next = node.previous = NULL;
        }
        ~entry() {
            // nodes should be removed from the list before deletion
            assert(!node.next && !node.previous && !owner);
        }
    };

    fair_queue()
    {
        ellInit(&list);
    }
    ~fair_queue()
    {
        clear();
        assert(ellCount(&list)==0);
    }

    void clear()
    {
        value_type C;
        guard_t G(mutex);
        do {
            pop_front_try(C);
        } while(C);
    }

    bool empty() const {
        guard_t G(mutex);
        return ellFirst(&list)==NULL;
    }

    void push_back(const value_type& ent)
    {
        bool wake;
        entry *P = ent.get();
        {
            guard_t G(mutex);
            wake = ellFirst(&list)==NULL; // empty queue

            if(P->Qcnt++==0) {
                // not in list
                assert(P->owner==NULL);
                P->owner = this;
                P->holder = ent; // the list will hold a reference
                ellAdd(&list, &P->node); // push_back
            } else
                assert(P->owner==this);
        }
        if(wake) wakeup.signal();
    }

    bool pop_front_try(value_type& ret)
    {
        guard_t G(mutex);
        ELLNODE *cur = ellGet(&list); // pop_front

        if(cur) {
            entry *P = CONTAINER(cur, entry, node);
            assert(P->owner==this);
            assert(P->Qcnt>0);
            if(--P->Qcnt==0) {
                P->node.previous = P->node.next = NULL;
                P->owner = NULL;

                ret.swap(P->holder);
            } else {
                ellAdd(&list, &P->node); // push_back

                ret = P->holder;
            }
            return true;
        } else {
            ret.reset();
            return false;
        }
    }

    void pop_front(value_type& ret)
    {
        while(1) {
            pop_front_try(ret);
            if(ret)
                break;
            wakeup.wait();
        }
    }

    bool pop_front(value_type& ret, double timeout)
    {
        while(1) {
            pop_front_try(ret);
            if(ret)
                return true;
            if(!wakeup.wait(timeout))
                return false;
        }
    }

private:
    ELLLIST list;
    mutable epicsMutex mutex;
    mutable epicsEvent wakeup;
};

}} // namespace

#endif // FAIRQUEUE_H
