/*
  
    fileSystem.hpp 
    
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
    
    August 12, 2023, Bojan Jurca

    FFAT or LittleFS for ESP32 built-in flash disk

        https://github.com/espressif/arduino-esp32/blob/master/libraries/FS/src/FS.cpp
        https://github.com/espressif/arduino-esp32/blob/master/libraries/FFat/src/FFat.h

    SD card

        https://github.com/espressif/arduino-esp32/tree/master/libraries/SD

        With the following exception: VDD is connected to 5V instead of 3V3, it looks like 3V3 is not enough to power the SD card

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <FS.h>
    // fixed size strings
    #include "fsString.h"


#ifndef __FILE_SYSTEM__
    #define __FILE_SYSTEM__

    // TUNNING PARAMETERS

    #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0) 

    // choose file system (it must correspond to Tools | Partition scheme setting)
        #define FILE_SYSTEM_FAT      1   
        #define FILE_SYSTEM_LITTLEFS 2
        #define FILE_SYSTEM_SD_CARD  4
    // one of the above
    #ifndef FILE_SYSTEM
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "FILE_SYSTEM was not defined previously, #defining the default FILE_SYSTEM_FAT in file_system.h"
        #endif
        #define FILE_SYSTEM FILE_SYSTEM_FAT // by default
    #endif

    #if (FILE_SYSTEM & (FILE_SYSTEM_FAT | FILE_SYSTEM_LITTLEFS)) == (FILE_SYSTEM_FAT | FILE_SYSTEM_LITTLEFS)
        #pragma error "FILE_SYSTEM can't be both, FILE_SYSTEM_FAT and FILE_SYSTEM_LITTLEFS, at the same time"
    #endif

    #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
        #include <FFat.h>
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling fileSystem.hpp for FAT file system"
        #endif
    #endif
    #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
        #include <LittleFS.h>
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling fileSystem.hpp for LittleFS file system"
        #endif
    #endif
    #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
        #include <SD.h>
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling fileSystem.hpp for SD card"
        #endif
    #endif


    // ----- CODE -----


    // log network Traffic information for each socket
    struct diskTrafficInformationType {
        unsigned long bytesRead;
        unsigned long bytesWritten;
    };
    diskTrafficInformationType diskTrafficInformation = {}; // measure disk Traffic on ESP32 level


    #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
        #define __fileSystem__ FFat
    #elif (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
        #define __fileSystem__ LittleFS
    #else
        #pragma error "FILE_SYSTEM should be eighter FILE_SYSTEM_FAT or FILE_SYSTEM_LITTLEFS"
    #endif
    #if FILE_SYSTEM == FILE_SYSTEM_SD_CARD // just that the code compiles in case SD card is the only file system
        #define __fileSystem__ SD
    #endif

    class fileSys { 

        public:

            fileSys () {} // constructor

            // format (only the built-in flash disk can be formated with LittleFs or FAT, SD card can not be formatted)

            #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT || (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
                [[deprecated("Use formatFAT or formatLittleFS instead.")]]  
                inline bool format () __attribute__((always_inline)) { return __fileSystem__.format (); }
                #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
                    inline bool formatFAT () __attribute__((always_inline)) { return __fileSystem__.format (); }
                #endif
                #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
                    inline bool formatLittleFs () __attribute__((always_inline)) { return __fileSystem__.format (); }
                #endif
            #endif

            // mounts built-in flash disk

            #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
                bool mount (bool formatIfUnformatted) { return mountFAT (formatIfUnformatted); }
                bool mountFAT (bool formatIfUnformatted) { 
                    if (__builtinFlashDiskMounted__) return false; // already mounted

                    __builtinFlashDiskMounted__ = false;
                    
                    if (__fileSystem__.begin (false)) {
                        __builtinFlashDiskMounted__ = true;
                    } else {
                        if (formatIfUnformatted) {
                            Serial.printf ("[%10lu] [fileSystem] formatting, please wait ...\n", millis ());
                            if (__fileSystem__.format ()) {
                                Serial.printf ("[%10lu] [fileSystem] ... formatted\n", millis ());
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem] formatted.");
                                #endif
                                if (__fileSystem__.begin (false)) {
                                    __builtinFlashDiskMounted__ = true;
                                }
                            } else {
                                Serial.printf ("[%10lu] [fileSystem] ... formatting failed\n", millis ());
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem] formatting failed.");
                                #endif
                            }
                        }
                    }

                    if (__builtinFlashDiskMounted__) {
                        Serial.printf ("[%10lu] [fileSystem] FAT mounted\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] FAT mounted"); 
                        #endif
                    } else { 
                        Serial.printf ("[%10lu] [fileSystem] failed to mount FAT\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] failed to mount FAT");
                        #endif
                    }
                    return __builtinFlashDiskMounted__;
                }

            #endif
            #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
                bool mount (bool formatIfUnformatted) { return mountLittleFs (formatIfUnformatted); }
                bool mountLittleFs (bool formatIfUnformatted) { 
                    if (__builtinFlashDiskMounted__) return false; // already mounted

                    __builtinFlashDiskMounted__ = false;
                    
                    if (__fileSystem__.begin (false)) {
                        __builtinFlashDiskMounted__ = true;
                    } else {
                        if (formatIfUnformatted) {
                            Serial.printf ("[%10lu] [fileSystem] formatting, please wait ...\n", millis ());
                            if (__fileSystem__.format ()) {
                                Serial.printf ("[%10lu] [fileSystem] ... formatted\n", millis ());
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem] formatted.");
                                #endif
                                if (__fileSystem__.begin (false)) {
                                    __builtinFlashDiskMounted__ = true;
                                }
                            } else {
                                Serial.printf ("[%10lu] [fileSystem] ... formatting failed\n", millis ());
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem] formatting failed.");
                                #endif
                            }
                        }
                    }

                    if (__builtinFlashDiskMounted__) {
                        Serial.printf ("[%10lu] [fileSystem] LittleFS mounted\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] LittleFS mounted"); 
                        #endif
                    } else {
                        Serial.printf ("[%10lu] [fileSystem] failed to mount LittleFS\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] failed to mount LittleFS");
                        #endif
                    }
                    return __builtinFlashDiskMounted__;
                }

            #endif

            // mounts SD card, provide pin connected to CS and a mount point for SD card to the fileSystem

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                bool mountSD (const char *mountPoint = "/SD", uint8_t pinCS = 5) {
                    if (__SDcardMounted__) return false; // already mounted

                    __SDcardMounted__ = false;

                    if (!mountPoint || *mountPoint != '/' || strlen (mountPoint) >= sizeof (__SDcardMountPoint__) - 1) {
                        Serial.printf ("[%10lu] [fileSystem] invalid SD card mount point: %s\n", millis (), mountPoint);
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] invalid SD card mount point: ", mountPoint); 
                        #endif
                        return false;
                    }

                    strcpy (__SDcardMountPoint__, mountPoint); // we've made the length checking before
                    if (__SDcardMountPoint__ [strlen (__SDcardMountPoint__) - 1] != '/') strcat (__SDcardMountPoint__, "/"); // __SDcardMountPoint__ now always ends with /


                    // if builtin flash disk file system is already mounted then create mountPoint directory on builtin flash disk which would make operations like list directory much easier 
                    // note that SD card is not mounted yet at this point so mountPOint will not get redirected to SD
                    if (__builtinFlashDiskMounted__) {
                        if (!isDirectory (mountPoint)) {
                            // create directory tree
                            for (int i = 1; i < strlen (__SDcardMountPoint__); i++) {
                                if (__SDcardMountPoint__ [i] == '/') {
                                    __SDcardMountPoint__ [i] = 0;
                                    makeDirectory (__SDcardMountPoint__);
                                    __SDcardMountPoint__ [i] = '/';
                                }
                            }
                            // check success
                            if (!isDirectory (mountPoint)) {
                                Serial.printf ("[%10lu] [fileSystem] can't make the SD mount point directory: %s\n", millis (), mountPoint);
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem] can't make the SD mount point directory: ", mountPoint); 
                                #endif
                                return false;
                            }
                        }  
                    }
                     
                    if (!SD.begin (pinCS)) {
                        Serial.printf ("[%10lu] [fileSystem] failed to mount SD card\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] failed to mount SD card"); 
                        #endif
                        return false;
                    }

                    if (SD.cardType () == CARD_NONE) {
                        Serial.printf ("[%10lu] [fileSystem] no SD card attached\n", millis ());
                        #ifdef __DMESG__
                            dmesg ("[fileSystem] no SD card attached"); 
                        #endif
                        return false;
                    }

                    __SDcardMounted__ = true;

                    Serial.printf ("[%10lu] [fileSystem] SD card mounted to %s\n", millis (), mountPoint);
                    #ifdef __DMESG__
                        dmesg ("[fileSystem] SD card mounted to ", mountPoint); 
                    #endif

                    return true;
                } 
            #endif

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                inline char *SDcardMountPoint () __attribute__((always_inline)) { return __SDcardMountPoint__; }
            #endif            

            // unmounts file system

            #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT || (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
                void unmount () {
                    __builtinFlashDiskMounted__ = false;
                    __fileSystem__.end ();
                }
            #endif

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                void unmountSD () {
                    __SDcardMounted__ = false;
                    SD.end ();
                }
            #endif     

            // is a file system mounted?       

            bool mounted () { return __builtinFlashDiskMounted__ || __SDcardMounted__; } // if file system is mounted or not 

            // tells wether the path points to builtin flash disk or to SD card

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                bool pointsToSDcard (const char *fullPath) {
                    if (!__SDcardMounted__) return false;
                    if (strstr (fullPath, __SDcardMountPoint__) == fullPath) return true; // fullPath points to SD card like mountPoint = /SD/ and fullPath = /SD/a.txt
                    if (strstr (__SDcardMountPoint__, fullPath) == __SDcardMountPoint__ && strlen (fullPath) == strlen (__SDcardMountPoint__) - 1) return true; // fullPath points to SD card like mountPoint = /SD/ and fullPath = /SD
                    return false;
                }
            #endif     

            // calculates path to SD card from full path ant SD card mount point

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                const char *SDcardPath (const char *fullPath) {
                    return strlen (fullPath) < strlen (__SDcardMountPoint__) ? "/" : fullPath + strlen (__SDcardMountPoint__) - 1;
                }
            #endif                 

            // opens the file

            File open (const char* path) { 
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (path)) return SD.open (SDcardPath (path));  
                #endif

                // directory points to builtin flash disk
                return __fileSystem__.open (path); 
            }

            // opens the file

            File open (const char* path, const char* mode, const bool create) { 
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (path)) return SD.open (SDcardPath (path), mode, create);  
                #endif

                // directory points to builtin flash disk
                return __fileSystem__.open (path, mode, create); 
            }

            // reads entire configuration file in the buffer - returns success, it also removes \r characters, double spaces, comments, ...

            bool readConfigurationFile (char *buffer, size_t bufferSize, const char *fileName) {
                *buffer = 0;
                int i = 0; // index in the buffer
                bool beginningOfLine = true;  // beginning of line
                bool inComment = false;       // if reading comment text
                char lastCharacter = 0;       // the last character read from the file
            
                File f = open (fileName, "r", false);
                if (f) {
                    if (!f.isDirectory ()) {
                      
                      while (f.available ()) { 
                          char c = (char) f.read (); 
                          switch (c) {
                              case '\r':  break; // igonore \r
                              case '\n':  inComment = false; // \n terminates comment
                                          if (beginningOfLine) break; // ignore 
                                          if (i > 0 && buffer [i - 1] == ' ') i--; // right trim (we can not reach the beginning of the line - see left trim)
                                          goto processNormalCharacter;
                              case '=':
                              case '\t':
                              case ' ':   if (inComment) break; // ignore
                                          if (beginningOfLine) break; // left trim - ignore
                                          if (lastCharacter == ' ') break; // trim in the middle - ignore
                                          c = ' ';
                                          goto processNormalCharacter;
                              case '#':   if (beginningOfLine) inComment = true; // mark comment and ignore
                                          goto processNormalCharacter;
                              default:   
            processNormalCharacter:
                                          if (inComment) break; // ignore
                                          if (i > bufferSize - 2) { f.close (); return false; } // buffer too small
                                          buffer [i++] = lastCharacter = c; // copy space to the buffer                       
                                          beginningOfLine = (c == '\n');
                                          break;
                            }
                        }
                        buffer [i] = 0; 
                        f.close ();
                        return true;
                    }
                    f.close ();
                }             
                return false; // can't open the file or it is a directory
            }

            // deletes file

            bool deleteFile (const char *fileName) {
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (fileName)) {
                        if (!SD.remove (SDcardPath (fileName))) {
                            #ifdef __DMESG__
                                dmesg ("[fileSystem][SD] unable to delete ", fileName); 
                            #endif
                            return false;
                        }
                        return true;
                    }
                #endif

                // builtin flash disk
                if (!__fileSystem__.remove (fileName)) {
                    #ifdef __DMESG__
                        dmesg ("[fileSystem] unable to delete ", fileName); 
                    #endif
                    return false;
                }
                return true;    
            }

            // makes directory

            bool makeDirectory (const char *directory) {
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (directory)) {
                        if (!SD.mkdir (SDcardPath (directory))) {
                            #ifdef __DMESG__
                                dmesg ("[fileSystem][SD] unable to make ", directory); 
                            #endif
                            return false;
                        }
                        return true;
                    }
                #endif

                // builtin flash disk
                if (!__fileSystem__.mkdir (directory)) {
                    #ifdef __DMESG__
                        dmesg ("[fileSystem] unable to make ", directory);
                    #endif
                    return false;
                }
                return true;    
            }

            // removes directory

            bool removeDirectory (const char *directory) {
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (directory)) {
                        // is it a SD card mount point?
                        if (strlen (directory) == strlen (__SDcardMountPoint__) - 1) {
                            #ifdef __DMESG__
                                dmesg ("[fileSystem][SD] can not remove SC card mount point ", directory); 
                            #endif
                            return false;
                        }

                        if (!SD.rmdir (SDcardPath (directory))) {
                            #ifdef __DMESG__
                                dmesg ("[fileSystem][SD] unable to remove ", directory); 
                            #endif
                            return false;
                        }
                        return true;
                    }
                #endif

                // builtin flash disk
                if (!__fileSystem__.rmdir (directory)) {
                    #ifdef __DMESG__
                        dmesg ("[fileSystem] unable to remove ", directory);
                    #endif
                    return false;
                }
                return true;    
            }

            // renames file or directory

            bool rename (const char* pathFrom, const char* pathTo) {
                // builtin flash disk or SD card?
                #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                    if (pointsToSDcard (pathFrom)) {
                        if (pointsToSDcard (pathTo)) {
                            if (!SD.rename (SDcardPath (pathFrom), SDcardPath (pathTo))) {
                                #ifdef __DMESG__
                                    dmesg ("[fileSystem][SD] unable to rename ", pathFrom);
                                #endif
                                return false;
                            }
                            return true;
                        } else {
                            #ifdef __DMESG__
                                dmesg ("[fileSystem][SD] can't mix builtin flash disk and SD card with rename");
                            #endif
                            return false;
                        }
                    } 
                #endif
                
                // both paths point to builtin flash disk
                if (!__fileSystem__.rename (pathFrom, pathTo)) {
                    #ifdef __DMESG__
                        dmesg ("[fileSystem] unable to rename ", pathFrom); 
                    #endif
                    return false;
                }
                return true; 
            }

            // makes the full path out of relative path and working directory, for example if working directory is /usr and relative patj is a.txt then full path is /usr/a.txt

            string makeFullPath (const char *relativePath, const char *workingDirectory) { 
                char *p = relativePath [0] ? (char *) relativePath : (char *) "/"; // relativePath should never be empty

                string s;
                if (p [0] == '/') { // if path begins with / then it is already supposed to be fullPath
                    s = p; 
                } else if (!strcmp (p, ".")) { // . means the working directory
                    s = workingDirectory;
                } else if (p [0] == '.' && p [1] == '/') { // if path begins with ./ then fullPath = workingDirectory + the rest of paht
                    s = string (workingDirectory);
                    if (s [s.length () - 1] != '/') s += '/'; 
                    s += (p + 2); 
                } else { // else fullPath = workingDirectory + path
                    s = string (workingDirectory);
                    if (s [s.length () - 1] != '/') s += '/'; 
                    s += p; 
                }

                if (s [1] && s [s.length () - 1] == '/') s [s.length () - 1] = 0; // remove trailing '/' if exists

                // resolve (possibly multiple) ..
                int i = 0;
                while (i >= 0) {
                    switch ((i = s.indexOf ((char *) "/.."))) {
                        case -1:  return s;  // no .. found, nothing to resolve any more
                        case  0:  return ""; // s begins with /.. - error in path
                        default:             // restructure path
                                  int j = s.substring (0, i - 1).lastIndexOf ('/');
                                  if (j < 0) return ""; // error in path - should't happen
                                  s = s.substring (0, j) + s.substring (i + 3);
                                  if (s == "") s = "/";
                    }
                }

                return ""; // never executes
            }

            // is full path a directory?

            bool isDirectory (const char *fullPath) {
                bool b = false;
                File f = open (fullPath, "r", false);
                if (f) {
                    b = f.isDirectory ();
                    f.close ();
                }     
                return b;
            }

            // is full path a file?
  
            bool isFile (const char *fullPath) {
                bool b = false;
                File f = open (fullPath, "r", false);
                if (f) {
                    b = !f.isDirectory ();
                    f.close ();
                } 
                return b;
            }

            // returns information about file or directory in UNIX like text line

            string fileInformation (const char *fileOrDirectory, bool showFullPath = false) { // returns UNIX like text with file information
                string s;
                File f = open (fileOrDirectory, "r", false);
                if (f) { 
                    unsigned long fSize = 0;
                    struct tm fTime = {};
                    time_t lTime = f.getLastWrite ();
                    localtime_r (&lTime, &fTime);
                    sprintf (s.c_str (), "%crw-rw-rw-   1 root     root          %7lu ", f.isDirectory () ? 'd' : '-', f.size ());  // string = fsString<350> so we have enough space
                    strftime (s.c_str () + strlen (s.c_str ()), 25, " %b %d %H:%M      ", &fTime);  
                    if (showFullPath || !strcmp (fileOrDirectory, "/")) {
                        s += fileOrDirectory;
                    } else {
                        int lastSlash = 0;
                        for (int i = 1; fileOrDirectory [i]; i++) if (fileOrDirectory [i] == '/') lastSlash = i;
                        s += fileOrDirectory + lastSlash + 1;
                    }
                    f.close ();
                }
                return s;
            }

        private:

            bool __builtinFlashDiskMounted__ = false;
            bool __SDcardMounted__ = false;

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                char __SDcardMountPoint__ [FILE_PATH_MAX_LENGTH] = "";
            #endif

    };


    // create a working instance before including time_functions.h, time_functions.h will use the fileSystem instance
    fileSys fileSystem;

    // strBetween function, usefull for parsing configuration files content

    char *strBetween (char *buffer, size_t bufferSize, char *src, const char *left, const char *right) { // copies substring of src between left and right to buffer or "" if not found or buffer too small, return bufffer
      *buffer = 0;
      char *l, *r;

      if (*left) l = strstr (src, left);
      else l = src;
      
      if (l) {  
        l += strlen (left);

        if (*right) r = strstr (l, right);
        else r = l + strlen (l);
        
        if (r) { 
          int len = r - l;
          if (len < bufferSize - 1) { 
            strncpy (buffer, l, len); 
            buffer [len] = 0; 
          }
        }
      }    
      return buffer;                                                     
    }

#endif
