/*
 * 
 * file_system.h 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *
 *  Only flash disk formating and mounting of SPIFFS file system is defined here, a documentation on SPIFFS
 *  can be found here: http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html
 * 
 * History:
 *          - first release, 
 *            November 15, 2018, Bojan Jurca
 *          - added SPIFFSsemaphore and SPIFFSsafeDelay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca
 *          - added functions __readEntireFileWithoutSemaphore__ and __writeEntireFileWithoutSemaphore__
 *            September 8, Bojan Jurca
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
      Serial.println (message); 
    #endif
  }
  void (* fileSystemDmesg) (String) = __fileSystemDmesg__; // use this pointer to display / record system messages

  bool __fileSystemMounted__ = false;


  bool mountSPIFFS (bool formatIfUnformatted) {                                           // mount file system by calling this function
    
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);

    if (SPIFFS.begin (false)) {
      
      xSemaphoreGive (SPIFFSsemaphore);

      fileSystemDmesg ("[file system] SPIFFS mounted.");
      __fileSystemMounted__ = true;
      return true;
    } else {
      if (formatIfUnformatted) {
        Serial.printf ("[%10d] [file system] formatting, please wait ...\n", millis ()); 
        if (SPIFFS.format ()) {
          fileSystemDmesg ("[file system] formatted.");
          if (SPIFFS.begin (false)) {
          
            xSemaphoreGive (SPIFFSsemaphore);
          
            fileSystemDmesg ("[file system] SPIFFS mounted.");
            return true;
          } else {
          
            xSemaphoreGive (SPIFFSsemaphore);
          
            fileSystemDmesg ("[file system] SPIFFS failed to mount.");
            return false;      
          }
        } else {
        
          xSemaphoreGive (SPIFFSsemaphore);
        
          fileSystemDmesg ("[file system] SPIFFS formatting failed.");
          return false;
        }
      } else {
        fileSystemDmesg ("[file system] SPIFFS failed to mount.");
      }
    }
  }

  // reads entire file into String without using sempahore - it is expected that calling functions would provide it - returns success
  bool __readEntireFileWithoutSemaphore__ (String *fileContent, char *fileName) {
    *fileContent = "";
    
    File file;
    if ((bool) (file = SPIFFS.open (fileName, "r")) && !file.isDirectory ()) {
      while (file.available ()) *fileContent += String ((char) file.read ());
      file.close ();
      return true;
    } else { 
      Serial.printf ("[%10d] [file_system] can't read %s\n", millis (), fileName);
      file.close ();
      return false;      
    }
  }  

  // writes String into file file without using sempahore - it is expected that calling functions would provide it - returns success
  bool __writeEntireFileWithoutSemaphore__ (String fileContent, char *fileName) {
    File file;
    if ((bool) (file = SPIFFS.open (fileName, "w")) && !file.isDirectory ()) {
      if (file.printf (fileContent.c_str ()) != strlen (fileContent.c_str ())) { // can't write file
        file.close ();
        Serial.printf ("[%10d] [file_system] can't write %s\n", millis (), fileName);
        return false;                
      } else {
        file.close ();
        return true;        
      }
    }
  }  

  // reads entire file into String, returns success
  bool readEntireFile (String *fileContent, char *fileName) {
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      bool b = __readEntireFileWithoutSemaphore__ (fileContent, fileName);
    xSemaphoreGive (SPIFFSsemaphore);
    return b;      
  }

  // writes String into file file, returns success
  bool writeEntireFile (String& fileContent, char *fileName) {
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      bool b = __writeEntireFileWithoutSemaphore__ (fileContent, fileName);
    xSemaphoreGive (SPIFFSsemaphore);
    return b;  
  }  
  
  String readEntireTextFile (char *fileName) { // reads entire file into String (ignoring \r) - it is supposed to be used for small files
    String s = "";
    char c;
    File file;
    
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      if ((bool) (file = SPIFFS.open (fileName, FILE_READ)) && !file.isDirectory ()) {
        while (file.available ()) if ((c = file.read ()) != '\r') s += String (c);
        file.close (); 
      } else {
        Serial.printf ("[%10d] [file system] can't read %s\n", millis (), fileName);
      }
    xSemaphoreGive (SPIFFSsemaphore);
    
    return s;  
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
