/*
  
  fileSystem.hpp 
  
  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
  May 23, 2023, Bojan Jurca

  FFAT or LittleFS for ESP32 built-in flash disk

    https://github.com/espressif/arduino-esp32/blob/master/libraries/FS/src/FS.cpp
    https://github.com/espressif/arduino-esp32/blob/master/libraries/FFat/src/FFat.h

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <FS.h>
    // fixed size strings
    #include "fsString.h"


#ifndef __FILE_SYSTEM__
    #define __FILE_SYSTEM__

    // TUNNING PARAMETERS

    // choose file system (it must correspond to Tools | Partition scheme setting)
        #define FILE_SYSTEM_FAT      1   
        #define FILE_SYSTEM_LITTLEFS 2
    // one of the above
    #ifndef FILE_SYSTEM
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "FILE_SYSTEM was not defined previously, #defining the default FILE_SYSTEM_FAT in file_system.h"
        #endif
        #define FILE_SYSTEM FILE_SYSTEM_FAT // by default
    #endif

    #if FILE_SYSTEM == FILE_SYSTEM_FAT
        #include <FFat.h>
        #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0)
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling fileSystem.hpp for FAT file system"
        #endif
    #endif
    #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
        #include <LittleFS.h>
        #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0)
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling fileSystem.hpp for LittleFS file system"
        #endif
    #endif

    // ----- CODE -----

    #include "dmesg_functions.h"  

    // log network Traffic information for each socket
    struct diskTrafficInformationType {
        unsigned long bytesRead;
        unsigned long  bytesWritten;
    };
    diskTrafficInformationType diskTrafficInformation = {}; // measure disk Traffic on ESP32 level


    #if FILE_SYSTEM == FILE_SYSTEM_FAT
        #define __fileSystem__ FFat
    #elif FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
        #define __fileSystem__ LittleFS
    #else
        #pragma error "FILE_SYSTEM should be eighter FILE_SYSTEM_FAT or FILE_SYSTEM_LITTLEFS"
    #endif

    class fileSys { 

        public:

            fileSys () {} // constructor

            [[deprecated("Use formatFAT or formatLittleFS instead.")]]  
            inline bool format () __attribute__((always_inline)) { return __fileSystem__.format (); }
            #if FILE_SYSTEM == FILE_SYSTEM_FAT
                inline bool formatFAT () __attribute__((always_inline)) { return __fileSystem__.format (); }
            #endif
            #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
                inline bool formatLittleFs () __attribute__((always_inline)) { return __fileSystem__.format (); }
            #endif

            #if FILE_SYSTEM == FILE_SYSTEM_FAT
                bool mount (bool formatIfUnformatted) { return mountFAT (formatIfUnformatted); }
                bool mountFAT (bool formatIfUnformatted) { 
            #endif
            #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
                bool mount (bool formatIfUnformatted) { return mountLittleFs (formatIfUnformatted); }
                bool mountLittleFs (bool formatIfUnformatted) { 
            #endif

                __mounted__ = false;
                
                if (__fileSystem__.begin (false)) {
                    __mounted__ = true;
                } else {
                    if (formatIfUnformatted) {
                        Serial.printf ("[%10lu] [fileSystem] formatting, please wait ...\n", millis ());
                        if (__fileSystem__.format ()) {
                            Serial.printf ("[%10lu] [fileSystem] ... formatted\n", millis ());
                            dmesg ("[fileSystem] formatted.");
                            if (__fileSystem__.begin (false)) {
                                __mounted__ = true;
                            }
                        } else {
                            Serial.printf ("[%10lu] [fileSystem] ... formatting failed\n", millis ());
                            dmesg ("[fileSystem] formatting failed.");
                        }
                    }
                }

                #if FILE_SYSTEM == FILE_SYSTEM_FAT
                    if (__mounted__) dmesg ("[fileSystem] FAT mounted"); else dmesg ("[fileSystem] failed to mount FAT");
                #endif
                #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
                    if (__mounted__) dmesg ("[fileSystem] LittleFS mounted"); else dmesg ("[fileSystem] failed to mount LittleFS");
                #endif                
                return __mounted__;
            }

            void unmount () { 
                __fileSystem__.end ();
                __mounted__ = false;
            }

            bool mounted () { return __mounted__; } // if file system is mounted or not 

            #if FILE_SYSTEM == FILE_SYSTEM_FAT
                inline size_t totalBytes () __attribute__((always_inline)) { return __fileSystem__.totalBytes (); }
                size_t usedBytes () { return totalBytes () - freeBytes (); }
                inline size_t freeBytes () __attribute__((always_inline)) { return __fileSystem__.freeBytes (); }
            #endif
            #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
                inline size_t totalBytes () __attribute__((always_inline)) { return __fileSystem__.totalBytes (); }
                inline size_t usedBytes () __attribute__((always_inline)) { return __fileSystem__.usedBytes (); }
                size_t freeBytes () { return totalBytes () - usedBytes (); }
            #endif

            inline File open (const char* path) __attribute__((always_inline)) { return __fileSystem__.open (path); }
            // inline File open (string& path) __attribute__((always_inline)) { return __fileSystem__.open (path.c_str ()); }
            inline File open (const char* path, const char* mode, const bool create) __attribute__((always_inline)) { return __fileSystem__.open (path, mode, create); }
            // inline File open (string& path, const char* mode, const bool create) __attribute__((always_inline)) { return __fileSystem__.open (path.c_str (), mode, create); }

            // reads entire configuration file in the buffer - returns success
            // it also removes \r characters, double spaces, comments, ...
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

            bool deleteFile (const char *fileName) {
                if (!__fileSystem__.remove (fileName)) {
                    dmesg ("[fileSystem] unable to delete ", fileName); 
                    return false;
                }
                return true;    
            }
  
            bool makeDirectory (const char *directory) {
                if (!__fileSystem__.mkdir (directory)) {
                    dmesg ("[fileSystem] unable to make ", directory);
                    return false;
                }
                return true;    
            }

            bool removeDirectory (const char *directory) {
                if (!__fileSystem__.rmdir (directory)) {
                    dmesg ("[fileSystem] unable to remove ", directory);
                    return false;
                }
                return true;    
            }

            bool rename (const char* pathFrom, const char* pathTo) {
                if (!__fileSystem__.rename (pathFrom, pathTo)) {
                    dmesg ("[fileSystem] unable to rename ", pathFrom); 
                    return false;
                }
                return true; 
            }

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

            bool isDirectory (const char *fullPath) {
                bool b = false;
                File f = open (fullPath, "r", false);
                if (f) {
                    b = f.isDirectory ();
                    f.close ();
                }     
                return b;
            }
  
            bool isFile (const char *fullPath) {
                bool b = false;
                File f = open (fullPath, "r", false);
                if (f) {
                    b = !f.isDirectory ();
                    f.close ();
                } 
                return b;
            }

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
                    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS  // f.name contains only a file name without a path
                        s += showFullPath ? fileOrDirectory : f.name ();
                    #else                                                 // f.name contains full file path
                        if (showFullPath) {
                            s += showFullPath;
                        } else {
                            string t = f.name ();
                            while ((int i = t.indexOf ("/")) >= 0) t = t.substring (i + 1);
                            if (t == "") t = "/";
                            s += t;    
                        }
                    #endif
                    f.close ();
                }
                return s;
            }

        private:

            bool __mounted__ = false;

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
