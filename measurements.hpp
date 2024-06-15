/*
  
    measurements.hpp 
  
    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
  
    Measurements.hpp include circular queue for storing measurements data set.
  
    May 1, 2024, Bojan Jurca

*/


#include "servers/threadSafeCircularqueue.hpp"


#ifndef __MEASUREMENTS__
    #define __MEASUREMENTS__
    
    struct measurement_t {
        unsigned char scale;
        int           value;
    };


     // define measurements circular queue
    template<size_t maxSize> class measurements : public threadSafeCircularQueue<measurement_t, maxSize> {

        public:

            void increase_valueCounter () {
                threadSafeCircularQueue<measurement_t, maxSize>::Lock ();
                __valueCounter__ ++;
                threadSafeCircularQueue<measurement_t, maxSize>::Unlock ();
            }

            void reset_valueCounter () {
                threadSafeCircularQueue<measurement_t, maxSize>::Lock ();
                __valueCounter__ = 0;
                threadSafeCircularQueue<measurement_t, maxSize>::Unlock ();
            }

            void push_back_and_reset_valueCounter (unsigned char scale) {
                threadSafeCircularQueue<measurement_t, maxSize>::Lock ();
                threadSafeCircularQueue<measurement_t, maxSize>::push_back ( { scale, __valueCounter__ } );
                __valueCounter__ = 0;
                threadSafeCircularQueue<measurement_t, maxSize>::Unlock ();
            }

            String toJson () {
                bool first = true;
                String s1 = "{\"scale\":[";
                String s2 = "],\"value\":[";

                for (auto e = threadSafeCircularQueue<measurement_t, maxSize>::begin (); e != threadSafeCircularQueue<measurement_t, maxSize>::end (); ++ e) { // scan measurements with iterator where the locking mechanism is already implemented
                    if (!first) {
                        s1 += ",";
                        s2 += ",";
                    }
                    first = false;
                    s1 += "\"" + String ((*e).scale) + "\"";
                    s2 += "\"" + String ((*e).value) + "\"";
                }

                s2 += "]}\r\n";
                return s1 + s2;
            }

        private:

            int __valueCounter__ = 0; // in case measurements will be entered by counting
    
    };

#endif    
