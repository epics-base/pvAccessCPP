/*
 * growingCircularBuffer.h
 *
 *  Created on: Nov 11, 2010
 *      Author: Miha Vitorovic
 */

#ifndef GROWINGCIRCULARBUFFER_H_
#define GROWINGCIRCULARBUFFER_H_

#include <epicsException.h>

using epics::pvData::BaseException;

namespace epics {
    namespace pvAccess {

        /**
         * Implementation of circular FIFO unbouded buffer.
         * Instance is not thread safe.
         * @author Miha Vitorovic
         */
        template<class T>
        class GrowingCircularBuffer {
        public:

            /**
             * Create a GrowingCircularBuffer with the given capacity.
             **/
            GrowingCircularBuffer(size_t capacity = 16) :
                _elements(new T[capacity]), _takePointer(0), _putPointer(0), _count(0), _size(capacity)
            {
            }

            ~GrowingCircularBuffer() {
                delete[] _elements;
            }

            /**
             * Get number of elements in the buffer.
             * @return number of elements in the buffer.
             */
            inline size_t size() {
                return _count;
            }

            /**
             * Get current buffer capacity.
             * @return buffer current capacity.
             */
            inline size_t capacity() {
                return _size;
            }

            /**
             * Insert a new element in to the buffer.
             * If buffer full the buffer is doubled.
             *
             * @param x element to insert.
             * @return <code>true</code> if first element.
             */
            bool insert(const T x);

            /**
             * Extract the oldest element from the buffer.
             * @return the oldest element from the buffer.
             */
            T extract();

        private:
            /**
             * Array (circular buffer) of elements.
             */
            T* _elements;

            /**
             * Take (read) pointer.
             */
            size_t _takePointer;

            /**
             * Put (write) pointer.
             */
            size_t _putPointer;

            /**
             * Number of elements in the buffer.
             */
            size_t _count;

            size_t _size;

            void arraycopy(T* src, size_t srcPos, T* dest, size_t destPos,
                    size_t length);
        };

        /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
         * g++ requires template definition inside a header file.
         * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

        template<class T>
        void GrowingCircularBuffer<T>::arraycopy(T* src, size_t srcPos, T* dest,
                size_t destPos, size_t length) {
            if(srcPos<destPos) // this takes care of same-buffer copy
                for(int i = length-1; i>=0; i--)
                    dest[destPos+i] = src[srcPos+i];
            else
                for(size_t i = 0; i<length; i++)
                    dest[destPos++] = src[srcPos++];
        }

        template<class T>
        bool GrowingCircularBuffer<T>::insert(const T x) {
            if (_count == _size)
            {
                // we are full, grow by factor 2
                T* newElements = new T[_size * 2];

                // invariant: _takePointer < _size
                size_t split = _size - _takePointer;
                if (split > 0)
                    arraycopy(_elements, _takePointer, newElements, 0, split);
                if (_takePointer != 0)
                    arraycopy(_elements, 0, newElements, split, _putPointer);

                _takePointer = 0;
                _putPointer = _size;
                _size *= 2;
                delete[] _elements;
                _elements = newElements;
            }
            _count++;

            _elements[_putPointer] = x;
            if (++_putPointer >= _size) _putPointer = 0;
            return _count == 1;
        }

        template<class T>
        T GrowingCircularBuffer<T>::extract() {
            if(_count==0) THROW_BASE_EXCEPTION("Buffer empty.");

            _count--;
            T old = _elements[_takePointer];
            if(++_takePointer>=_size) _takePointer = 0;
            return old;
        }

    }
}
#endif /* GROWINGCIRCULARBUFFER_H_ */
