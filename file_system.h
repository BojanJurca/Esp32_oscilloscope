/*
 * 
 * file_system.h 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 * History:
 *          - first release, 
 *            November 15, 2018, Bojan Jurca
 *          - added fileSystemSemaphore and delay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca
 *          - added functions __readFileWithoutSemaphore__ and __writeFileWithoutSemaphore__
 *            September 8, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca
 *          - port from SPIFFS to FAT file system, adjustment for Arduino 1.8.13
 *            October 10, 2020, Bojan Jurca
 *  
 */


#ifndef __FILE_SYSTEM__
  #define __FILE_SYSTEM__

  // ----- includes, definitions and supporting functions -----
  
  #include <WiFi.h>
  #include "common_functions.h"   // between
  #include "FFat.h"  
  #include "TcpServer.hpp"        // we'll need FS semaphore defined in TcpServer.hpp 
  // #include "time_functions.h"  
    time_t timeToLocalTime (time_t t);
    struct tm timeToStructTime (time_t t);

  void __fileSystemDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__  // use dmesg from telnet server if possible
      dmesg (message);
    #else                     // if not just output to Serial
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
    #endif
  }
  void (* fileSystemDmesg) (String) = __fileSystemDmesg__; // use this pointer to display / record system messages

  // SemaphoreHandle_t fileSystemSemaphore = xSemaphoreCreateMutex ();

  #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0)

  bool __fileSystemMounted__ = false;

  /*
  bool mountFileSystem (bool formatIfUnformatted) {                                           // mount file system by calling this function
    fileSystemDmesg ("[file system] mounting ...");
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      __fileSystemMounted__ = FFat.begin (formatIfUnformatted);
    // xSemaphoreGive (fileSystemSemaphore);
      fileSystemDmesg (__fileSystemMounted__ ? "[file system] mounted." : "[file system] failed to mount.");
      return __fileSystemMounted__;
  }
  */

  bool mountFileSystem (bool formatIfUnformatted) { // mount file system by calling this function
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      if (FFat.begin (false)) {
        fileSystemDmesg ("[file system] mounted.");
        __fileSystemMounted__ = true;
      } else {
        if (formatIfUnformatted) {
          Serial.printf ("[%10lu] [file system] formatting, please wait ...\n", millis ());
          if (FFat.format ()) {
            fileSystemDmesg ("[file system] formatted.");
            if (FFat.begin (false)) {
              fileSystemDmesg ("[file system] mounted.");
              __fileSystemMounted__ = true;
            } else {
              fileSystemDmesg ("[file system] failed to mount.");
            }
          } else {
            fileSystemDmesg ("[file system] formatting failed.");
          }
        } else {
          fileSystemDmesg ("[file system] failed to mount.");
        }
      }
    // xSemaphoreGive (fileSystemSemaphore);
    return __fileSystemMounted__;
  }

  bool deleteFile (String fileName) {
    if (!__fileSystemMounted__) { fileSystemDmesg ("[file system] requested to delete a file but file system is not mounted." ); return false; }

    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      if (!FFat.remove (fileName)) {
        // xSemaphoreGive (fileSystemSemaphore);
        fileSystemDmesg ("[file system] unable to delete " + fileName);
        return false;
      }
    // xSemaphoreGive (fileSystemSemaphore);
    return true;    
  }

  bool makeDir (String directory) {
    if (!__fileSystemMounted__) { fileSystemDmesg ("[file system] requested to make a directory but file system is not mounted." ); return false; }
    if (directory.substring (directory.length () - 1) == "/") directory = directory.substring (0, directory.length () - 1); if (directory == "") directory = "/"; // except for /, path never ends with / - otherwise mkdir would fail

    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      if (!FFat.mkdir (directory)) {
        // xSemaphoreGive (fileSystemSemaphore);
        fileSystemDmesg ("[file system] unable to make " + directory);
        return false;
      }
    // xSemaphoreGive (fileSystemSemaphore);
    return true;    
  }

  bool removeDir (String directory) {
    if (!__fileSystemMounted__) { fileSystemDmesg ("[file system] requested to remove a directory but file system is not mounted." ); return false; }
    if (directory.substring (directory.length () - 1) == "/") directory = directory.substring (0, directory.length () - 1); if (directory == "") directory = "/"; // except for /, path never ends with / - otherwise rmdir would fail

    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      if (!FFat.rmdir (directory)) {
        // xSemaphoreGive (fileSystemSemaphore);
        fileSystemDmesg ("[file system] unable to remove " + directory);
        return false;
      }
    // xSemaphoreGive (fileSystemSemaphore);
    return true;    
  }

  // reads entire file into String without using sempahore - it is expected that calling functions would provide it - returns success
  bool __readFileWithoutSemaphore__ (String& fileContent, String fileName, bool ignoreCR = false) {
    fileContent = "";
    
    File f = FFat.open (fileName, FILE_READ);
    if (f) {
      if (!f.isDirectory ()) {
        while (f.available ()) { char c = (char) f.read (); if (c != '\r' || !ignoreCR) fileContent += c; }
        f.close ();
        return true;
      } else { 
        fileSystemDmesg ("[file_system] " + String (fileName) + " is a directory.");
      }
      f.close ();
    }             
    return false;
  }  

  // writes String into file file without using sempahore - it is expected that calling functions would provide it - returns success
  bool __writeFileWithoutSemaphore__ (String& fileContent, String fileName) {
    File f = FFat.open (fileName, FILE_WRITE);
    if (f) {
      if (!f.isDirectory ()) {
        if (f.printf (fileContent.c_str ()) == strlen (fileContent.c_str ())) {
          f.close ();
          return true;        
        } else {
          fileSystemDmesg ("[file_system] can't write " + String (fileName));
        }
      }
      f.close ();
    }    
    return false;
  }  

  // reads entire file into String, returns success
  bool readFile (String& fileContent, String fileName, bool ignoreCR = false) {
    fileContent = "";
    bool b;
    if (!__fileSystemMounted__) { fileSystemDmesg ("[file system] requested to read a file but file system is not mounted."); return false; }
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      b = __readFileWithoutSemaphore__ (fileContent, fileName, ignoreCR);
    // xSemaphoreGive (fileSystemSemaphore);             
    return b;      
  }

  // writes String into file file, returns success
  bool writeFile (String& fileContent, String fileName) {
    if (!__fileSystemMounted__) { fileSystemDmesg ("[file system] requested to write a file but file system is not mounted."); return false; }
    bool b; 
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
      b = __writeFileWithoutSemaphore__ (fileContent, fileName);
    // xSemaphoreGive (fileSystemSemaphore);
    return b;  
  }  
  
  String readTextFile (String fileName) { // reads entire file, ignoring \r
    String fileContent;
    readFile (fileContent, fileName, true);
    return fileContent;
  }

  // constructs full path from path and working directory, returns "" if full path is invalid
  String fullFilePath (String path, String workingDirectory) {
    if (path.substring (path.length () - 1) == "/") path = path.substring (0, path.length () - 1); if (path == "") path = "/"; // except for /, path never ends with /
    if (workingDirectory.substring (workingDirectory.length () - 1) != "/") workingDirectory += '/'; // workingDirectory always ends with /
    
    String s = "";
  
         if (path.charAt (0) == '/') s = path; // if path begins with / then it is already supposed to be fullPath
    else if (path == ".") s = workingDirectory;
    else if (path.substring (0, 2) == "./") s = workingDirectory + path.substring (2); // if path begins with ./ then fullPath = workingDirectory + the rest of paht
    else s = workingDirectory + path; // else fullPath = workingDirectory + paht

    // resolve ..
    int i = 0;
    while (i >= 0) {
      switch ((i = s.indexOf ("/.."))) {
        case -1:  break;     // no .. found, proceed
        case  0:  return ""; // we've got /.. - error in path
        default:             // restructure path
                  int j = s.substring (0, i - 1).lastIndexOf ('/');
                  if (j < 0) return ""; // error in path
                  s = s.substring (0, j + 1) + s.substring (i + 4);
      }
    }

    return s;
  }

  // te same as fullFilePath except that it always ends with /
  String fullDirectoryPath (String path, String workingDirectory) {  
    String s = fullFilePath (path, workingDirectory);
    if (s != "") if (s.substring (s.length () - 1) != "/") s += '/';
    return s;
  }

  // checks if user, whose home directory is known, has a right to access full path
  bool userMayAccess (String fullPath, String homeDir) {
    return fullPath.substring (0, homeDir.length ()) == homeDir;
  }

  // check if full path is a directory
  bool isDirectory (String fullPath) {
    if (fullPath.substring (fullPath.length () - 1) == "/") fullPath = fullPath.substring (0, fullPath.length () - 1); if (fullPath == "") fullPath = "/"; // except for /, fullPath now neverends with /
    
    bool b = false;
    
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
    
      File f = FFat.open (fullPath, FILE_READ);
      if (f) {
        b = f.isDirectory ();
        f.close ();
      } 
    
    // xSemaphoreGive (fileSystemSemaphore);    
    return b;
  }

  // check if full path is a file
  bool isFile (String fullPath) {
    bool b = false;
    
    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
    
      File f = FFat.open (fullPath, FILE_READ);
      if (f) {
        b = !f.isDirectory ();
        f.close ();
      } 
    
    // xSemaphoreGive (fileSystemSemaphore);    
    return b;
  }

  String __fileInfoWithoutSemaphore__ (String fileOrDirectory, bool showFullPath) {
    String s = "";

    File f = FFat.open (fileOrDirectory, FILE_READ);
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
      String n = f.name (); if (!showFullPath) { int i = n.lastIndexOf ('/'); if (i >= 0) n = n.substring (i + 1); }
      if (d && strcmp (f.name (), "/")) n += "/";
      s = String (line) + n;
      f.close ();
    }
      
    return s;
  }

  String listDirectory (String directory) {
    if (!__fileSystemMounted__) return "File system not mounted. You may have to use mkfs.fat to format flash disk first.";

    if (directory.substring (directory.length () - 1) == "/") directory = directory.substring (0, directory.length () - 1); if (directory == "") directory = "/"; // except for /, directory now neverends with /

    String list = "";

    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);
    
      File d = FFat.open (directory);
      if (d) {
        for (File f = d.openNextFile (); f; f = d.openNextFile ()) {

          String s = __fileInfoWithoutSemaphore__ (f.name (), false);
          if (s > "") {
            if (list > "") list += "\r\n";
            list += s;
          }
          yield ();

          /*
          char line [100 + FILE_PATH_MAX_LENGTH];  
          unsigned long fSize = 0;
          struct tm fTime = {0, 0, 0, 0, 0, 0, 0, 0, 0};
          fSize = f.size ();
          time_t lTime = f.getLastWrite (); if (lTime) lTime = timeToLocalTime (lTime);
          fTime = timeToStructTime (&lTime);  
          if (f.isDirectory ()) sprintf (line, "drw-rw-rw-   1 root     root           %6lu ", fSize);  
          else                  sprintf (line, "-rw-rw-rw-   1 root     root           %6lu ", fSize);  
          strftime (line + strlen (line), 25, " %b %d %H:%M      ", &fTime);  
          String s = f.name (); int i = s.lastIndexOf ('/'); if (i >= 0) s = s.substring (i + 1);
          strcat (line, s.c_str ());  
          if (list > "") list += "\r\n";
          list += String (line);
          f.close ();
          */
          
        } // for     
        d.close ();
      }
    
    // xSemaphoreGive (fileSystemSemaphore);
    return list;    
  }
  
  String recursiveListDirectory (String directory) {
    if (!__fileSystemMounted__) return "File system not mounted. You may have to use mkfs.fat to format flash disk first.";

    if (directory.substring (directory.length () - 1) == "/") directory = directory.substring (0, directory.length () - 1); if (directory == "") directory = "/"; // except for /, directory now neverends with /

    // create 2 lists: fileList and directoryList  
    String dirList = "\n"; // \n separated list od directories
    String fileList = "";  // list of files formatted for output

    // xSemaphoreTake (fileSystemSemaphore, portMAX_DELAY);

      // 1. display directory info
      String s = __fileInfoWithoutSemaphore__ (directory, true);
      if (s > "") {
        if (fileList > "") fileList += "\r\n";
        fileList += s; 
      }

      // 2. display information about files and remember subdirectories
      File d = FFat.open (directory);
      if (!d) {
        // xSemaphoreGive (fileSystemSemaphore);
        return "";
      } else {
        for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
          if (f.isDirectory ()) {

            // save directory name for later recursion
            dirList += String (f.name ()) + "\n";

          } else {

            String s = __fileInfoWithoutSemaphore__ (f.name (), true);
            if (s > "") {
              if (fileList > "") fileList += "\r\n";
              fileList += s;
            }
            yield ();
            
          }
        }     
        d.close ();
        // Serial.println ("List of directories: {" + dirList + "}");
      // xSemaphoreGive (fileSystemSemaphore);
      yield ();

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

  String compactConfigurationFileContent (String inp) { // skips comments, empty lines, ...
    String outp = "";
    bool inComment = false;  
    bool inQuotation = false;
    char c;
    char lastc = '\n';
    
    for (int i = 0; i < inp.length (); i++) {
      switch ((c = inp.charAt (i))) {
        case '#':   c = 0; inComment = true; break;
        case '\"':  c = 0; inQuotation = !inQuotation; break;
        case '\r':  c = 0; break;        
        case '\n':  if (lastc != '\n') { if (outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1); }
                    else c = 0;
                    inComment = inQuotation = false;
                    break;        
        case '\t':  c = ' ';
        case '=':   c = ' ';
        case ' ':   if (lastc == '\n' || lastc == ' ') c = 0;
        default:    break;
      }
      if (!inComment && c) outp += (lastc = c); 
    }
    if (outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1);
    if (outp.endsWith ("\n")) outp = outp.substring (0, outp.length () - 1);
    return outp;
  }
  
  #include "time_functions.h"
  
#endif
