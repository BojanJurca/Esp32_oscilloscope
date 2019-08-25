/*
 * 
 * File_system.h 
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
 *  
 */


#ifndef __FILE_SYSTEM__
  #define __FILE_SYSTEM__

  #ifndef PROJECT_ID
    #define PROJECT_ID "undefined"
  #endif
  
  #include "TcpServer.hpp"
  
 
  bool SPIFFSmounted = false;                                     // use this variable to check if file system has mounted before I/O operations  

  #include <SPIFFS.h>

  String readEntireTextFile (char *fileName);
  
  bool mountSPIFFS () {                                           // mount file system by calling this function
    
    xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);

    if (SPIFFS.begin (false)) {
      
      xSemaphoreGive (SPIFFSsemaphore);
          
      Serial.printf ("[file system] SPIFFS mounted\n");
      return SPIFFSmounted = true;
    } else {
      Serial.printf ("[file system] formatting, please wait ... "); 
      if (SPIFFS.format ()) {
        Serial.printf ("formatted\n");
        if (SPIFFS.begin (false)) {
          
          xSemaphoreGive (SPIFFSsemaphore);
          
          Serial.printf ("[file system] SPIFFS mounted\n");
          return SPIFFSmounted = true;
        } else {
          
          xSemaphoreGive (SPIFFSsemaphore);
          
          Serial.printf ("[file system] SPIFFS mount failed\n");
          return false;      
        }
      } else {
        
        xSemaphoreGive (SPIFFSsemaphore);
        
        Serial.printf ("[file system] SPIFFS formatting failed\n");
        return false;
      }
    }
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
      Serial.printf ("[file system] can't read %s\n", fileName);
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

  void writeProjectIdOnFlashDrive () {
    if (readEntireTextFile ("/ID") != PROJECT_ID) {
      if (File file = SPIFFS.open ("/ID", FILE_WRITE)) {
        file.printf ("%s", PROJECT_ID);
        file.close ();           
      }
    }          
  }

#endif
