/*
  
   file_system.h 
  
   This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
   July, 5, 2021, Bojan Jurca
    
*/


#ifndef __FILE_SYSTEM__
  #define __FILE_SYSTEM__


  // ----- includes, definitions and supporting functions -----
  
  #include <WiFi.h>
  #include "FFat.h"
  #include "common_functions.h"   // between
  // defined in time_functions.h
  time_t timeToLocalTime (time_t t);
  struct tm timeToStructTime (time_t t);


  /*

     Support for telnet dmesg command. If telnetServer.hpp is included in the project __fileSystemDmesg__ function will be redirected
     to message queue defined there and dmesg command will display its contetn. Otherwise it will just display message on the
     Serial console.
     
  */  

  void __fileSystemDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__  // use dmesg from telnet server if possible
      dmesg (message);
    #else                     // if not just output to Serial
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
    #endif
  }
  void (* fileSystemDmesg) (String) = __fileSystemDmesg__; // use this pointer to display / record system messages

  #ifndef dmesg
    #define dmesg fileSystemDmesg
  #endif


  #define FILE_PATH_MAX_LENGTH (256 - 1) // the number of characters of longest full file path on FAT (not counting closing 0)

  bool __fileSystemMounted__ = false;

  // mount file system by calling this function
  bool mountFileSystem (bool formatIfUnformatted) { 
    __fileSystemMounted__ = false;
    
    if (FFat.begin (false)) {
      __fileSystemMounted__ = true;
    } else {
      if (formatIfUnformatted) {
        Serial.printf ("[%10lu] [file system] formatting, please wait ...\n", millis ());
        if (FFat.format ()) {
          dmesg ("[file system] formatted.");
          if (FFat.begin (false)) {
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

  bool deleteFile (String fileName) {
    if (!__fileSystemMounted__) { dmesg ("[file system] requested to delete a file but file system is not mounted." ); return false; }

    if (!FFat.remove (fileName)) {
      dmesg ("[file system] unable to delete " + fileName);
      return false;
    }
    return true;    
  }

  // removes ending / from directory path if exists and if it is not root directory
  String removeExtraSlash (String path) {
    if (path.substring (path.length () - 1) == "/") path = path.substring (0, path.length () - 1); if (path == "") path = "/"; // except for /, path never ends with /
    return path;
  }
  
  bool makeDir (String directory) {
    if (!__fileSystemMounted__) { dmesg ("[file system] requested to make a directory but file system is not mounted." ); return false; }

    directory = removeExtraSlash (directory);

    if (!FFat.mkdir (directory)) {
      dmesg ("[file system] unable to make " + directory);
      return false;
    }
    return true;    
  }

  bool removeDir (String directory) {
    if (!__fileSystemMounted__) { dmesg ("[file system] requested to remove a directory but file system is not mounted." ); return false; }

    directory = removeExtraSlash (directory);

    if (!FFat.rmdir (directory)) {
      dmesg ("[file system] unable to remove " + directory);
      return false;
    }
    return true;    
  }

  // reads entire file into String - returns success
  bool readFile (String& fileContent, String fileName, bool ignoreCR = false) {
    fileContent = "";
    
    File f = FFat.open (fileName, FILE_READ);
    if (f) {
      if (!f.isDirectory ()) {
        while (f.available ()) { char c = (char) f.read (); if (c != '\r' || !ignoreCR) fileContent += c; }
        f.close ();
        return true;
      } else { 
        dmesg ("[file_system] " + String (fileName) + " is a directory.");
      }
      f.close ();
    }             
    return false;
  }  

  // reads entire file, ignoring \r
  String readTextFile (String fileName) { 
    String fileContent;
    readFile (fileContent, fileName, true);
    return fileContent;
  }

  // reads entire file into buffer allocted by malloc, if successful returns pointer to buffer - it has to be free-d after not needed any more
  byte *readFile (String fileName, size_t *buffSize) {
    byte *retVal = NULL;

    File f = FFat.open (fileName, FILE_READ);
    if (f) {
      if (!f.isDirectory ()) {
        *buffSize = f.size ();
        retVal = (byte *) malloc (*buffSize);
        if (retVal) {
          byte *p = (byte *) retVal;
          int i = 0;
          while (f.available () && i++ < *buffSize) *(p++) = (byte) f.read ();
          *buffSize = i;
        } else dmesg ("[file system] malloc failed in " + String (__func__)); 
        f.close ();
      }
    }

    return retVal;      
  }


  // writes String into file file, returns success
  bool writeFile (String& fileContent, String fileName) {
    File f = FFat.open (fileName, FILE_WRITE);
    if (f) {
      if (!f.isDirectory ()) {
        if (f.printf (fileContent.c_str ()) == strlen (fileContent.c_str ())) {
          f.close ();
          return true;        
        } else {
          dmesg ("[file_system] can't write " + String (fileName));
        }
      }
      f.close ();
    }    
    return false;
  }  

  // constructs full path from path and working directory, returns "" if full path is invalid
  String fullFilePath (String path, String workingDirectory) {
    path = removeExtraSlash (path);
    if (workingDirectory.substring (workingDirectory.length () - 1) != "/") workingDirectory += '/'; // workingDirectory now always ends with /
    
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

  // the same as fullFilePath except that it always ends with /
  String fullDirectoryPath (String path, String workingDirectory) {  
    String s = fullFilePath (path, workingDirectory);
    if (s != "") if (s.substring (s.length () - 1) != "/") s += '/';
    return s;
  }

  // checks if user, whose home directory is known, has a right to access full path (if the beginning of the path matches)
  bool userMayAccess (String fullPath, String homeDir) {
    return fullPath.substring (0, homeDir.length ()) == homeDir;
  }

  // check if full path is a directory
  bool isDirectory (String fullPath) {
    fullPath = removeExtraSlash (fullPath);
    
    bool b = false;
    
    File f = FFat.open (fullPath, FILE_READ);
    if (f) {
      b = f.isDirectory ();
      f.close ();
    }     
    return b;
  }

  // check if full path is a file
  bool isFile (String fullPath) {
    bool b = false;
    
    File f = FFat.open (fullPath, FILE_READ);
    if (f) {
      b = !f.isDirectory ();
      f.close ();
    } 
    return b;
  }

  // returns file information in Unix like format
  String fileInfo (String fileOrDirectory, bool showFullPath) {
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
      s = String (line) + n;
      f.close ();
    }
    return s;
  }

  // returns the content of the whole directory
  String listDirectory (String directory) {
    if (!__fileSystemMounted__) return "File system not mounted. You may have to use mkfs.fat to format flash disk first.";

    directory = removeExtraSlash (directory);

    String list = "";

    File d = FFat.open (directory);
    if (d) {
      for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
        String s = fileInfo (f.name (), false);
        if (s > "") {
          if (list > "") list += "\r\n";
          list += s;
        }
        yield ();
      }
      d.close ();
    }
    return list;    
  }

  // returns the content of whole subdirectory tree
  String recursiveListDirectory (String directory) {
    if (!__fileSystemMounted__) return "File system not mounted. You may have to use mkfs.fat to format flash disk first.";

    directory = removeExtraSlash (directory);    

    // create 2 lists: fileList and directoryList  
    String dirList = "\n"; // \n separated list od directories
    String fileList = "";  // list of files formatted for output

    // 1. display directory info
    String s = fileInfo (directory, true);
    if (s > "") {
      if (fileList > "") fileList += "\r\n";
      fileList += s; 
    }

    // 2. display information about files and remember subdirectories
    File d = FFat.open (directory);
    if (!d) {
      return "";
    } else {
      for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
        if (f.isDirectory ()) {

          // save directory name for later recursion
          dirList += String (f.name ()) + "\n";

        } else {

          String s = fileInfo (f.name (), true);
          if (s > "") {
            if (fileList > "") fileList += "\r\n";
            fileList += s;
          }
          yield ();
          
        }
      }     
      d.close ();
      // Serial.println ("List of directories: {" + dirList + "}");
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

  // removes configuration comments, empty lines, ...
  String compactConfigurationFileContent (String inp) { 
    String outp = "";
    bool comment = false;  
    char c;
    char lastc = '\n';
    
    for (int i = 0; i < inp.length (); i++) {
      switch ((c = inp.charAt (i))) {
        case '#':   c = 0; comment = true; break;
        case '\r':  c = 0; break;        
        case '\n':  if (lastc != '\n') { if (outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1); }
                    else c = 0;
                    comment = false;
                    break;        
        case '\t':  c = ' ';
        case ' ':   if (lastc == '\n' || lastc == ' ') c = 0;
        default:    break;
      }
      if (!comment && c) outp += (lastc = c); 
    }
    if (outp.endsWith (" ")) outp = outp.substring (0, outp.length () - 1);
    if (outp.endsWith ("\n")) outp = outp.substring (0, outp.length () - 1);
    return outp;
  }
  
  #include "time_functions.h"

  
#endif
