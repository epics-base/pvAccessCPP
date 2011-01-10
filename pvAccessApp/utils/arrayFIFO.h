/*
 * arrayFIFO.h
 *
 *  Created on: Nov 8, 2010
 *      Author: Miha Vitorovic
 */

#ifndef ARRAYFIFO_H_
#define ARRAYFIFO_H_

#ifdef ARRAY_FIFO_DEBUG
#include <iostream>
#endif

#include <lock.h>
#include <epicsException.h>

using epics::pvData::Mutex;
using epics::pvData::Lock;
using epics::pvData::BaseException;

namespace epics {
    namespace pvAccess {

        template<class T>
        class ArrayFIFO {
        public:
            /**
             * Constructs an empty array deque with an initial capacity
             * sufficient to hold the specified number of elements. Array FIFO
             * is designed to hold objects allocated on the heap.
             *
             * @param[in] numElements  lower bound on initial capacity of the
             * deque. Default value is 16 elements.
             */
            ArrayFIFO(size_t numElements = 16);
            ~ArrayFIFO();

            /**
             * Inserts the specified element at the front of this deque.
             *
             * @param[in] e the element to add.
             */
            void addFirst(const T e);

            /**
             * Inserts the specified element at the end of this deque.
             * @param[in] e the element to add
             */
            void addLast(const T e);

            T pollFirst();
            T pollLast();
            T peekFirst();
            T peekLast();

            /**
             * Pushes an element onto the stack represented by this deque.  In other
             * words, inserts the element at the front of this deque.
             *
             * @param[in] e the element to push
             */
            void push(const T e);

            /**
             * Pops an element from the stack represented by this deque.  In other
             * words, removes and returns the first element of this deque.
             *
             * @return the element at the front of this deque (which is the top
             *         of the stack represented by this deque), <code>null</code> if no element available.
             */
            T pop();

            /**
             * Looks at the object at the top of this stack without removing it
             * from the stack.
             * @return     the object at the top of this stack (the last item
             *             of the <tt>Vector</tt> object).
             */
            T peek();

            /**
             * Returns the number of elements in this deque.
             * @return the number of elements in this deque
             */
            size_t size();

            /**
             * Returns <tt>true</tt> if this deque contains no elements.
             *
             * @return <tt>true</tt> if this deque contains no elements
             */
            bool isEmpty();

            /**
             * Removes all of the elements from this deque.
             * The deque will be empty after this call returns.
             *
             * @param freeElements  tells the methods to automatically free
             * the memory of all the elments in the FIFO. Default is {@code true}
             */
            void clear();

            /**
             * Removes the first occurrence of the specified element in this
             * deque (when traversing the deque from head to tail).
             * If the deque does not contain the element, it is unchanged.
             * More formally, removes the first element <tt>e</tt> such that
             * <tt>o.equals(e)</tt> (if such an element exists).
             * Returns <tt>true</tt> if this deque contained the specified element
             * (or equivalently, if this deque changed as a result of the call).
             *
             * @param o element to be removed from this deque, if present
             * @return <tt>true</tt> if the deque contained the specified element
             */
            bool remove(const T e);

#ifdef ARRAY_FIFO_DEBUG
            void debugState() {
                //size_t mask = _size-1;
                std::cout<<"h:"<<_head<<",t:"<<_tail<<",c:"<<_size;
                std::cout<<",s:"<<size()<<std::endl;
                std::cout<<"Content:"<<std::endl;
                for (size_t i = 0; i < _size; i++)
                    std::cout<<"["<<i<<"]: "<<_elements[i]<<std::endl;
            }
#endif

        private:
            T* _elements; // array of pointers
            size_t _head, _tail, _size;
            Mutex _mutex;
            static int MIN_INITIAL_CAPACITY;

            /**
             * Allocate empty array to hold the given number of elements.
             * @param[in] numElements  the number of elements to hold
             */
            void allocateElements(const size_t numElements);

            /**
             * Double the capacity of this deque.  Call only when full, i.e.,
             * when head and tail have wrapped around to become equal.
             */
            void doubleCapacity();

            void arraycopy(T* src, size_t srcPos, T* dest, size_t destPos,
                    size_t length);

            /**
             * Removes the element at the specified position in the elements array,
             * adjusting head and tail as necessary.  This can result in motion of
             * elements backwards or forwards in the array.
             *
             * <p>This method is called delete rather than remove to emphasize
             * that its semantics differ from those of {@link List#remove(int)}.
             *
             * @return true if elements moved backwards
             */
            bool del(const size_t i);

        };

        /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
         * g++ requires template definition inside a header file.
         * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
        template<class T>
        int ArrayFIFO<T>::MIN_INITIAL_CAPACITY = 8;

        template<class T>
        void ArrayFIFO<T>::arraycopy(T* src, size_t srcPos, T* dest,
                size_t destPos, size_t length) {
            if(srcPos<destPos) // this takes care of same-buffer copy
                for(int i = length-1; i>=0; i--)
                    dest[destPos+i] = src[srcPos+i];
            else
                for(size_t i = 0; i<length; i++)
                    dest[destPos++] = src[srcPos++];
        }

