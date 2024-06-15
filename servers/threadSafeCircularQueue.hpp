/*
 * theadSafeCircularQueue.hpp
 * 
 * This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
 * 
 *  May 22, 2024, Bojan Jurca
 * 
 */


#include "std/queue.hpp"


#ifndef __THREAD_SAFE_CIRCULAR_QUEUE_HPP__
    #define __THREAD_SAFE_CIRCULAR_QUEUE_HPP__


    template<class queueType, size_t maxSize> class threadSafeCircularQueue : public queue<queueType, maxSize> {

        public:

            void Lock () { xSemaphoreTakeRecursive (__semaphore__, portMAX_DELAY); } 

            void Unlock () { xSemaphoreGiveRecursive (__semaphore__); }

            void push_back (queueType element) {
                Lock ();
                queue<queueType, maxSize>::push_back (element);
                Unlock ();
            }

            void pop_front () {
                Lock ();
                queue<queueType, maxSize>::pop_front ();
                Unlock ();
            }

            class Iterator : public queue<queueType, maxSize>::Iterator {
              
                public:

                    Iterator (threadSafeCircularQueue* cq, size_t position) : queue<queueType, maxSize>::Iterator (cq, position) { __cq__ = cq; }

                    Iterator (queueType *qPtr, threadSafeCircularQueue* cq) : queue<queueType, maxSize>::Iterator (cq, qPtr) { __cq__ = cq; }

                    ~Iterator () { __cq__->Unlock (); }

                private:

                    threadSafeCircularQueue *__cq__;

            };

            Iterator begin () { 
                Lock (); 
                return Iterator (this, 0); 
            } 

            Iterator begin (queueType *qPtr) { 
                Lock (); 
                return Iterator (qPtr, this); 
            }
            
            Iterator end () { 
                Lock (); 
                return Iterator (this, this->size () - 1); 
            } 


        private:

            SemaphoreHandle_t __semaphore__ = xSemaphoreCreateRecursiveMutex (); 

    };

#endif
