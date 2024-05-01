/*
 * Cstring.hpp for Arduino (ESP boards)
 * 
 * This file is part of C++ std package for Arduino: https://github.com/BojanJurca/console-string-vector-map-for-Arduino
 * 
 * Bojan Jurca, April 27, 2024
 *  
 */


#ifndef __CSTRING_HPP__
    #define __CSTRING_HPP__


        // missing C function in Arduino
        char *stristr (const char *haystack, const char *needle) { 
            if (!haystack || !needle) return NULL; // nonsense
            int n = strlen (haystack) - strlen (needle) + 1;
            for (int i = 0; i < n; i++) {
                int j = i;
                int k = 0;
                char hChar, nChar;
                bool match = true;
                while (*(needle + k)) {
                    hChar = *(haystack + j); if (hChar >= 'a' && hChar <= 'z') hChar -= 32; // convert to upper case
                    nChar = *(needle + k); if (nChar >= 'a' && nChar <= 'z') nChar -= 32; // convert to upper case
                    if (hChar != nChar && nChar) {
                        match = false;
                        break;
                    }
                    j ++;
                    k ++;
                }
                if (match) return (char *) haystack + i; // match!
            }
            return NULL; // no match
        }


    // ----- TUNNING PARAMETERS -----

    #ifndef string
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Cstring with default Cstring size was not defined previously, #defining the default string as Cstring<300> in Cstring.hpp" 
        #endif
        #define string Cstring<300>
    #endif


    // error flags: there are only two types of error flags that can be set: OVERFLOW and OUT_OF_RANGE - please note that all errors are negative (char) numbers
    #define OK           ((signed char) 0b00000000) //    0 - no error    
    #define OVERFLOW     ((signed char) 0b10000001) // -127 - buffer overflow
    #define OUT_OF_RANGE ((signed char) 0b10000010) // -126 - invalid index


    // fixed size string, actually C char arrays with additional functions and operators
    template<size_t N> struct Cstring {
        
        private: 

            // internal storage: char array (= 0-terminated C string)
            char __c_str__ [N + 2] = {}; // add 2 characters at the end __c_str__ [N] will detect if string gets too long (owerflow), __c_str__ [N + 1] will guard the end of the string and will always be 0, initialize it with zeros
            signed char __errorFlags__ = 0;
        
        public:
        
            signed char errorFlags () { return __errorFlags__ & 0b01111111; }
            void clearErrorFlags () { __errorFlags__ = 0; }
        
        
            // constructors
            Cstring () {}                                      // for declarations like Cstring<15> a;
            
            Cstring (const char *other) {                      // for declarations with initialization, like Cstring<15> b ("abc");
                if (other) {                                                  // check if NULL char * pointer to overcome from possible programmer's errors
                    strncpy (this->__c_str__, other, N + 1);
                    if (this->__c_str__ [N]) {
                         __errorFlags__ = OVERFLOW;                           // OVEVRFLOW
                        this->__c_str__ [N] = 0;                              // mark the end of the string regardles OVERFLOW
                    }
                } 
            }

            Cstring (const Cstring& other) {                  // for declarations with initialization, like Cstring<15> c = a;            
                strncpy (this->__c_str__, other.__c_str__, N + 1);
                this->__errorFlags__ = other.__errorFlags__;                  // inherit all errors from other Cstring
            }
        
            Cstring (const char& other) {                      // for declarations with initialization, like Cstring<15> d ('c'); (convert char to Cstring)
                this->__c_str__ [0] = other;
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (int number) {                             // for declarations with initialization, like Cstring<15> e (3); (convert int to Cstring)
                snprintf (this->__c_str__, N + 2, "%i", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (unsigned int number) {                    // for declarations with initialization, like Cstring<15> e (3); (convert unsigned int to Cstring)
                snprintf (this->__c_str__, N + 2, "%u", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (long number) {                            // for declarations with initialization, like Cstring<15> e (3); (convert long to Cstring)
                snprintf (this->__c_str__, N + 2, "%l", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (unsigned long number) {                   // for declarations with initialization, like Cstring<15> e (3); (convert unsigned long to Cstring)
                snprintf (this->__c_str__, N + 2, "%lu", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (float number) {                           // for declarations with initialization, like Cstring<15> e (3.1); (convert float to Cstring)
                snprintf (this->__c_str__, N + 2, "%f", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }

            Cstring (double number) {                          // for declarations with initialization, like Cstring<15> e (3.1); (convert float to Cstring)
                snprintf (this->__c_str__, N + 2, "%lf", number);
                if (this->__c_str__ [N]) {
                    __errorFlags__ = OVERFLOW;                                // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                }
            }


            // char *() operator so that Cstring can be used the same way as C char arrays, like; Cstring<5> a = "123"; Serial.printf ("%s\n", a);
            inline operator char *() __attribute__((always_inline)) { return __c_str__; }
            

            // = operator
            Cstring operator = (const char *other) {           // for assigning C char array to Cstring, like: a = "abc";
                if (other) {                                                  // check if NULL char * pointer to overcome from possible programmers' errors
                    strncpy (this->__c_str__, other, N + 1);
                    if (this->__c_str__ [N]) {
                        this->__errorFlags__ = OVERFLOW;                      // OVEVRFLOW
                        this->__c_str__ [N] = 0;                              // mark the end of the string regardles OVERFLOW
                    } else {
                        this->__errorFlags__ = 0;                             // clear error after new assignment
                    }
                }
                return *this;
            }
    
            Cstring operator = (const Cstring& other) {       // for assigning other Cstring to Cstring, like: a = b;
                if (this != &other) {
                    strncpy (this->__c_str__, other.__c_str__, N + 1);
                    this->__errorFlags__ = other.__errorFlags__;              // inherit all errors from original string
                }
                return *this;
            }

            template<size_t M>
            Cstring operator = (Cstring<M>& other) {    // for assigning other Cstring to Cstring of different size, like: a = b;
                strncpy (this->__c_str__, other.c_str (), N + 1);
                this->__errorFlags__ = other.errorFlags ();                   // inherit all errors from original string
                return *this;
            }

            Cstring operator = (const char& other) {           // for assigning character to Cstring, like: a = 'b';
                this->__c_str__ [0] = other; 
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                #if N > 0
                    this->__c_str__ [1] = 0;                                  // mark the end of the string
                #else
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                #endif
                return *this;
            }   

            Cstring operator = (int number) {                   // for assigning int to Cstring, like: a = 1234;
                snprintf (this->__c_str__, N + 2, "%i", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }

            Cstring operator = (unsigned int number) {           // for assigning unsigned int to Cstring, like: a = 1234;
                snprintf (this->__c_str__, N + 2, "%u", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles the OVERFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }

            Cstring operator = (long number) {                   // for assigning long to Cstring, like: a = 1234;
                snprintf (this->__c_str__, N + 2, "%l", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }

            Cstring operator = (unsigned long number) {          // for assigning unsigned long to Cstring, like: a = 1234;
                snprintf (this->__c_str__, N + 2, "%lu", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }

            Cstring operator = (float number) {                  // for assigning float to Cstring, like: a = 1234.5;
                snprintf (this->__c_str__, N + 2, "%f", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVEFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }

            Cstring operator = (double number) {                 // for assigning double to Cstring, like: a = 1234.5;
                snprintf (this->__c_str__, N + 2, "%lf", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ = OVERFLOW;                          // OVEVRFLOW
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } else {
                    this->__errorFlags__ = 0;                                 // clear error after new assignment
                }
                return *this;
            }


            // += operator
            Cstring operator += (const char *other) {          // concatenate C string to Cstring, like: a += "abc";
                if (other) {                                                  // check if NULL char * pointer to overcome from possible programmer's errors
                    strncat (this->__c_str__, other, N + 1 - strlen (this->__c_str__));
                    if (this->__c_str__ [N]) {
                        this->__errorFlags__ |= OVERFLOW;                     // add OVERFLOW flag to possibe already existing error flags
                        this->__c_str__ [N] = 0;                              // mark the end of the string regardles OVERFLOW
                    } 
                }
                return *this;
            }

            Cstring operator += (const Cstring& other) {      // concatenate one Cstring to anoterh, like: a += b;
                strncat (this->__c_str__, other.__c_str__, N + 1 - strlen (this->__c_str__));
                this->__errorFlags__ |= other.__errorFlags__;                 // add all errors from other string
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }

            template<size_t M>
            Cstring operator += (Cstring<M>& other) {      // concatenate one Cstring to another of different size, like: a += b;
                strncat (this->__c_str__, other.c_str (), N + 1 - strlen (this->__c_str__));
                this->__errorFlags__ |= other.errorFlags ();                  // add all errors from other string
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }

            Cstring operator += (const char& other) {          // concatenate a charactr to Cstring, like: a += 'b';
                size_t l = strlen (this->__c_str__);
                if (l < N) { 
                    this->__c_str__ [l] = other; 
                    this->__c_str__ [l + 1] = 0; 
                } else {
                    __errorFlags__ |= OVERFLOW;                               // add OVERFLOW flag to possibe already existing error flags
                }
                return *this;
            }   

            Cstring operator += (int number) {                 // concatenate an int to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%i", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }

            Cstring operator += (unsigned int number) {        // concatenate an int to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%u", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }   

            Cstring operator += (long number) {                // concatenate a long to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%l", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }   

            Cstring operator += (unsigned long number) {        // concatenate an unsigned long to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%lu", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }   

            Cstring operator += (float number) {                // concatenate a flaot to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%f", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }   

            Cstring operator += (double number) {                // concatenate a double to Cstring, like: a += 12;
                size_t l = strlen (this->__c_str__);
                snprintf (this->__c_str__ + l, N + 2 - l, "%lf", number);
                if (this->__c_str__ [N]) {
                    this->__errorFlags__ |= OVERFLOW;                         // add OVERFLOW flag to possibe already existing error flags
                    this->__c_str__ [N] = 0;                                  // mark the end of the string regardles OVERFLOW
                } 
                return *this;
            }   


            // + operator
            Cstring operator + (const char *other) {             // for adding C string to Cstring, like: a + "abc";
                Cstring<N> tmp = *this; // copy of this, including error information
                tmp += other;
                return tmp;
            }
        
            Cstring operator + (const Cstring& other) {       // for concatenating one Cstring with anoterh, like: a + b;
                Cstring<N> tmp = *this; // copy of this, including error information
                tmp += other;
                return tmp;
            }

            template<size_t M>
            Cstring operator + (Cstring<M>& other) {          // concatenate one Cstring to another of different size, like: a + b;
                Cstring<N> tmp = *this; // copy of this, including error information
                tmp += other;
                return tmp;
            }

            Cstring operator + (const char& other) {           // for adding a character to Cstring, like: a + 'b';
                Cstring<N> tmp = *this; // copy of this, including error information
                tmp += other;
                return tmp;
            } 

            // can't use + operator for integers, this would make impossible to use for example Cstring + int to calculate the pointer to the location 

        
            // logical operators: ==, !=, <, <=, >, >=, ignore all possible errors
            inline bool operator == (const char *other) __attribute__((always_inline))        { return !strcmp (this->__c_str__, other); }              // Cstring : C string   
            inline bool operator == (char *other) __attribute__((always_inline))              { return !strcmp (this->__c_str__, other); }              // Cstring : C string   
            inline bool operator == (const Cstring& other) __attribute__((always_inline))    { return !strcmp (this->__c_str__, other.__c_str__); }    // Cstring : Cstring
            inline bool operator != (const char *other) __attribute__((always_inline))        { return strcmp (this->__c_str__, other); }               // Cstring : C string
            inline bool operator != (char *other) __attribute__((always_inline))              { return strcmp (this->__c_str__, other); }               // Cstring : C string
            inline bool operator != (const Cstring& other) __attribute__((always_inline))    { return strcmp (this->__c_str__, other.__c_str__); }     // Cstring : Cstring
            inline bool operator <  (const char *other) __attribute__((always_inline))        { return strcmp (this->__c_str__, other) < 0; }           // Cstring : C string
            inline bool operator <  (char *other) __attribute__((always_inline))              { return strcmp (this->__c_str__, other) < 0; }           // Cstring : C string
            inline bool operator <  (const Cstring& other) __attribute__((always_inline))    { return strcmp (this->__c_str__, other.__c_str__) < 0; } // Cstring : Cstring
            inline bool operator <= (const char *other) __attribute__((always_inline))        { return strcmp (this->__c_str__, other) <= 0; }          // Cstring : C string
            inline bool operator <= (char *other) __attribute__((always_inline))              { return strcmp (this->__c_str__, other) <= 0; }          // Cstring : C string
            inline bool operator <= (const Cstring& other) __attribute__((always_inline))    { return strcmp (this->__c_str__, other.__c_str__) <= 0; }// Cstring : Cstring
            inline bool operator >  (const char *other) __attribute__((always_inline))        { return strcmp (this->__c_str__, other) > 0; }           // Cstring : C string    
            inline bool operator >  (char *other) __attribute__((always_inline))              { return strcmp (this->__c_str__, other) > 0; }           // Cstring : C string    
            inline bool operator >  (const Cstring& other) __attribute__((always_inline))    { return strcmp (this->__c_str__, other.__c_str__) > 0; } // Cstring : Cstring
            inline bool operator >= (const char *other) __attribute__((always_inline))        { return strcmp (this->__c_str__, other) >= 0; }          // Cstring : C string    
            inline bool operator >= (char *other) __attribute__((always_inline))              { return strcmp (this->__c_str__, other) >= 0; }          // Cstring : C string    
            inline bool operator >= (const Cstring& other) __attribute__((always_inline))    { return strcmp (this->__c_str__, other.__c_str__) >= 0; }// Cstring : Cstring


            // [] operator
            inline char &operator [] (size_t i) __attribute__((always_inline)) { return __c_str__ [i]; }
            inline char &operator [] (int i) __attribute__((always_inline)) { return __c_str__ [i]; }
            // inline char &operator [] (unsigned int i) __attribute__((always_inline)) { return __c_str__ [i]; }
            inline char &operator [] (long i) __attribute__((always_inline)) { return __c_str__ [i]; }
            inline char &operator [] (unsigned long i) __attribute__((always_inline)) { return __c_str__ [i]; }


            // some std::string-like member functions
            inline char *c_str () __attribute__((always_inline)) { return __c_str__; } // not really needed, use char *() operator instead
        
            inline size_t length () __attribute__((always_inline)) { return strlen (__c_str__); } 
            
            inline size_t max_size () __attribute__((always_inline)) { return N; } 

            Cstring<N> substr (size_t pos = 0, size_t len = N + 1) {
                Cstring<N> r;
                r.__errorFlags__ = this->__errorFlags__;                      // inherit all errors from original string
                if (pos >= strlen (this->__c_str__)) {
                    r.__errorFlags__ |= OUT_OF_RANGE;
                } else {
                    strncpy (r.__c_str__, this->__c_str__ + pos, len);        // can't overflow 
                }
                return r;
            }

            static const size_t npos = (size_t) 0xFFFFFFFFFFFFFFFF;

            size_t find (const char *str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str);
                if (p)  return p - __c_str__;
                return npos;
            }

            size_t find (Cstring str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str.__c_str__);
                if (p)  return p - __c_str__;
                return npos;
            }

            size_t rfind (char *str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str);
                char *q = NULL;
                while (p) { q = p; p = strstr (p + 1, str); }
                if (q) return q - __c_str__;
                return npos;
            }            

            size_t rfind (Cstring str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str.__c_str__);
                char *q = NULL;
                while (p) { q = p; p = strstr (p + 1, str); }
                if (q) return q - __c_str__;
                return npos;
            }            

            void erase (size_t pos = 0) {
                if (pos > N) pos = N;
                __c_str__ [pos] = 0;
            }
        
            // some Arduino String-like member functions
            Cstring<N> substring (size_t from = 0, size_t to = N - 1) {
                Cstring<N> r;
                r.__errorFlags__ = this->__errorFlags__;                      // inherit all errors from original string
                if (from >= strlen (this->__c_str__) || to < from) {
                    r.__errorFlags__ |= OUT_OF_RANGE;
                } else {
                    strncpy (r.__c_str__, this->__c_str__ + from, to - from); // can't overflow 
                }
                return r;
            }

            int indexOf (const char *str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str);
                if (p)  return p - __c_str__;
                return -1;
            }

            int indexOf (Cstring str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str.__c_str__);
                if (p)  return p - __c_str__;
                return -1;
            }

            int lastIndexOf (char *str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str);
                char *q = NULL;
                while (p) { q = p; p = strstr (p + 1, str); }
                if (q) return q - __c_str__;
                return -1;
            }

            int lastIndexOf (Cstring str, size_t pos = 0) {
                char *p = strstr (__c_str__ + pos, str.__c_str__);
                char *q = NULL;
                while (p) { q = p; p = strstr (p + 1, str); }
                if (q) return q - __c_str__;
                return -1;
            }

            bool endsWith (char *str) { 
              size_t lStr = strlen (str);
              size_t lThis = strlen (__c_str__);
              if (lStr > lThis) return false;
              return !strcmp (str, __c_str__ + (lThis - lStr));
            }

            void remove (size_t pos = 0) {
                if (pos > N) pos = N;
                __c_str__ [pos] = 0;
            }

            void trim () {
                lTrim ();
                rTrim ();
            }
        
        
            // add other functions that may be useful 
            void lTrim () {
                size_t i = 0;
                while (__c_str__ [i ++] == ' ');
                if (i) strcpy (__c_str__, __c_str__ + i - 1);
            }    
            
            void rTrim () {
                size_t i = strlen (__c_str__);
                while (__c_str__ [-- i] == ' ');
                __c_str__ [i + 1] = 0;
            }

            void rPad (size_t toLength, char withChar) {
                if (toLength > N) {
                  toLength = N;
                  __errorFlags__ |= OVERFLOW;                                 // error? overflow?
                }
                size_t l = strlen (__c_str__);
                while (l < toLength) __c_str__ [l ++] = withChar;
                __c_str__ [l] = 0;
            }

    };

#endif