        template<class T>
        void ArrayFIFO<T>::allocateElements(size_t numElements) {
            _size = MIN_INITIAL_CAPACITY;
            // Find the best power of two to hold elements.
            // Tests "<=" because arrays aren't kept full.
            if(numElements>=_size) {
                _size = numElements;
                _size |= (_size>>1);
                _size |= (_size>>2);
                _size |= (_size>>4);
                _size |= (_size>>8);
                _size |= (_size>>16);
                _size++;
            }
            _elements = new T[_size];
        }

        template<class T>
        void ArrayFIFO<T>::doubleCapacity() {
            size_t p = _head;
            size_t n = _size;
            size_t r = n-p; // number of elements to the right of p
            size_t newCapacity = n<<1;
            T* a = new T[newCapacity];
            arraycopy(_elements, p, a, 0, r);
            arraycopy(_elements, 0, a, r, p);
            delete[] _elements;
            _elements = a;
            _size = newCapacity;
            _head = 0;
            _tail = n;

        }

        template<class T>
        ArrayFIFO<T>::ArrayFIFO(size_t numElements) :
            _head(0), _tail(0), _mutex() {
            allocateElements(numElements);
        }

        template<class T>
        ArrayFIFO<T>::~ArrayFIFO() {
            delete[] _elements;
        }

        template<class T>
        void ArrayFIFO<T>::addFirst(const T e) {
            Lock lock(&_mutex);

            _elements[_head = (_head-1)&(_size-1)] = e;
            if(_head==_tail) doubleCapacity();
        }

        template<class T>
        void ArrayFIFO<T>::addLast(const T e) {
            Lock lock(&_mutex);

            _elements[_tail] = e;
            if((_tail = (_tail+1)&(_size-1))==_head) doubleCapacity();
        }

        template<class T>
        T ArrayFIFO<T>::pollFirst() {
            Lock lock(&_mutex);

            if(isEmpty()) return 0;

            T result = _elements[_head]; // Element is null if deque empty
            _head = (_head+1)&(_size-1);
            return result;
        }

        template<class T>
        T ArrayFIFO<T>::pollLast() {
            Lock lock(&_mutex);

            if(isEmpty()) return 0;

            _tail = (_tail-1)&(_size-1);
            return _elements[_tail];
        }

        template<class T>
        T ArrayFIFO<T>::peekFirst() {
            Lock lock(&_mutex);

            if(isEmpty()) return 0;

            return _elements[_head];
        }

        template<class T>
        T ArrayFIFO<T>::peekLast() {
            Lock lock(&_mutex);

            if(isEmpty()) return 0;

            return _elements[(_tail-1)&(_size-1)];
        }

        template<class T>
        void ArrayFIFO<T>::push(const T e) {
            addLast(e);
        }

        template<class T>
        T ArrayFIFO<T>::pop() {
            return pollLast();
        }

        template<class T>
        T ArrayFIFO<T>::peek() {
            return peekFirst();
        }

        template<class T>
        size_t ArrayFIFO<T>::size() {
            Lock lock(&_mutex);

            return (_tail-_head)&(_size-1);
        }

        template<class T>
        bool ArrayFIFO<T>::isEmpty() {
            Lock lock(&_mutex);

            return _head==_tail;
        }

        template<class T>
        void ArrayFIFO<T>::clear() {
            Lock lock(&_mutex);

            _head = _tail = 0;
        }

        template<class T>
        bool ArrayFIFO<T>::del(const size_t i) {
            // i is absolute index in the array
            size_t mask = _size-1;
            size_t h = _head;
            size_t t = _tail;
            size_t front = (i-h)&mask;
            size_t back = (t-i)&mask;

            // Invariant: head <= i < tail mod circularity
            if(front>=((t-h)&mask)) THROW_BASE_EXCEPTION(
                    "Illegal State Exception"); // concurrency problem!!!

            // Optimize for least element motion
            if(front<back) {
                if(h<=i) {
                    arraycopy(_elements, h, _elements, h+1, front);
                }
                else { // Wrap around
                    arraycopy(_elements, 0, _elements, 1, i);
                    if(t>0) _elements[0] = _elements[mask];
                    arraycopy(_elements, h, _elements, h+1, mask-h);
                }
                _head = (h+1)&mask;

                return false;
            }
            else {
                if(i<t) { // Copy the null tail as well
                    arraycopy(_elements, i+1, _elements, i, back);
                    _tail = t-1;
                }
                else { // Wrap around
                    arraycopy(_elements, i+1, _elements, i, mask-i);
                    _elements[mask] = _elements[0];
                    arraycopy(_elements, 1, _elements, 0, t);
                    _tail = (t-1)&mask;
                }
                return true;
            }
        }

        template<class T>
        bool ArrayFIFO<T>::remove(const T e) {
            Lock lock(&_mutex);

            if(isEmpty()) return false; // nothing to do

            size_t mask = _size-1;
            size_t i = _head;
            while(i!=_tail) {
                if(e==_elements[i]) {
                    del(i);
                    return true;
                }
                i = (i+1)&mask;
            }
            return false;
        }

    }
}

#endif /* ARRAYFIFO_H_ */
