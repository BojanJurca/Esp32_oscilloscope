/*
 * keyValueDatabase.hpp for Arduino (ESP boards with flash disk)
 * 
 * This file is part of Key-value-database-for-Arduino: https://github.com/BojanJurca/Key-value-database-for-Arduino
 *
 * Key-value-database-for-Arduino may be used as a simple database with the following functions (see examples in BasicUsage.ino).
 * The functions are thread-safe:
 *
 *    - Insert (key, value)                                   - inserts a new key-value pair
 *
 *    - FindBlockOffset (key)                                 - searches (memory) Map for key
 *    - FindValue (key, optional block offset)                - searches (memory) Map for blockOffset connected to key and then it reads the value from (disk) data file (it works slightly faster if block offset is already known, such as during iterations)
 *
 *    - Update (key, new value, optional block offset)        - updates the value associated by the key (it works slightly faster if block offset is already known, such as during iterations)
 *    - Update (key, callback function, optional blockoffset) - if the calculation is made with existing value then this is prefered method, since calculation is performed while database is being loceks
 *
 *    - Upsert (key, new value)                               - update the value if the key already exists, else insert a new one
 *    - Update (key, callback function, default value)        - if the calculation is made with existing value then this is prefered method, since calculation is performed while database is being loceks
 *
 *    - Delete (key)                                          - deletes key-value pair identified by the key
 *    - Truncate                                              - deletes all key-value pairs
 *
 *    - Iterate                                               - iterate (list) through all the keys and their blockOffsets with an Iterator
 *
 *    - Lock                                                  - locks (takes the semaphore) to (temporary) prevent other taska accessing keyValueDatabase
 *    - Unlock                                                - frees the lock
 *
 * Data storage structure used for keyValueDatabase:
 *
 * (disk) keyValueDatabase consists of:
 *    - data file
 *    - (memory) Map that keep keys and pointers (offsets) to data in the data file
 *    - (memory) vector that keeps pointers (offsets) to free blocks in the data file
 *    - semaphore to synchronize (possible) multi-tasking accesses to keyValueDatabase
 *
 *    (disk) data file structure:
 *       - data file consists consecutive of blocks
 *       - Each block starts with int16_t number which denotes the size of the block (in bytes). If the number is positive the block is considered to be used
 *         with useful data, if the number is negative the block is considered to be deleted (free). Positive int16_t numbers can vary from 0 to 32768, so
 *         32768 is the maximum size of a single data block.
 *       - after the block size number, a key and its value are stored in the block (only if the block is beeing used).
 *
 *    (memory) Map structure:
 *       - the key is the same key as used for keyValueDatabase
 *       - the value is an offset to data file block containing the data, keyValueDatabase' value will be fetched from there. Data file offset is
 *         stored in uint32_t so maximum data file offest can theoretically be 4294967296, but ESP32 files can't be that large.
 *
 *    (memory) vector structure:
 *       - a free block list vector contains structures with:
 *            - data file offset (uint16_t) of a free block
 *            - size of a free block (int16_t)
 * 
 * May 22, 2024, Bojan Jurca
 *  
 */


