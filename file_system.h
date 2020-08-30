/*
 * 
 * file_system.h 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *
 *  A documentation on SPIFFS can be found here: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
 * 
 * History:
 *          - first release, 
 *            November 15, 2018, Bojan Jurca
 *          - added SPIFFSsemaphore and SPIFFSsafeDelay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca
 *          - added functions __readEntireFileWithoutSemaphore__ and __writeEntireFileWithoutSemaphore__
 *            September 8, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca
 *  
 */


#ifndef __FILE_SYSTEM__
  #define __FILE_SYSTEM__

  // ----- includes, definitions and supporting functions -----
  
  #include <WiFi.h>
  #include <SPIFFS.h>  
  #include "TcpServer.hpp" // we'll need SPIFFSsemaphore defined in TcpServer.hpp 


  void __fileSystemDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__ // use dmesg from telnet server if possible
      dmesg (message);
    #else
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
    #endif
  }
  void (* fileSystemDmesg) (String) = __fileSystemDmesg__; // use this pointer to display / record system messages

  bool __fileSystemMounted__ = false;


  bool mountSPIFFS (bool formatIfUnformatted) {                                           // mount file system by calling this function
    bool b;
    
      fileSystemDmesg ("[file system] mounting SPIFFS ...");
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      b = SPIFFS.begin (formatIfUnformatted);
    xSemaphoreGive (SPIFFSsemaphore);
      if (b) {
        __fileSystemMounted__ = true;
        fileSystemDmesg ("[file system] SPIFFS mounted.");
      } else {
        fileSystemDmesg ("[file system] SPIFFS failed to mount.");
      }
      return b;
  }

  // reads entire file into String without using sempahore - it is expected that calling functions would provide it - returns success
  bool __readEntireFileWithoutSemaphore__ (String *fileContent, const char *fileName) {
    *fileContent = "";
    
    File file;
    if ((bool) (file = SPIFFS.open (fileName, "r")) && !file.isDirectory ()) {
      while (file.available ()) *fileContent += String ((char) file.read ());
      file.close ();
      return true;
    } else { 
      Serial.printf ("[%10lu] [file_system] can't read %s\n", millis (), fileName);
      file.close ();
      return false;      
    }
  }  

  // writes String into file file without using sempahore - it is expected that calling functions would provide it - returns success
  bool __writeEntireFileWithoutSemaphore__ (String fileContent, const char *fileName) {
    File file;
    if ((bool) (file = SPIFFS.open (fileName, "w")) && !file.isDirectory ()) {
      if (file.printf (fileContent.c_str ()) != strlen (fileContent.c_str ())) { // can't write file
        file.close ();
        Serial.printf ("[%10lu] [file_system] can't write %s\n", millis (), fileName);
        return false;                
      } else {
        file.close ();
        return true;        
      }
    }
    return false; // never executes
  }  

  // reads entire file into String, returns success
  bool readEntireFile (String *fileContent, const char *fileName) {
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      bool b = __readEntireFileWithoutSemaphore__ (fileContent, fileName);
    xSemaphoreGive (SPIFFSsemaphore);
    return b;      
  }

  // writes String into file file, returns success
  bool writeEntireFile (String& fileContent, const char *fileName) {
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      bool b = __writeEntireFileWithoutSemaphore__ (fileContent, fileName);
    xSemaphoreGive (SPIFFSsemaphore);
    return b;  
  }  
  
  String readEntireTextFile (const char *fileName) { // reads entire file into String (ignoring \r) - it is supposed to be used for small files
    String s = "";
    char c;
    File file;
    
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      if ((bool) (file = SPIFFS.open (fileName, FILE_READ)) && !file.isDirectory ()) {
        while (file.available ()) if ((c = file.read ()) != '\r') s += String (c);
        file.close (); 
      } else {
        Serial.printf ("[%10lu] [file system] can't read %s\n", millis (), fileName);
      }
    xSemaphoreGive (SPIFFSsemaphore);
    
    return s;  
  }

  // reads entire file into buffer allocted by malloc, returns success pointer to buffer - it has to be
  // free-d after not needed any more
  byte *readEntireFile (char *fileName, size_t *buffSize) {
    byte *retVal = NULL;
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);

      File file;
      if ((bool) (file = SPIFFS.open (fileName, "r")) && !file.isDirectory ()) {
        *buffSize = file.size ();
        if ((retVal = (byte *) malloc (*buffSize))) {
          char *p = (char *) retVal;
          int i = 0;
          while (file.available () && i++ < *buffSize) *(p++) = file.read ();
          *buffSize = i;
        } else fileSystemDmesg ("[file system] malloc failed in function " + String (__func__) + "."); 
        file.close ();
      } else fileSystemDmesg ("[file system] can't open " + String (fileName) + "."); 
      
    xSemaphoreGive (SPIFFSsemaphore);
    return retVal;      
  }

  void listFilesOnFlashDrive () {
    Serial.printf ("\r\nFiles present on built-in flash disk:\n\n");
  
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      File dir = SPIFFS.open ("/");
      if (!dir) {
        Serial.printf ("\r\n\nFailed to open root directory /.\n");
      } else {
        if (!dir.isDirectory ()) {
          Serial.printf ("\r\n\nInvalid directory.\n");
        } else {
          File file = dir.openNextFile ();
          while (file) {
            if(!file.isDirectory ()) Serial.printf ("  %6i bytes   %s\n", file.size (), file.name ());
            file = dir.openNextFile ();
          }
        }
      }
    xSemaphoreGive (SPIFFSsemaphore);
  
    Serial.printf ("\n");    
  }

#endif
