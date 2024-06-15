/*
 *  queue.hpp for Arduino (ESP boards) - circular queue with defined maxSize, a more simple version of vector
 * 
 *  This file is part of C++ std package for Arduino: https://github.com/BojanJurca/std-package-for-Arduino
 * 
 *  Picture of queue intgernal storge:
 * 
 *  queueType __elements__ :   | | |5|4|3|2|1|0|5|6|7|8|9| | | | | | | |
 *                              |    |<--- __size__ ---->|              |
 *                              | __front__           __back__          |
 *                              |<---------------- maxSize ------------>|  
 *
 * 
 *  May 22, 2024, Bojan Jurca
 * 
 */


#ifndef __QUEUE_HPP__
    #define __QUEUE_HPP__


    // ----- TUNNING PARAMETERS -----

    // #define __THROW_QUEUE_EXCEPTIONS__  // uncomment this line if you want queue to throw exceptions


    // error flags: there are only two types of error flags that can be set: OVERFLOW and OUT_OF_RANGE - please note that all errors are negative (char) numbers
    #define err_ok           ((signed char) 0b00000000) //    0 - no error 
    #define err_bad_alloc    ((signed char) 0b10000001) // -127 - out of memory
    #define err_out_of_range ((signed char) 0b10000010) // -126 - invalid index


    // type of memory used
    #define STACK_OR_GLOBAL_MEM 1
    #define HEAP_MEM 2
    #define PSRAM_MEM 3
    #ifndef QUEUE_MEMORY_TYPE 
        #define QUEUE_MEMORY_TYPE HEAP_MEM // use heap by default
    #endif


    template <class queueType, size_t maxSize> class queue {

        private: 

            signed char __errorFlags__ = 0;


        public:

            signed char errorFlags () { return __errorFlags__ & 0b01111111; }
            void clearErrorFlags () { __errorFlags__ = 0; }

      
           /*
            *  Constructor of queue:
            *  
            *     queue<int, 100> Q;
            */
      
            queue () {
                // allocate new memory for the queue
                #if QUEUE_MEMORY_TYPE == PSRAM_MEM
                    __elements__ = (queueType *) ps_malloc (sizeof (queueType) * maxSize);
                #elif QUEUE_MEMORY_TYPE == HEAP_MEM
                    __elements__ = (queueType *) malloc (sizeof (queueType) * maxSize);
                #else // use stack or global memory
                    // __elemets__ aray is already created
                #endif
                if (__elements__ == NULL) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                }

                #ifndef ARDUINO_ARCH_AVR // Assuming Arduino Mega or Uno
                    new (__elements__) queueType [maxSize]; 
                #else                 
                    // we can skip this step but won't be able to use objects as queue elements then
                #endif

                if (__elements__ == NULL) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                }
            }


           /*
            * queue destructor - free the memory occupied by queue elements
            */
            
            ~queue () { 
                #if QUEUE_MEMORY_TYPE != STACK_OR_GLOBAL_MEM
                    if (__elements__ != NULL) {
                        for (int i = 0; i < maxSize; i++)
                            __elements__ [i].~queueType ();
                        free (__elements__);
                    }
                #endif
            }


           /*
            * Returns the number of elements in queue.
            */

            int size () { return __size__; }


           /*
            * Returns storage capacity = the number of elements that can fit into the queue (which is maxSize).
            * 
            */

            int capacity () { return __elements__ ? maxSize : 0; }


           /*
            * Checks if queue is empty.
            */

            bool empty () { return __size__ == 0; }


           /*
            * Clears all the elements from the queue.
            */

            void clear () { 
                __size__ = __front__ = 0;
            } 


           /*
            *  [] operator enables elements of queue to be addressed by their positions (indexes) like:
            *  
            *    for (int i = 0; i < E.size (); i++)
            *      Serial.printf ("E [%i] = %i\n", i, E [i]);    
            *    
            *    or
            *    
            *     E [0] = E [1];
            *     
            *  If the index is not a valid index, the result is unpredictable
            */
      
            queueType &operator [] (int position) {
                if (position < 0 || position >= __size__) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_out_of_range;
                    #endif                      
                    __errorFlags__ |= err_out_of_range;                    
                }
                return __elements__ [(__front__ + position) % maxSize];
            }

    
           /*
            *  Same as [] operator, so it is not really needed but added here because it is supported in STL C++ queues
            */      

            queueType &at (int position) {
                if (position < 0 || position >= __size__) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_out_of_range;
                    #endif                      
                    __errorFlags__ |= err_out_of_range;                    
                }
                return __elements__ [(__front__ + position) % maxSize];
            }

          
           /*
            *  Adds element to the end of a queue, like:
            *  
            *    Q.push_back (700);
            *    
            *  Returns OK or one of the error flags in case of error:
            *    - could not allocate enough memory for requested storage
            */
    
            signed char push_back (queueType element) {
                #if QUEUE_MEMORY_TYPE != STACK_OR_GLOBAL_MEM
                    if (__elements__ == NULL) {
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }
                #endif

                // add the new element at the end = (__front__ + __size__) % __capacity__
                if (__size__ == maxSize) {
                    int i = (__front__ + __size__) % maxSize;
                    queueType tmp = __elements__ [i];
                    __elements__ [i] = element;
                    __backPtr__ = &__elements__ [i];
                    __front__ = (__front__ + 1) % maxSize;
                    __frontPtr__ = &__elements__ [__front__];                    
                    pushed_back (element);
                    popped_front (tmp);
                } else {
                    int i = (__front__ + __size__) % maxSize;
                    __elements__ [i] = element;
                    __backPtr__ = &__elements__ [i];
                    __size__ ++;
                    pushed_back (element);
                }
                return err_ok;
            }


           /*
            * pop_front
            */
        
            signed char pop_front () {
                if (__size__ == 0) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        if (__size__ == 0) throw err_out_of_range;
                    #endif          
                    __errorFlags__ |= err_out_of_range;                                      
                    return err_out_of_range;
                }

                // remove the first element
                __front__ = (__front__ + 1) % maxSize;
                __size__ --;
                popped_front (__elements__ [__front__]);
        
                return err_ok;
            }


            // overrride these functions in inherited class if needed
            virtual void pushed_back (queueType& element) {}
            virtual void popped_front (queueType& element) {}

            queueType *front () { return __frontPtr__; }
            queueType *back () { return __backPtr__; }

           /*
            *  Iterator is needed in order for STL C++ for each loop to work. 
            *  A good source for iterators is: https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp
            *  
            *  Example:
            *  
            *    for (auto element: A) 
            *      Serial.println (element);
            */
      
            class Iterator {
                public:
                              
                    // constructor with element index
                    Iterator (queue* q, int pos) { 
                        __queue__ = q; 
                        __position__ = pos;
                    }

                    // constructor with element reference (pointer)
                    Iterator (queue* q, queueType *qPtr) {
                        __queue__ = q; 
                        __position__ = ((qPtr - q->__elements__) + (maxSize - q->__front__)) % maxSize; // calculate poistion from reference (pointer)
                    }
                    
                    // * operator
                    queueType& operator *() const { return __queue__->at (__position__); }
                
                    // ++ (prefix) increment
                    Iterator& operator ++ () { __position__ ++; return *this; }
          
                    // C++ will stop iterating when != operator returns false, this is when __position__ counts to queue.size ()
                    // friend bool operator != (const Iterator& a, const Iterator& b) { return a.__position__ != a.__queue__->size (); }
                    friend bool operator != (const Iterator& a, const Iterator& b) { return a.__position__ <= b.__position__; }

                    // this will tell if iterator is valid (if the queue doesn't have elements the iterator can not be valid)
                    operator bool () const { return __queue__->size () > 0; }
          
                private:
        
                    queue* __queue__;
                    int __position__;
                    
            };      
      
            Iterator begin () { return Iterator (this, 0); }  

            Iterator begin (queueType *p) { 
              int absPos = (p - __elements__) / sizeof (queueType);
              int relPos = absPos >= __front__ ?  absPos - __front__ :  maxSize - (__front__ - absPos);
              return Iterator (this, relPos);
            } // start iterating at a given element address - calculate position from it

            Iterator end () { return Iterator (this, this->size () - 1); }


      private:

            #if QUEUE_MEMORY_TYPE == STACK_OR_GLOBAL_MEM
                queueType __elements__ [maxSize];
            #else // HEAP_MEM or PSRAM_MEM: memory will be allocated when constructor is called
                queueType *__elements__ = NULL;
            #endif

            int __size__ = 0;                 // initially there are not elements in __elements__
            int __front__ = 0;                // points to the first element in __elements__, which do not exist yet at instance creation time

            queueType *__frontPtr__ = NULL;
            queueType *__backPtr__ = NULL;
    };


   /*
    * Arduino String queue template specialization
    */

    template <size_t maxSize> class queue <String, maxSize> {

        private: 

            signed char __errorFlags__ = 0;


        public:

            signed char errorFlags () { return __errorFlags__ & 0b01111111; }
            void clearErrorFlags () { __errorFlags__ = 0; }

      
           /*
            *  Constructor of queue:
            *  
            *     queue<String, 100> Q;
            */
      
            queue () {
                // allocate new memory for the queue
                #if QUEUE_MEMORY_TYPE == PSRAM_MEM
                    __elements__ = (String *) ps_malloc (sizeof (String) * maxSize);
                #elif QUEUE_MEMORY_TYPE == HEAP_MEM
                    __elements__ = (String *) malloc (sizeof (String) * maxSize);
                #else // use stack or global memory
                    // __elemets__ aray is already created
                #endif

                if (__elements__ == NULL) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                }

                #ifndef ARDUINO_ARCH_AVR // Assuming Arduino Mega or Uno
                    new (__elements__) String [maxSize]; 
                #else                 
                    // if this is not supported by older boards, we can use the following instead:
                    memset (__elements__, 0, sizeof (String) * maxSize); // prevent caling String destructors at the following assignments
                    for (int i = 0; i < maxSize; i++) __elements__ [i] = String (); // assign empty String
                #endif

                if (__elements__ == NULL) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                }
            }


           /*
            * queue destructor - free the memory occupied by queue elements
            */
            
            ~queue () { 
                #if QUEUE_MEMORY_TYPE != STACK_OR_GLOBAL_MEM
                    if (__elements__ != NULL) {
                        #ifndef ARDUINO_ARCH_AVR // Assuming Arduino Mega or Uno
                            for (int i = 0; i < maxSize; i++)
                                __elements__ [i].~String ();
                        #endif
                        free (__elements__);
                    }
                #endif
            }


           /*
            * Returns the number of elements in queue.
            */

            int size () { return __size__; }


           /*
            * Returns storage capacity = the number of elements that can fit into the queue (which is maxSize).
            * 
            */

            int capacity () { return __elements__ ? maxSize : 0; }


           /*
            * Checks if queue is empty.
            */

            bool empty () { return __size__ == 0; }


           /*
            * Clears all the elements from the queue.
            */

            void clear () { 
                __size__ = __front__ = 0;
            } 


           /*
            *  [] operator enables elements of queue to be addressed by their positions (indexes) like:
            *  
            *    for (int i = 0; i < E.size (); i++)
            *      Serial.printf ("E [%i] = %i\n", i, E [i]);    
            *    
            *    or
            *    
            *     E [0] = E [1];
            *     
            *  If the index is not a valid index, the result is unpredictable
            */
      
            String &operator [] (int position) {
                if (position < 0 || position >= __size__) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_out_of_range;
                    #endif                      
                    __errorFlags__ |= err_out_of_range;                    
                }
                return __elements__ [(__front__ + position) % maxSize];
            }

    
           /*
            *  Same as [] operator, so it is not really needed but added here because it is supported in STL C++ queues
            */      

            String &at (int position) {
                if (position < 0 || position >= __size__) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        throw err_out_of_range;
                    #endif                      
                    __errorFlags__ |= err_out_of_range;                    
                }
                return __elements__ [(__front__ + position) % maxSize];
            }

          
           /*
            *  Adds element to the end of a queue, like:
            *  
            *    Q.push_back ("seven hundred");
            *    
            *  Returns OK or one of the error flags in case of error:
            *    - could not allocate enough memory for requested storage
            */
    
            signed char push_back (String element) {
                #if QUEUE_MEMORY_TYPE != STACK_OR_GLOBAL_MEM
                    if (__elements__ == NULL) {
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }
                #endif

                if (!element) {                        // ... check if parameter construction is valid
                    __errorFlags__ |= err_bad_alloc;       // report error if it is not
                    return err_bad_alloc;
                }

                // add the new element at the end = (__front__ + __size__) % __capacity__
                if (__size__ == maxSize) {
                    int i = (__front__ + __size__) % maxSize;
                    String tmp;
                    __swapStrings__ (&tmp, &__elements__ [i]);
                    __swapStrings__ (&element, &__elements__ [i]);
                    __backPtr__ = &__elements__ [i];
                    __front__ = (__front__ + 1) % maxSize;
                    __frontPtr__ = &__elements__ [__front__];                      
                    pushed_back (element);
                    popped_front (tmp);
                } else {
                    int i = (__front__ + __size__) % maxSize;
                    __swapStrings__ (&element, &__elements__ [i]);
                    __backPtr__ = &__elements__ [i];
                    __size__ ++;
                    pushed_back (element);
                }
                return err_ok;
            }


           /*
            * pop_front
            */
        
            signed char pop_front () {
                if (__size__ == 0) {
                    #ifdef __THROW_QUEUE_EXCEPTIONS__
                        if (__size__ == 0) throw err_out_of_range;
                    #endif          
                    __errorFlags__ |= err_out_of_range;                                      
                    return err_out_of_range;
                }

                // remove first element
                __front__ = (__front__ + 1) % maxSize;
                __size__ --;
                popped_front (__elements__ [__front__]);
                delete __elements__ [__front__];
        
                return err_ok;
            }


            // overrride these functions in inherited class if needed
            virtual void pushed_back (String& element) {};
            virtual void popped_front (String& element) {};

            String *front () { return __frontPtr__; }
            String *back () { return __backPtr__; }


           /*
            *  Iterator is needed in order for STL C++ for each loop to work. 
            *  A good source for iterators is: https://www.internalpointers.com/post/writing-custom-iterators-modern-cpp
            *  
            *  Example:
            *  
            *    for (auto element: A) 
            *      Serial.println (element);
            */
      
            class Iterator {
                public:
                              
                    // constructor
                    Iterator (queue* q, int pos) { 
                        __queue__ = q; 
                        __position__ = pos;
                    }
                    
                    // * operator
                    String& operator *() const { return __queue__->at (__position__); }
                
                    // ++ (prefix) increment
                    Iterator& operator ++ () { __position__ ++; return *this; }
          
                    // C++ will stop iterating when != operator returns false, this is when __position__ counts to queue.size ()
                    // friend bool operator != (const Iterator& a, const Iterator& b) { return a.__position__ != a.__queue__->size (); }
                    friend bool operator != (const Iterator& a, const Iterator& b) { return a.__position__ <= b.__position__; }

                    // this will tell if iterator is valid (if the queue doesn't have elements the iterator can not be valid)
                    operator bool () const { return __queue__->size () > 0; }
          
                private:
        
                    queue* __queue__;
                    int __position__;
                    
            };      
      
            Iterator begin () { return Iterator (this, 0); }  
            Iterator end () { return Iterator (this, this->size () - 1); }


      private:

            #if QUEUE_MEMORY_TYPE == STACK_OR_GLOBAL_MEM
                String __elements__ [maxSize];
            #else // HEAP_MEM or PSRAM_MEM: memory will be allocated when constructor is called
                String *__elements__ = NULL;
            #endif

            int __size__ = 0;                 // initially there are not elements in __elements__
            int __front__ = 0;                // points to the first element in __elements__, which do not exist yet at instance creation time

            String *__frontPtr__ = NULL;
            String *__backPtr__ = NULL;

            // swap strings by swapping their stack memory so constructors doesn't get called and nothing can go wrong like running out of memory meanwhile 
            void __swapStrings__ (String *a, String *b) {
                char tmp [sizeof (String)];
                memcpy (&tmp, a, sizeof (String));
                memcpy (a, b, sizeof (String));
                memcpy (b, tmp, sizeof (String));
            }

    };

#endif
