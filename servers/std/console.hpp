/*
 *  console.hpp for Arduino (ESP boards)
 * 
 *  This file is part of C++ std package for Arduino: https://github.com/BojanJurca/console-string-vector-map-for-Arduino
 * 
 *  April 20, 2024, Bojan Jurca
 *  
 */


#ifndef __CONSOLE_HPP__
    #define __CONSOLE_HPP__


    // ----- TUNNING PARAMETERS -----

    #define __CONSOLE_BUFFER_SIZE__ 64 // max 63 characters in internal buffer


    // ----- CODE -----


    // Serial initialization
    #ifdef ARDUINO_ARCH_AVR // Assuming Arduino Mega or Uno
        void cinit (bool waitForSerial = false, unsigned int serialSpeed = 9600, unsigned int waitAfterSerial = 1000) {
            Serial.begin (serialSpeed);
            if (waitForSerial)
                while (!Serial) 
                    delay (10);
            delay (waitAfterSerial);
        }
    #else
        void cinit (bool waitForSerial = false, unsigned int serialSpeed = 115200, unsigned int waitAfterSerial = 1000) {
            Serial.begin (serialSpeed);
            if (waitForSerial)
                while (!Serial) 
                    delay (10);
            delay (waitAfterSerial);
        }
    #endif


    // cout
    #define endl "\r\n"

    class Cout {

      public:

        template<typename T>
        Cout& operator << (const T& value) {
            Serial.print (value);            
            return *this;
        }        

        #ifdef __CSTRING_HPP__
            // Cout << Ctring<N>
            template<size_t N>
            Cout& operator << (const Cstring<N>& value) {
                Serial.print ((char *) &value);
                return *this;
            }
        #endif

        #ifdef __VECTOR_HPP__
            // Cout << vector<T>
            template<typename T>
            Cout& operator << (vector<T>& value) {
                bool first = true;
                Serial.print ("[");
                for (auto e: value) {
                    if (!first)
                        Serial.print (",");
                    first = false;
                    this->operator<<(e);
                }
                Serial.print ("]");
                return *this;
            }
        #endif

        #ifdef __QUEUE_HPP__
            // Cout << queue<T, N>
            template<typename T, size_t N>
            Cout& operator << (queue<T, N>& value) {
                bool first = true;
                Serial.print ("[");
                for (auto e: value) {
                    if (!first)
                        Serial.print (",");
                    first = false;
                    this->operator<<(e);
                }
                Serial.print ("]");
                return *this;
            }
        #endif

        #ifdef __MAP_HPP__
            // Cout << vector<T>
            template<typename K, typename V>
            Cout& operator << (Map<K, V>& value) {
                bool first = true;
                Serial.print ("[");
                for (auto e: value) {
                    if (!first)
                        Serial.print (",");
                    first = false;
                    this->operator<<(e->key); 
                    Serial.print ("=");
                    this->operator<<(e->value);
                }
                Serial.print ("]");
                return *this;
            }
        #endif

    };

    // Create a working instnces
    Cout cout;


    // cin
    class Cin {

      private:

          char buf [__CONSOLE_BUFFER_SIZE__];

      public:

        // Cin >> char
        Cin& operator >> (char& value) {
            while (!Serial.available ()) delay (10);
            value = Serial.read ();            
            return *this;
        }        

        // Cin >> int
        Cin& operator >> (int& value) {
            buf [0] = 0;
            int i = 0;
            while (i < __CONSOLE_BUFFER_SIZE__ - 1) {
                while (!Serial.available ()) delay (10);
                char c = Serial.read ();
                if (c > ' ') {
                    buf [i] = c;
                    buf [++ i] = 0;
                } else {
                    value = atoi (buf);
                    break;
                }
            }
            return *this;
        } 

        // Cin >> float
        Cin& operator >> (float& value) {
            buf [0] = 0;
            int i = 0;
            while (i < __CONSOLE_BUFFER_SIZE__ - 1) {
                while (!Serial.available ()) delay (10);
                char c = Serial.read ();
                if (c > ' ') {
                    buf [i] = c;
                    buf [++ i] = 0;
                } else {
                    value = atof (buf);
                    break;
                }
            }
            return *this;
        } 

        // Cin >> char * // warning, it doesn't chech buffer overflow
        Cin& operator >> (char *value) {
            buf [0] = 0;
            int i = 0;
            while (i < __CONSOLE_BUFFER_SIZE__ - 1) {
                while (!Serial.available ()) delay (10);
                char c = Serial.read ();
                if (c > ' ') {
                    buf [i] = c;
                    buf [++ i] = 0;
                } else {
                    strcpy (value, buf);
                    break;
                }
            }
            return *this;
        }

        // Cin >> String
        Cin& operator >> (String& value) {
            buf [0] = 0;
            int i = 0;
            while (i < __CONSOLE_BUFFER_SIZE__ - 1) {
                while (!Serial.available ()) delay (10);
                char c = Serial.read ();
                if (c > ' ') {
                    buf [i] = c;
                    buf [++ i] = 0;
                } else {
                    value = String (buf);
                    break;
                }
            }
            return *this;
        }


        // Cin >> any other class that has a constructor of type T (char *)
        template<typename T>
        Cin& operator >> (T& value) {
            buf [0] = 0;
            int i = 0;
            while (i < __CONSOLE_BUFFER_SIZE__ - 1) {
                while (!Serial.available ()) delay (10);
                char c = Serial.read ();
                if (c > ' ') {
                    buf [i] = c;
                    buf [++ i] = 0;
                } else {
                    value = T (buf);
                    break;
                }
            }
            return *this;
        }

    };

    // Create a working instnces
    Cin cin;

#endif
