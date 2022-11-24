/*
  
  file_system.h 
  
  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
  October, 23, 2022, Bojan Jurca

  FFAT behaves somewhat differently in different IDF versions. I'm not 100 % sure that version 4.4 and above should be treated differently since
  I have not tested all differnt versions. What matters is that file name once returns full path and in the other case just a name within directory.
  Try changing #definitions to what works for you.
    
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <FS.h>
    

#ifndef __FILE_SYSTEM__
  #define __FILE_SYSTEM__

    // TUNNING PARAMETERS

    // choose file system (it must correspond to Tools | Partition scheme setting)
          #define FILE_SYSTEM_FAT      1   
          #define FILE_SYSTEM_LITTLEFS 2
    // one of the above
    #ifndef FILE_SYSTEM
      #define FILE_SYSTEM FILE_SYSTEM_FAT // by default
    #endif

    #if FILE_SYSTEM == FILE_SYSTEM_FAT
      #include <FFat.h>
      #define fileSystem FFat
      #pragma message "Compiling file_system.h for FAT file system"
    #endif
    #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
      #include <LittleFS.h>
      #define fileSystem LittleFS
      #pragma message "Compiling file_system.h for LittleFS file system"
    #endif


    // ----- functions and variables in this modul -----

    #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0)
    bool __fileSystemMounted__ = false;

    bool mountFileSystem (bool);    
    bool readConfigurationFile (char *, size_t, char *);
    bool deleteFile (String);
    bool removeDir (String);
    String fullFilePath (String, String);
    String fullDirectoryPath (String, String);
    bool isDirectory (String);
    bool isFile (String);
    String fileInfo (String, bool);
    String listDirectory (String);
    String recursiveListDirectory (String);

  
    // ----- CODE -----

    #include "dmesg_functions.h"  
    #ifndef __TIME_FUNCTIONS__
      #pragma message "Implicitly including time_functions.h (needed to display file times)"
      #include "time_functions.h"
    #endif

    // mount file system by calling this function
    bool mountFileSystem (bool formatIfUnformatted) { 
      __fileSystemMounted__ = false;
      
      if (fileSystem.begin (false)) {
        __fileSystemMounted__ = true;
      } else {
        if (formatIfUnformatted) {
          Serial.printf ("[%10lu] [file system] formatting, please wait ...\n", millis ());
          if (fileSystem.format ()) {
            dmesg ("[file system] formatted.");
            if (fileSystem.begin (false)) {
              __fileSystemMounted__ = true;
            }
          } else {
            dmesg ("[file system] formatting failed.");
          }
        }
      }
  
      if (__fileSystemMounted__) dmesg ("[file system] mounted."); else dmesg ("[file system] failed to mount.");
      return __fileSystemMounted__;
    }

    // reads entire configuration file in the buffer - returns success
    // it also removes \r characters, double spaces, comments, ...
    bool readConfigurationFile (char *buffer, size_t bufferSize, char *fileName) {
      *buffer = 0;
      int i = 0; // index in the buffer
      bool beginningOfLine = true; // beginning of line
      bool inComment = false; // if reading comment text
      char lastCharacter = 0; // the last character read from the file
  
      File f = fileSystem.open (fileName, FILE_READ);
      if (f) {
        if (!f.isDirectory ()) {
          
          while (f.available ()) { 
            char c = (char) f.read (); 
            switch (c) {
              case '\r': break; // igonore \r
              case '\n': inComment = false; // \n terminates comment
                         if (beginningOfLine) break; // ignore 
                         if (i > 0 && buffer [i - 1] == ' ') i--; // right trim (we can not reach the beginning of the line - see left trim)
                         goto processNormalCharacter;
              case '=':
              case '\t':
              case ' ':  if (inComment) break; // ignore
                         if (beginningOfLine) break; // left trim - ignore
                         if (lastCharacter == ' ') break; // trim in the middle - ignore
                         c = ' ';
                         goto processNormalCharacter;
              case '#':  if (beginningOfLine) inComment = true; // mark comment and ignore
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

    bool deleteFile (String fileName) {
      if (!fileSystem.remove (fileName)) {
        dmesg ("[file system] unable to delete ", (char *) fileName.c_str ());
        return false;
      }
      return true;    
    }
  
    bool makeDir (String directory) {
      if (!fileSystem.mkdir (directory)) {
        dmesg ("[file system] unable to make ", (char *) directory.c_str ());
        return false;
      }
      return true;    
    }

    bool removeDir (String directory) {
      if (!fileSystem.rmdir (directory)) {
        dmesg ("[file system] unable to remove ", (char *) directory.c_str ());
        return false;
      }
      return true;    
    }

    // constructs full path from path and working directory, returns "" if full path is invalid
    String fullFilePath (String path, String workingDirectory) {
      // remove extra /
      if (path.charAt (path.length () - 1) == '/') path = path.substring (0, path.length () - 1); 
      if (path == "") path = "/";
      if (workingDirectory.substring (workingDirectory.length () - 1) != "/") workingDirectory += '/'; // workingDirectory now always ends with /
      
      String s = "";
           if (path.charAt (0) == '/') s = path; // if path begins with / then it is already supposed to be fullPath
      else if (path == ".") s = workingDirectory;
      else if (path.substring (0, 2) == "./") s = workingDirectory + path.substring (2); // if path begins with ./ then fullPath = workingDirectory + the rest of paht
      else s = workingDirectory + path; // else fullPath = workingDirectory + paht

      // resolve (possibly multiple) ..
      int i = 0;
      while (i >= 0) {
        switch ((i = s.indexOf ("/.."))) {
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
  
    // the same as fullFilePath except that it always ends with /
    /*
    String fullDirectoryPath (String path, String workingDirectory) {  
      String s = fullFilePath (path, workingDirectory);
      if (s != "") if (s.charAt (s.length () - 1) != '/') s += '/';
      return s;
    }
    */
  
    // check if full path is a directory
    bool isDirectory (String fullPath) {
      bool b = false;
      
      File f = fileSystem.open (fullPath, FILE_READ);
      if (f) {
        b = f.isDirectory ();
        f.close ();
      }     
      return b;
    }
  
    // check if full path is a file
    bool isFile (String fullPath) {
      bool b = false;
      
      File f = fileSystem.open (fullPath, FILE_READ);
      if (f) {
        b = !f.isDirectory ();
        f.close ();
      } 
      return b;
    }

    // returns file information in Unix like format
    String fileInfo (String fileOrDirectory, bool showFullPath) {
      String s ("");
      File f = fileSystem.open (fileOrDirectory, FILE_READ);
      if (f) { 
        char line [100 + FILE_PATH_MAX_LENGTH];  
        unsigned long fSize = 0;
        struct tm fTime = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        fSize = f.size ();
        time_t lTime = f.getLastWrite (); if (lTime) lTime = timeToLocalTime (lTime);
        fTime = timeToStructTime (lTime);  
        bool d = f.isDirectory ();
        sprintf (line, "%crw-rw-rw-   1 root     root           %6lu ", d ? 'd' : '-', fSize);  
        strftime (line + strlen (line), 25, " %b %d %H:%M      ", &fTime);  
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS  // f.name contains only a file name without a path
          s = String (line) + (showFullPath ? fileOrDirectory : String (f.name ()));      
        #else                                                 // f.name contains full file path
          if (showFullPath) { s = String (line) + String (f.name ()); }
          else { s = String (f.name ()); int i = s.lastIndexOf ('/'); if (i >= 0) s = s.substring (i + 1); s = String (line) + s; }
        #endif
        f.close ();
      }
      return s;
    }
  
    // returns the content of the whole directory in UNIX like format
     String listDirectory (String directoryName) { // directoryName = full path
      String list ("");
      File d = fileSystem.open (directoryName);
      if (d) {
        for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
          #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS  // f.name contains only a file name without a path
            String fp = directoryName; if (fp.charAt (fp.length () - 1) != '/') fp += '/'; fp += f.name (); 
            String s = fileInfo (fp, false);
          #else                                                 // f.name contains full file path
            String s = fileInfo (f.name (), false); 
          #endif
          if (s > "") {
            if (list > "") list += "\r\n";
            list += s;
          }
        }
        d.close ();
      }
      return list;    
    }
  
    // returns the content of whole subdirectory tree
    String recursiveListDirectory (String directoryName) {
      // create 2 lists: fileList and directoryList  
      String dirList = "\n"; // \n separated list od directories
      String fileList = "";  // list of files formatted for output
  
      // 1. display directory info
      
      String s = fileInfo (directoryName, true);
      if (s > "") if (fileList > "") fileList += "\r\n"; fileList += s; 
  
      // 2. display information about files and remember subdirectories
      File d = fileSystem.open (directoryName);
      if (!d) {
        return "";
      } else {
        for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
          if (f.isDirectory ()) {
            // save directory name for later recursion
            #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS  // f.name contains only a file name without a path
              String fp = directoryName; if (fp.charAt (fp.length () - 1) != '/') fp += '/'; fp += f.name ();
              dirList += fp; dirList += '\n';
            #else                                                 // f.name contains full file path
              dirList += f.name (); dirList += '\n';
            #endif
          } else {
            #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS  // f.name contains only a file name without a path
              String fp = directoryName; if (fp.charAt (fp.length () - 1) != '/') fp += '/'; fp += f.name ();
              String s = fileInfo (fp, true);
            #else                                                 // f.name contains full file path
              String s = fileInfo (f.name (), true);
            #endif
            if (s > "") if (fileList > "") fileList += "\r\n"; fileList += s;
          }
        }     
        d.close ();
        // DEBUG: Serial.println ("List of files: {" + fileList + "}"); Serial.println ("List of directories: {" + dirList + "}");
        delay (1); // reset the watchdog

        // 3. recurse subdirectories
        int i = 1;
        while (true) { // scan directories in dirList
          int j = dirList.indexOf ('\n', i + 1);
          if (j > i) {
            String s = recursiveListDirectory (dirList.substring (i, j));
            if (s > "") {
              if (fileList > "") fileList += "\r\n";
              fileList += s;
            }
            i = j + 1;
          } else {
            break;
          }
        }
      }
      
      return fileList;    
    }
  
#endif