#ifndef __KEY_VALUE_DATABASE_HPP__
    #define __KEY_VALUE_DATABASE_HPP__


    // ----- TUNNING PARAMETERS -----

    #define __KEY_VALUE_DATABASE_PCT_FREE__ 0.2 // how much space is left free in data block to let data "breed" a little - only makes sense for String values 

    // #define __USE_KEY_VALUE_DATABASE_EXCEPTIONS__   // uncomment this line if you want Map to throw exceptions



    // ----- CODE -----
    
    #include "std/Map.hpp"
    #include "std/vector.hpp"

    // error flags - only tose not defined in Map.hpp, please, note that all error flgs are negative (char) numbers
    #define err_data_changed    ((signed char) 0b10010000) // -112 - unexpected data value found
    #define err_file_io         ((signed char) 0b10100000) //  -96 - file operation error
    #define err_cant_do_it_now  ((signed char) 0b11000000) //  -64 - for example changing the data while iterating or loading the data if already loaded

    #ifdef SEMAPHORE_H // RTOS is running beneath Arduino sketch, multitasking (and semaphores) is supported
        static SemaphoreHandle_t __keyValueDatabaseSemaphore__ = xSemaphoreCreateMutex (); 
    #endif

    template <class keyType, class valueType> class keyValueDatabase : private Map<keyType, uint32_t> {
  
        private:

            signed char __errorFlags__ = 0;


        public:

            signed char errorFlags () { return __errorFlags__ & 0b01111111; }
            void clearErrorFlags () { __errorFlags__ = 0; }


           /*
            *  Constructor of keyValueDatabase that does not load the data. Subsequential call to loadData function is needed:
            *  
            *     keyValueDatabase<int, String> pkvpA;
            *
            *     void setup () {
            *         Serial.begin (115200);
            *
            *         fileSystem.mountFAT (true);    
            *
            *         kvp.loadData ("/keyValueDatabase/A.kvp");
            *
            *          ...
            */

            keyValueDatabase () {
                // log_i ("keyValueDatabase ()");
            }

           /*
            *  Constructor of keyValueDatabase that also loads the data from data file: 
            *  
            *     keyValueDatabase<int, String> pkvpA ("/keyValueDatabase/A.kvp");
            *     if (pkvpA.lastErrorCode != pkvpA.OK) 
            *         Serial.printf ("pkvpA constructor failed: %s, all the data may not be indexed\n", pkvpA.errorCodeText (pkvpA.lastErrorCode));
            *
            */
            
            keyValueDatabase (const char *dataFileName) { 
                // log_i ("keyValueDatabase (dataFileName)");
                loadData (dataFileName);
            }

            ~keyValueDatabase () { 
                // log_i ("~keyValueDatabase");
                if (__dataFile__) {
                    __dataFile__.close ();
                }
            } 


           /*
            *  Loads the data from data file.
            *  
            */

            signed char loadData (const char *dataFileName) {
                // log_i ("(dataFileName)");
                Lock ();
                if (__dataFile__) {
                    // log_e ("data already loaded error: err_cant_do_it_now");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_cant_do_it_now;
                    #endif
                    __errorFlags__ |= err_cant_do_it_now;
                    Unlock (); 
                    return err_cant_do_it_now;
                }

                // load new data
                strcpy (__dataFileName__, dataFileName);

                __dataFile__ = fileSystem.open (dataFileName, "r+"); // , false);
                if (!__dataFile__) {
                    __dataFile__ = fileSystem.open (dataFileName, "w"); // , true);
                    if (__dataFile__) {
                          __dataFile__.close (); 
                          __dataFile__ = fileSystem.open (dataFileName, "r+"); // , false);
                    } else {
                        Unlock (); 
                        // log_e ("error opening the data file: err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock ();
                        return err_file_io;
                    }
                }

                if (__dataFile__.isDirectory ()) {
                    __dataFile__.close ();
                    // log_e ("error data file shouldn't be a directory: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }

                __dataFileSize__ = __dataFile__.size ();         
                uint64_t blockOffset = 0;

                while (blockOffset < __dataFileSize__ &&  blockOffset <= 0xFFFFFFFF) { // max uint32_t
                    int16_t blockSize;
                    keyType key;
                    valueType value;

                    signed char e = __readBlock__ (blockSize, key, value, (uint32_t) blockOffset, true);
                    if (e) { // != OK
                        // log_e ("error reading the data block: err_file_io");
                        __dataFile__.close ();
                        Unlock (); 
                        return e;
                    }
                    if (blockSize > 0) { // block containining the data -> insert into Map
                        signed char e = Map<keyType, uint32_t>::insert (key, (uint32_t) blockOffset);
                        if (e) { // != OK
                            // log_e ("keyValuePairs.insert failed failed");
                            __dataFile__.close ();
                            __errorFlags__ |= Map<keyType, uint32_t>::errorFlags ();
                            Unlock (); 
                            return e;
                        }
                    } else { // free block -> insert into __freeBlockList__
                        blockSize = (int16_t) -blockSize;
                        signed char e = __freeBlocksList__.push_back ( {(uint32_t) blockOffset, blockSize} );
                        if (e) { // != OK
                            // log_e ("freeeBlockList.push_back failed failed");
                            __dataFile__.close ();
                            __errorFlags__ |= __freeBlocksList__.errorFlags ();
                            Unlock (); 
                            return e;
                        }
                    } 

                    blockOffset += blockSize;
                }

                Unlock (); 
                // log_i ("OK");
                return err_ok;
            }


           /*
            *  Returns true if the data has already been successfully loaded from data file.
            *  
            */

            bool dataLoaded () {
                return __dataFile__;
            }


           /*
            * Returns the lenght of a data file.
            */

            unsigned long dataFileSize () { return __dataFileSize__; } 


           /*
            * Returns the number of key-value pairs.
            */

            int size () { return Map<keyType, uint32_t>::size (); }


           /*
            *  Inserts a new key-value pair, returns OK or one of the error codes.
            */

            signed char Insert (keyType key, valueType value) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                if (is_same<valueType, String>::value)                                                                        // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &value) {                                                                                 // ... check if parameter construction is valid
                        // log_e ("String value construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock (); 
                if (__inIteration__) {
                    // log_e ("not while iterating, error: err_cant_do_it_now");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_cant_do_it_now;
                    #endif
                    __errorFlags__ |= err_cant_do_it_now;
                    Unlock (); 
                    return err_cant_do_it_now;
                }

                // 1. get ready for writting into __dataFile__
                // log_i ("step 1: calculate block size");
                size_t dataSize = sizeof (int16_t); // block size information
                size_t blockSize = dataSize;
                if (is_same<keyType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    dataSize += (((String *) &key)->length () + 1); // add 1 for closing 0
                    blockSize += (((String *) &key)->length () + 1) + (((String *) &key)->length () + 1) * __KEY_VALUE_DATABASE_PCT_FREE__ + 0.5; // add PCT_FREE for Strings
                } else { // fixed size key
                    dataSize += sizeof (keyType);
                    blockSize += sizeof (keyType);
                }                
                if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    dataSize += (((String *) &value)->length () + 1); // add 1 for closing 0
                    blockSize += (((String *) &value)->length () + 1) + (((String *) &value)->length () + 1) * __KEY_VALUE_DATABASE_PCT_FREE__ + 0.5; // add PCT_FREE for Strings
                } else { // fixed size value
                    dataSize += sizeof (valueType);
                    blockSize += sizeof (valueType);
                }
                if (blockSize > 32768) {
                    // log_e ("block size > 32768, error: err_bad_alloc");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                    Unlock (); 
                    return err_bad_alloc;
                }

                // 2. search __freeBlocksList__ for most suitable free block, if it exists
                // log_i ("step 2: find most suitable free block if it already exists");
                int freeBlockIndex = -1;
                uint32_t minWaste = 0xFFFFFFFF;
                for (int i = 0; i < __freeBlocksList__.size (); i ++) {
                    if (__freeBlocksList__ [i].blockSize >= dataSize && __freeBlocksList__ [i].blockSize - dataSize < minWaste) {
                        freeBlockIndex = i;
                        minWaste = __freeBlocksList__ [i].blockSize - dataSize;
                    }
                }

                // 3. reposition __dataFile__ pointer
                // log_i ("step 3: reposition data file pointer");
                uint32_t blockOffset;                
                if (freeBlockIndex == -1) { // append data to the end of __dataFile__
                    // log_i ("step 3a: appending new block at the end of data file");
                    blockOffset = __dataFileSize__;
                } else { // writte data to free block in __dataFile__
                    // log_i ("step 3b: writing new data to exiisting free block");
                    blockOffset = __freeBlocksList__ [freeBlockIndex].blockOffset;
                    blockSize = __freeBlocksList__ [freeBlockIndex].blockSize;
                }
                if (!__dataFile__.seek (blockOffset, SeekSet)) {
                    // log_e ("seek error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }

                // 4. update (memory) Map structure 
                // log_i ("step 4: insert (key, blockOffset) into Map");
                signed char e = Map<keyType, uint32_t>::insert (key, blockOffset);
                if (e) { // != OK
                    // log_e ("keyValuePairs.insert failed failed");
                    __errorFlags__ |= e;
                    Unlock (); 
                    return e;
                }

                // 5. construct the block to be written
                // log_i ("step 5: construct data block");
                byte *block = (byte *) malloc (blockSize);
                if (!block) {
                    // log_e ("malloc error, out of memory");

                    // 7. (try to) roll-back
                    // log_i ("step 7: try to roll-back");
                    signed char e = Map<keyType, uint32_t>::erase (key);
                    if (e) { // != OK
                        // log_e ("keyValuePairs.erase failed failed, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                        __errorFlags__ |= e;
                        Unlock (); 
                        return e;
                    }
                    // roll-back succeded
                    // log_e ("roll-back succeeded, returning error: err_bad_alloc");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                    Unlock (); 
                    return err_bad_alloc;
                }

                int16_t i = 0;
                int16_t bs = (int16_t) blockSize;
                memcpy (block + i, &bs, sizeof (bs)); i += sizeof (bs);
                if (is_same<keyType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    size_t l = ((String *) &key)->length () + 1; // add 1 for closing 0
                    memcpy (block + i, ((String *) &key)->c_str (), l); i += l;
                } else { // fixed size key
                    memcpy (block + i, &key, sizeof (key)); i += sizeof (key);
                }       
                if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    size_t l = ((String *) &value)->length () + 1; // add 1 for closing 0
                    memcpy (block + i, ((String *) &value)->c_str (), l); i += l;
                } else { // fixed size value
                    memcpy (block + i, &value, sizeof (value)); i += sizeof (value);
                }

                // 6. write block to __dataFile__
                // log_i ("step 6: write block to data file");
                if (__dataFile__.write (block, blockSize) != blockSize) {
                    // log_e ("write failed");
                    free (block);

                    // 9. (try to) roll-back
                    // log_i ("step 9: try to roll-back");
                    if (__dataFile__.seek (blockOffset, SeekSet)) {
                        blockSize = (int16_t) -blockSize;
                        if (__dataFile__.write ((byte *) &blockSize, sizeof (blockSize)) != sizeof (blockSize)) { // can't roll-back
                            // log_e ("write error, can't roll-back, critical error, closing data file");
                            __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                        }
                    } else { // can't roll-back
                        // log_e ("seek error, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                    }
                    __dataFile__.flush ();

                    signed char e = Map<keyType, uint32_t>::erase (key);
                    if (e) { // != OK
                        // log_e ("keyValuePairs.erase failed failed, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                        __errorFlags__ |= Map<keyType, uint32_t>::errorFlags ();
                        Unlock (); 
                        return e;
                    }
                    // roll-back succeded
                    // log_e ("roll-back succeeded, returning error: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }

                // write succeeded
                __dataFile__.flush ();
                free (block);

                // 8. roll-out
                // log_i ("step 8: roll_out");
                if (freeBlockIndex == -1) { // data appended to the end of __dataFile__
                    __dataFileSize__ += blockSize;       
                } else { // data written to free block in __dataFile__
                    __freeBlocksList__.erase (freeBlockIndex); // doesn't fail
                }
                
                // log_i ("OK");
                Unlock (); 
                return err_ok;
            }


           /*
            *  Retrieve blockOffset from (memory) Map, so it is fast.
            */

            signed char FindBlockOffset (keyType key, uint32_t& blockOffset) {
                // log_i ("(key, block offset)");
                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock ();
                Map<keyType, uint32_t>::clearErrorFlags ();
                uint32_t *p = Map<keyType, uint32_t>::find (key);
                if (p) { // if found
                    blockOffset = *p;
                    Unlock ();  
                    // log_i ("OK");
                    return err_ok;
                } else { // not found or error
                    signed char e = Map<keyType, uint32_t>::errorFlags ();
                    if (e == err_not_found) {
                        // log_i ("error (key): NOT_FOUD");
                        __errorFlags__ |= err_not_found;
                        Unlock ();  
                        return err_not_found;
                    } else {
                        // log_i ("error: some other error, check result");
                        __errorFlags__ |= e;
                        Unlock ();  
                        return e;
                    }
                }
            }


           /*
            *  Read the value from (disk) __dataFile__, so it is slow. 
            */

            signed char FindValue (keyType key, valueType *value, uint32_t blockOffset = 0xFFFFFFFF) { 
                // log_i ("(key, *value, block offset)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                keyType storedKey = {};

                Lock (); 

                if (blockOffset == 0xFFFFFFFF) { // if block offset was not specified find it from Map
                    Map<keyType, uint32_t>::clearErrorFlags ();
                    uint32_t *pBlockOffset = Map<keyType, uint32_t>::find (key);
                    if (!pBlockOffset) { // if not found or error
                        signed char e = Map<keyType, uint32_t>::errorFlags ();
                        if (e) { // != OK
                            // log_i ("error (key): NOT_FOUD");
                            __errorFlags__ |= err_not_found;
                            Unlock ();  
                            return err_not_found;
                        } else {
                            // log_i ("error: some other error, check result");
                            __errorFlags__ |= e;
                            Unlock ();  
                            return e;
                        }
                    }
                    blockOffset = *pBlockOffset;
                }

                int16_t blockSize;
                if (!__readBlock__ (blockSize, storedKey, *value, blockOffset)) {
                    if (blockSize > 0 && storedKey == key) {
                        // log_i ("OK");
                        Unlock ();  
                        return err_ok; // success  
                    } else {
                        // log_e ("error that shouldn't happen: err_data_changed");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_data_changed;
                        #endif
                        __errorFlags__ |= err_data_changed;
                        Unlock ();  
                        return err_data_changed; // shouldn't happen, but check anyway ...
                    }
                } else {
                    // log_e ("error reading data block: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock ();  
                    return err_file_io; 
                } 
            }


           /*
            *  Updates the value associated with the key
            */

            signed char Update (keyType key, valueType newValue, uint32_t *pBlockOffset = NULL) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                if (is_same<valueType, String>::value)                                                                        // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &newValue) {                                                                              // ... check if parameter construction is valid
                        // log_e ("String value construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock (); 

                // 1. get blockOffset
                if (!pBlockOffset) { // find block offset if not provided by the calling program
                    // log_i ("step 1: looking for block offset in Map");
                    Map<keyType, uint32_t>::clearErrorFlags ();
                    pBlockOffset = Map<keyType, uint32_t>::find (key);
                    if (!pBlockOffset) { // if not found
                        signed char e = Map<keyType, uint32_t>::errorFlags ();
                        if (e) { // != OK
                            // log_e ("block offset not found for some kind of error occured");
                            __errorFlags__ |= e;
                            Unlock ();  
                            return e;
                        }
                    }
                } else {
                    // log_i ("step 1: block offset already profided by the calling program");
                }

                // 2. read the block size and stored key
                // log_i ("step 2: reading block size from data file");
                int16_t blockSize;
                size_t newBlockSize;
                keyType storedKey;
                valueType storedValue;

                signed char e = __readBlock__ (blockSize, storedKey, storedValue, *pBlockOffset, true);
                if (e) { // != OK
                    // log_e ("read block error");
                    Unlock ();  
                    return err_file_io;
                } 
                if (blockSize <= 0 || storedKey != key) {
                    // log_e ("error that shouldn't happen: err_data_changed");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_data_changed;
                    #endif
                    __errorFlags__ |= err_data_changed;
                    Unlock ();  
                    return err_data_changed; // shouldn't happen, but check anyway ...
                }

                // 3. calculate new block and data size
                // log_i ("step 3: calculate block size");
                size_t dataSize = sizeof (int16_t); // block size information
                newBlockSize = dataSize;
                if (is_same<keyType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    dataSize += (((String *) &key)->length () + 1); // add 1 for closing 0
                    newBlockSize += (((String *) &key)->length () + 1) + (((String *) &key)->length () + 1) * __KEY_VALUE_DATABASE_PCT_FREE__ + 0.5; // add PCT_FREE for Strings
                } else { // fixed size key
                    dataSize += sizeof (keyType);
                    newBlockSize += sizeof (keyType);
                }                
                if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    dataSize += (((String *) &newValue)->length () + 1); // add 1 for closing 0
                    newBlockSize += (((String *) &newValue)->length () + 1) + (((String *) &newValue)->length () + 1) * __KEY_VALUE_DATABASE_PCT_FREE__ + 0.5; // add PCT_FREE for Strings
                } else { // fixed size value
                    dataSize += sizeof (valueType);
                    newBlockSize += sizeof (valueType);
                }
                if (newBlockSize > 32768) {
                    // log_e ("block size > 32768, error: err_bad_alloc");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_bad_alloc;
                    #endif
                    __errorFlags__ |= err_bad_alloc;
                    Unlock (); 
                    return err_bad_alloc;
                }

                // 4. decide where to write the new value: existing block or a new one
                // log_i ("step 4: decide where to writte the new value: same or new block?");
                if (dataSize <= blockSize) { // there is enough space for new data in the existing block - easier case
                    // log_i ("reuse the same block");
                    uint32_t dataFileOffset = *pBlockOffset + sizeof (int16_t); // skip block size information
                    if (is_same<keyType, String>::value) { // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                        dataFileOffset += (((String *) &key)->length () + 1); // add 1 for closing 0
                    } else { // fixed size key
                        dataFileOffset += sizeof (keyType);
                    }                

                    // 5. write new value to __dataFile__
                    // log_i ("step 5: write new value");
                    if (!__dataFile__.seek (dataFileOffset, SeekSet)) {
                        // log_e ("seek error: err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock ();  
                        return err_file_io;
                    }
                    int bytesToWrite;
                    int bytesWritten;
                    if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                        bytesToWrite = (((String *) &newValue)->length () + 1);
                        bytesWritten = __dataFile__.write ((byte *) ((String *) &newValue)->c_str () , bytesToWrite);
                    } else {
                        bytesToWrite = sizeof (newValue);
                        bytesWritten = __dataFile__.write ((byte *) &newValue , bytesToWrite);
                    }
                    if (bytesWritten != bytesToWrite) { // file IO error, it is highly unlikely that rolling-back to the old value would succeed
                        // log_e ("write failed failed, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock ();  
                        return err_file_io;
                    }
                    // success
                    __dataFile__.flush ();
                    Unlock ();  
                    // log_i ("OK");
                    return err_ok;

                } else { // existing block is not big eneugh, we'll need a new block - more difficult case
                    // log_i ("new block is needed");

                    // 6. search __freeBlocksList__ for most suitable free block, if it exists
                    // log_i ("step 6: searching for the best block on free block list");
                    int freeBlockIndex = -1;
                    uint32_t minWaste = 0xFFFFFFFF;
                    for (int i = 0; i < __freeBlocksList__.size (); i ++) {
                        if (__freeBlocksList__ [i].blockSize >= dataSize && __freeBlocksList__ [i].blockSize - dataSize < minWaste) {
                            freeBlockIndex = i;
                            minWaste = __freeBlocksList__ [i].blockSize - dataSize;
                        }
                    }

                    // 7. reposition __dataFile__ pointer
                    // log_i ("step 7: reposition data file pointer");
                    uint32_t newBlockOffset;          
                    if (freeBlockIndex == -1) { // append data to the end of __dataFile__
                        // log_i ("append data to the end of data file");
                        newBlockOffset = __dataFileSize__;
                    } else { // writte data to free block in __dataFile__
                        // log_i ("found suitabel free data block");
                        newBlockOffset = __freeBlocksList__ [freeBlockIndex].blockOffset;
                        newBlockSize = __freeBlocksList__ [freeBlockIndex].blockSize;
                    }
                    if (!__dataFile__.seek (newBlockOffset, SeekSet)) {
                        // log_e ("seek error err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock (); 
                        return err_file_io;
                    }

                    // 8. construct the block to be written
                    // log_i ("step 8: construct data block");
                    byte *block = (byte *) malloc (newBlockSize);
                    if (!block) {
                        // log_e ("malloc error, out of memory");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        Unlock (); 
                        return err_bad_alloc;
                    }

                    int16_t i = 0;
                    int16_t bs = (int16_t) newBlockSize;
                    memcpy (block + i, &bs, sizeof (bs)); i += sizeof (bs);
                    if (is_same<keyType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                        size_t l = ((String *) &key)->length () + 1; // add 1 for closing 0
                        memcpy (block + i, ((String *) &key)->c_str (), l); i += l;
                    } else { // fixed size key
                        memcpy (block + i, &key, sizeof (key)); i += sizeof (key);
                    }       
                    if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                        size_t l = ((String *) &newValue)->length () + 1; // add 1 for closing 0
                        memcpy (block + i, ((String *) &newValue)->c_str (), l); i += l;
                    } else { // fixed size value
                        memcpy (block + i, &newValue, sizeof (newValue)); i += sizeof (newValue);
                    }

                    // 9. write new block to __dataFile__
                    // log_i ("step 9: write new block to data file");
                    if (__dataFile__.write (block, dataSize) != dataSize) {
                        // log_e ("write failed");
                        free (block);

                        // 10. (try to) roll-back
                        // log_i ("step 10: try to roll-back");
                        if (__dataFile__.seek (newBlockOffset, SeekSet)) {
                            newBlockSize = (int16_t) -newBlockSize;
                            if (__dataFile__.write ((byte *) &newBlockSize, sizeof (newBlockSize)) != sizeof (newBlockSize)) { // can't roll-back
                                __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                            }
                        } else { // can't roll-back 
                            // log_e ("seek failed failed, can't roll-back, critical error, closing data file");
                            __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                        }
                        // log_e ("error err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock (); 
                        return err_file_io;
                    }
                    free (block);
                    __dataFile__.flush ();

                    // 11. roll-out
                    // log_i ("step 11: roll-out");
                    if (freeBlockIndex == -1) { // data appended to the end of __dataFile__
                        __dataFileSize__ += newBlockSize;
                    } else { // data written to free block in __dataFile__
                        __freeBlocksList__.erase (freeBlockIndex); // doesn't fail
                    }
                    // mark old block as free
                    if (!__dataFile__.seek (*pBlockOffset, SeekSet)) {
                        // log_e ("seek error: err_file_io");
                        __dataFile__.close (); // data file is corrupt (it contains two entries with the same key) and it is not likely we can roll it back
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock (); 
                        return err_file_io;
                    }
                    blockSize = (int16_t) -blockSize;
                    if (__dataFile__.write ((byte *) &blockSize, sizeof (blockSize)) != sizeof (blockSize)) {
                        // log_e ("write error: err_file_io");
                        __dataFile__.close (); // data file is corrupt (it contains two entries with the same key) and it si not likely we can roll it back
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock (); 
                        return err_file_io;
                    }
                    __dataFile__.flush ();
                    // update __freeBlocklist__
                    // log_i ("roll-out");
                    if (__freeBlocksList__.push_back ( {*pBlockOffset, (int16_t) -blockSize} )) { // != OK
                        // log_i ("free block list push_back failed, continuing anyway");
                    }
                    // update Map information
                    *pBlockOffset = newBlockOffset; // there is no reason this would fail
                    Unlock ();  
                    // log_i ("OK");
                    return err_ok;
                }

                // Unlock ();  
                // return err_ok;
            }


           /*
            *  Updates the value associated with the key throught callback function (usefull for counting, etc, when all the calculation should be done while locking is in place)
            */

            signed char Update (keyType key, void (*updateCallback) (valueType &value), uint32_t *pBlockOffset = NULL) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock (); 

                valueType value;
                signed char e = FindValue (key, &value); 
                if (e) {
                    // log_e ("FindValue error");
                    __errorFlags__ |= e;
                    Unlock ();  
                    return e;
                }

                updateCallback (value);

                e = Update (key, value, pBlockOffset); 
                if (e) {
                    // log_e ("Update error");
                    __errorFlags__ |= e;
                    Unlock ();  
                    return e;
                }                
                // log_i ("OK");
                Unlock ();
                return err_ok;
            }


           /*
            *  Updates or inserts key-value pair
            */

            signed char Upsert (keyType key, valueType newValue) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                if (is_same<valueType, String>::value)                                                                        // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &newValue) {                                                                              // ... check if parameter construction is valid
                        // log_e ("String value construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock ();
                signed char e;
                e = Insert (key, newValue);
                if (e) // != OK
                    e = Update (key, newValue);
                if (e) { // != OK
                    // log_e ("Update or Insert error");
                    __errorFlags__ |= e;
                } else {
                    // log_i ("OK");
                } 
                Unlock ();
                return e; 
            }

           /*
            *  Updates or inserts the value associated with the key throught callback function (usefull for counting, etc, when all the calculation should be done while locking is in place)
            */

            signed char Upsert (keyType key, void (*updateCallback) (valueType &value), valueType defaultValue) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                if (is_same<valueType, String>::value)                                                                        // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &defaultValue) {                                                                          // ... check if parameter construction is valid
                        // log_e ("String value construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock (); 
                signed char e;
                e = Insert (key, defaultValue);
                if (e) // != OK
                    e = Update (key, updateCallback);
                if (e) { // != OK
                    // log_e ("Update or Insert error");
                    __errorFlags__ |= e;
                } else {
                    // log_i ("OK");
                }
                Unlock ();
                return e; 
            }


           /*
            *  Deletes key-value pair, returns OK or one of the error codes.
            */

            signed char Delete (keyType key) {
                // log_i ("(key, value)");
                if (!__dataFile__) { 
                    // log_e ("error, data file not opened: err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io; 
                }

                if (is_same<keyType, String>::value)                                                                          // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    if (!(String *) &key) {                                                                                   // ... check if parameter construction is valid
                        // log_e ("String key construction error: err_bad_alloc");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_bad_alloc;
                        #endif
                        __errorFlags__ |= err_bad_alloc;
                        return err_bad_alloc;
                    }

                Lock (); 

                if (__inIteration__) {
                    // log_e ("not while iterating, error: err_cant_do_it_now");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_cant_do_it_now;
                    #endif
                    __errorFlags__ |= err_cant_do_it_now;
                    Unlock (); 
                    return err_cant_do_it_now;
                }

                // 1. get blockOffset
                // log_i ("step 1: get block offset");
                uint32_t blockOffset;
                signed char e = FindBlockOffset (key, blockOffset);
                if (e) { // != OK
                    // log_e ("FindBlockOffset failed");
                    Unlock (); 
                    return e;
                }

                // 2. read the block size
                // log_i ("step 2: reading block size from data file");
                if (!__dataFile__.seek (blockOffset, SeekSet)) {
                    // log_e ("seek failed, error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }
                int16_t blockSize;
                if (__dataFile__.read ((uint8_t *) &blockSize, sizeof (int16_t)) != sizeof (blockSize)) {
                    // log_e ("read failed, error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }
                if (blockSize < 0) { 
                    // log_e ("error that shouldn't happen: err_data_changed");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_data_changed;
                    #endif
                    __errorFlags__ |= err_data_changed;
                    Unlock (); 
                    return err_data_changed; // shouldn't happen, but check anyway ...
                }

                // 3. erase the key from Map
                // log_i ("step 3: erase key from Map");
                e = Map<keyType, uint32_t>::erase (key);
                if (e) { // != OK
                    // log_e ("Map::erase failed");
                    __errorFlags__ |= e;
                    Unlock (); 
                    return e;
                }

                // 4. write back negative block size designatin a free block
                // log_i ("step 4: mark bloc as free");
                blockSize = (int16_t) -blockSize;
                if (!__dataFile__.seek (blockOffset, SeekSet)) {
                  // log_e ("seek failed, error err_file_io");

                    // 5. (try to) roll-back
                    // log_i ("step 5: try to roll-back");
                    if (Map<keyType, uint32_t>::insert (key, (uint32_t) blockOffset)) { // != OK
                        // log_e ("Map::insert failed failed, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost the file, this would cause all disk related operations from now on to fail
                    }
                    // log_e ("error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }
                if (__dataFile__.write ((byte *) &blockSize, sizeof (blockSize)) != sizeof (blockSize)) {
                    // log_e ("write failed, try to roll-back");
                     // 5. (try to) roll-back
                    if (Map<keyType, uint32_t>::insert (key, (uint32_t) blockOffset)) { // != OK
                        // log_e ("Map::insert failed failed, can't roll-back, critical error, closing data file");
                        __dataFile__.close (); // memory key value pairs and disk data file are synchronized any more - it is better to clost he file, this would cause all disk related operations from now on to fail
                    }
                    // log_e ("error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    Unlock (); 
                    return err_file_io;
                }
                __dataFile__.flush ();

                // 5. roll-out
                // log_i ("step 5: roll-out");
                // add the block to __freeBlockList__
                blockSize = (int16_t) -blockSize;
                if (__freeBlocksList__.push_back ( {(uint32_t) blockOffset, blockSize} )) { // != OK
                    // log_i ("free block list push_back failed, continuing anyway");
                    // it is not really important to return with an error here, keyValueDatabase can continue working with this error
                }
                // log_i ("OK");
                Unlock ();  
                return err_ok;
            }


           /*
            *  Truncates key-value pairs, returns OK or one of the error codes.
            */

            signed char Truncate () {
                // log_i ("()");
                
                Lock (); 
                    if (__inIteration__) {
                      // log_e ("not while iterating, error: err_cant_do_it_now");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_cant_do_it_now;
                        #endif
                        __errorFlags__ |= err_cant_do_it_now;
                        Unlock (); 
                        return err_cant_do_it_now;
                    }

                    if (__dataFile__) __dataFile__.close (); 

                    __dataFile__ = fileSystem.open (__dataFileName__, "w"); // , true);
                    if (__dataFile__) {
                        __dataFile__.close (); 
                    } else {
                        // log_e ("truncate failed, error err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock ();  
                        return err_file_io;
                    }

                    __dataFile__ = fileSystem.open (__dataFileName__, "r+"); // , false);
                    if (!__dataFile__) {
                        // log_e ("data file open failed, error err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        Unlock ();  
                        return err_file_io;
                    }

                    __dataFileSize__ = 0; 
                    Map<keyType, uint32_t>::clear ();
                    __freeBlocksList__.clear ();
                // log_i ("OK");
                Unlock ();  
                return err_ok;
            }


           /*
            *  The following iterator overloading is needed so that the calling program can iterate with key-blockOffset pair instead of key-value (value holding the blockOffset) pair.
            *  
            *  Unfortunately iterator's * oprator returns a pointer reather than the reference so the calling program should use "->key" instead of ".key" when
            *  refering to key (and blockOffset):
            *
            *      for (auto p: pkvpA) {
            *          // keys are always kept in memory and are obtained fast
            *          Serial.print (p->key); Serial.print (", "); Serial.print (p->blockOffset); Serial.print (" -> "); 
            *          
            *          // values are read from disk, obtaining a value may be much slower
            *          String value;
            *          keyValueDatabase<int, String>::errorCode e = pkvpA.FindValue (p->key, &value, p->blockOffset);
            *          if (e == pkvpA.OK) 
            *              Serial.println (value);
            *          else
            *              Serial.printf ("Error: %s\n", pkvpA.errorCodeText (e));
            *      }
            */

            struct keyBlockOffsetPair {
                keyType key;          // node key
                uint32_t blockOffset; // __dataFile__ offset of block containing a key-value pair
            };        

            class Iterator : public Map<keyType, uint32_t>::Iterator {
                public:
            
                    // called form begin () and first_element () - since only the begin () instance is used for iterating we'll do the locking here ...
                    Iterator (keyValueDatabase* pkvp, int8_t stackSize) : Map<keyType, uint32_t>::Iterator (pkvp, stackSize) {
                        __pkvp__ = pkvp;
                    }

                    // caled form end () andl last_element () 
                    Iterator (int8_t stackSize, keyValueDatabase* pkvp) : Map<keyType, uint32_t>::Iterator (stackSize, pkvp) {
                        __pkvp__ = pkvp;
                    }

                    ~Iterator () {
                        if (__pkvp__) {
                            __pkvp__->__inIteration__ --;
                            __pkvp__->Unlock (); 
                        }
                    }

                    // keyBlockOffsetPair& operator * () const { return (keyBlockOffsetPair&) Map<keyType, uint32_t>::Iterator::operator *(); }
                    keyBlockOffsetPair * operator * () const { return (keyBlockOffsetPair *) Map<keyType, uint32_t>::Iterator::operator *(); }

                    // this will tell if iterator is valid (if there are not elements the iterator can not be valid)
                    operator bool () const { return __pkvp__->size () > 0; }


                private:
          
                    keyValueDatabase* __pkvp__ = NULL;

            };

            Iterator begin () { // since only the begin () instance is neede for iteration we'll do the locking here
                Lock (); // Unlock () will be called in instance destructor
                __inIteration__ ++; // -- will be called in instance destructor
                return Iterator (this, this->height ()); 
            } 

            Iterator end () { 
                return Iterator ((int8_t) 0, (keyValueDatabase *) NULL); 
            } 


           /*
            *  Finds min and max keys in keyValueDatabase.
            *
            *  Example:
            *  
            *    auto firstElement = pkvpA.first_element ();
            *    if (firstElement) // check if first element is found (if pkvpA is not empty)
            *        Serial.printf ("first element (min key) of pkvpA = %i\n", (*firstElement)->key);
            */

          Iterator first_element () { 
              Lock (); // Unlock () will be called in instance destructor
              __inIteration__ ++; // -- will be called in instance destructor
              return Iterator (this, this->height ());  // call the 'begin' constructor
          }

          Iterator last_element () {
              Lock (); // Unlock () will be called in instance destructor
              __inIteration__ ++; // -- will be called in instance destructor
              return Iterator (this->height (), this);  // call the 'end' constructor
          }


           /*
            * Locking mechanism
            */

            void Lock () { 
                #ifdef SEMAPHORE_H // RTOS is running beneath Arduino sketch, multitasking (and semaphores) is supported
                    xSemaphoreTakeRecursive (__semaphore__, portMAX_DELAY); 
                #endif
            } 

            void Unlock () { 
                #ifdef SEMAPHORE_H // RTOS is running beneath Arduino sketch, multitasking (and semaphores) is supported
                    xSemaphoreGiveRecursive (__semaphore__); 
                #endif
            }


        private:

            char __dataFileName__ [255] = "";
            File __dataFile__;
            unsigned long __dataFileSize__ = 0;

            struct freeBlockType {
                uint32_t blockOffset;
                int16_t blockSize;
            };
            vector<freeBlockType> __freeBlocksList__;

            #ifdef SEMAPHORE_H // RTOS is running beneath Arduino sketch, multitasking (and semaphores) is supported
                SemaphoreHandle_t __semaphore__ = xSemaphoreCreateRecursiveMutex (); 
            #endif
            int __inIteration__ = 0;

            // som boards do no thave is_same implemented, so we have to imelement it ourselves: https://stackoverflow.com/questions/15200516/compare-typedef-is-same-type
            template<typename T, typename U> struct is_same { static const bool value = false; };
            template<typename T> struct is_same<T, T> { static const bool value = true; };

            
           /*
            *  Reads the value from __dataFile__.
            *  
            *  Returns success, in case of error it also sets lastErrorCode.
            *
            *  This function does not handle the __semaphore__.
            */

            signed char __readBlock__ (int16_t& blockSize, keyType& key, valueType& value, uint32_t blockOffset, bool skipReadingValue = false) {
                // reposition file pointer to the beginning of a block
                if (!__dataFile__.seek (blockOffset, SeekSet)) {
                    // log_e ("seek error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;
                    return err_file_io;
                }

                // read block size
                if (__dataFile__.read ((uint8_t *) &blockSize, sizeof (int16_t)) != sizeof (blockSize)) {
                    // log_e ("read block size error err_file_io");
                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                        throw err_file_io;
                    #endif
                    __errorFlags__ |= err_file_io;                    
                    return err_file_io;
                }
                // if block is free the reading is already done
                if (blockSize < 0) { 
                    // log_i ("OK");
                    return err_ok;
                }

                // read key
                if (is_same<keyType, String>::value) { // if key is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                    // read the file until 0 is read
                    while (__dataFile__.available ()) { 
                            char c = (char) __dataFile__.read (); 
                            if (!c) break;
                            if (!((String *) &key)->concat (c)) {
                                // log_e ("String key construction error err_bad_alloc");
                                #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                                    throw err_bad_alloc;
                                #endif
                                __errorFlags__ |= err_bad_alloc;
                                return err_bad_alloc;
                            }
                    }
                } else {
                    // fixed size key
                    if (__dataFile__.read ((uint8_t *) &key, sizeof (key)) != sizeof (key)) {
                        // log_e ("read key error err_file_io");
                        #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                            throw err_file_io;
                        #endif
                        __errorFlags__ |= err_file_io;
                        return err_file_io;
                    }                                
                }

                // read value
                if (!skipReadingValue) {
                    if (is_same<valueType, String>::value) { // if value is of type String ... (if anyone knows hot to do this in compile-time a feedback is welcome)
                        // read the file until 0 is read
                        while (__dataFile__.available ()) { 
                                char c = (char) __dataFile__.read (); 
                                if (!c) break;
                                if (!((String *) &value)->concat (c)) {
                                    // log_e ("String value construction error err_bad_alloc");
                                    #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                                        throw err_bad_alloc;
                                    #endif
                                    __errorFlags__ |= err_bad_alloc;
                                    return err_bad_alloc;     
                                }
                        }
                    } else {
                        // fixed size value
                        if (__dataFile__.read ((uint8_t *) &value, sizeof (value)) != sizeof (value)) {
                            // log_e ("read value error err_file_io");
                            #ifdef __USE_KEY_VALUE_DATABASE_EXCEPTIONS__
                                throw err_file_io;
                            #endif
                            __errorFlags__ |= err_file_io;
                            return err_file_io;      
                        }                                
                    }
                }
                // log_i ("OK");
                return err_ok;
            }

    };


    #ifndef __FIRST_LAST_ELEMENT__ 
        #define __FIRST_LAST_ELEMENT__

        template <typename T>
        typename T::Iterator first_element (T& obj) { return obj.first_element (); }

        template <typename T>
        typename T::Iterator last_element (T& obj) { return obj.last_element (); }

    #endif


#endif
