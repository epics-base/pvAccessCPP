/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef FAIRQUEUE_H
#define FAIRQUEUE_H

#include <vector>

#ifdef epicsExportSharedSymbols
#   define fairQueueExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsGuard.h>
#include <ellLib.h>
#include <dbDefs.h>

#include <pv/sharedPtr.h>

#ifdef fairQueueExportSharedSymbols
#   define epicsExportSharedSymbols
#   undef fairQueueExportSharedSymbols
#endif

#include <shareLib.h>

namespace epics {
namespace pvAccess {


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
class fair_queue
{
    typedef epicsGuard<epicsMutex> guard_t;
public:
    typedef std::tr1::shared_ptr<T> value_type;

    class entry {
        /* In c++, use of ellLib (which implies offsetof()) should be restricted
         * to POD structs.  So enode_t exists as a POD struct for which offsetof()
         * is safe and well defined.  enode_t::self is used in place of
         * casting via CONTAINER(penode, entry, enode)
         */
        struct enode_t {
            ELLNODE node;
            entry *self;
        } enode;
        unsigned Qcnt;
        value_type holder;
        fair_queue *owner;

        friend class fair_queue;

        entry(const entry&);
        entry& operator=(const entry&);
    public:
        entry() :Qcnt(0), holder()
            , owner(NULL)
        {
            enode.node.next = enode.node.previous = NULL;
            enode.self = this;
        }
        ~entry() {
            // nodes should be removed from the list before deletion
            assert(!enode.node.next && !enode.node.previous);
            assert(Qcnt==0 && !holder);
            assert(!owner);
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

    //! Remove all items.
    //! @post empty()==true
    void clear()
    {
        // destroy after unlock
        std::vector<value_type> garbage;
        {
            guard_t G(mutex);

            garbage.resize(unsigned(ellCount(&list)));
            size_t i=0;

            while(ELLNODE *cur = ellGet(&list)) {
                typedef typename entry::enode_t enode_t;
                enode_t *PN = CONTAINER(cur, enode_t, node);
                entry *P = PN->self;
                assert(P->owner==this);
                assert(P->Qcnt>0);

                PN->node.previous = PN->node.next = NULL;
                P->owner = NULL;
                P->Qcnt = 0u;
                garbage[i++].swap(P->holder);
            }
        }
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
                ellAdd(&list, &P->enode.node); // push_back
            } else
                assert(P->owner==this);
        }
        if(wake) wakeup.signal();
    }

    bool pop_front_try(value_type& ret)
    {
        ret.reset();
        guard_t G(mutex);
        ELLNODE *cur = ellGet(&list); // pop_front

        if(cur) {
            typedef typename entry::enode_t enode_t;
            enode_t *PN = CONTAINER(cur, enode_t, node);
            entry *P = PN->self;
            assert(P->owner==this);
            assert(P->Qcnt>0);
            if(--P->Qcnt==0) {
                PN->node.previous = PN->node.next = NULL;
                P->owner = NULL;

                ret.swap(P->holder);
            } else {
                ellAdd(&list, &P->enode.node); // push_back

                ret = P->holder;
            }
            return true;
        } else {
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

}
} // namespace

#endif // FAIRQUEUE_H
