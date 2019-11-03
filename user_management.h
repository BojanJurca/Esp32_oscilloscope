/*
 * 
 * user_management.h 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 *  User_management initially creates user management files:
 * 
 *    /etc/passwd        - modify the code below with your users
 *    /etc/shadow        - modify the code below with your passwords
 * 
 * Functions needed to manage users can be found in this module.
 * 
 * History:
 *          - first release, 
 *            November 18, 2018, Bojan Jurca
 *          - added SPIFFSsemaphore to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            simplified installation,
 *            April 13, 2019, Bojan Jurca
 *          - fixed bug in getUserHomeDirectory - it got additional parameter homeDir
 *            added functions userAdd and userDel
 *            September 4th, Bojan Jurca
 *  
 *  Unix user management references:
 *          - https://www.cyberciti.biz/faq/understanding-etcpasswd-file-format/
 *          - https://www.cyberciti.biz/faq/understanding-etcshadow-file/
 *          
 */


// change this definition according to your needs

#define NO_USER_MANAGEMENT                    1   // everyone is allowed to use FTP anf Telnet
#define HARDCODED_USER_MANAGEMENT             2   // define user name nad password that will be hard coded into program
#define UNIX_LIKE_USER_MANAGEMENT             3   // user names and passwords will be stored in UNIX like configuration files and checked by user_management.h functions

#ifndef USER_MANAGEMENT
  #define USER_MANAGEMENT                     UNIX_LIKE_USER_MANAGEMENT // one of the above
#endif

#ifndef __USER_MANAGEMENT__
  #define __USER_MANAGEMENT__

  // ----- includes, definitions and supporting functions -----

  #include <WiFi.h>
  

  #if USER_MANAGEMENT == NO_USER_MANAGEMENT           // ----- NO_USER_MANAGEMENT -----

    // max 32 characters including terminating /
    #define USER_HOME_DIRECTORY "/"                                                 // (home direcotry for FTP and Telnet user) change according to your needs
    #define WEBSERVER_HOME_DIRECTORY "/var/www/html/"                               // (where .html files are located) change according to your needs
    #define TELNETSERVER_HOME_DIRECTORY "/var/telnet/"                              // (where telnet help file is located) change according to your needs
    
    void usersInitialization () {;}                                                 // don't need to initialize users in this mode, we are not going to use user name and password at all
    bool checkUserNameAndPassword (char *userName, char *password) { return true; } // everyone can logg in
    // homeDir buffer must have at least 33 bytes
    char *getUserHomeDirectory (char *homeDir, char *userName) { 
                                                                 if (!strcmp (userName, "webserver"))         strcpy (homeDir, WEBSERVER_HOME_DIRECTORY);
                                                                 else if (!strcmp (userName, "telnetserver")) strcpy (homeDir, TELNETSERVER_HOME_DIRECTORY);
                                                                 else                                         strcpy (homeDir, USER_HOME_DIRECTORY);
                                                                 return homeDir;
                                                               }
  #endif  

  #if USER_MANAGEMENT == HARDCODED_USER_MANAGEMENT    // ----- HARDCODED_USER_MANAGEMENT -----

    #ifndef USERNAME
      #define USERNAME "root"                         // change according to your needs
    #endif
    #ifndef PASSWORD
      #define PASSWORD "rootpassword"                 // change according to your needs
    #endif
    // max 32 characters including terminating /
    #define USER_HOME_DIRECTORY "/"                                                 // (home direcotry for FTP and Telnet user) change according to your needs
    #define WEBSERVER_HOME_DIRECTORY "/var/www/html/"                               // (where .html files are located) change according to your needs
    #define TELNETSERVER_HOME_DIRECTORY "/var/telnet/"                              // (where telnet help file is located) change according to your needs
    
    void usersInitialization () {;}                                                 // don't need to initialize users in this mode, we are not going to use user name and password at all
    bool checkUserNameAndPassword (char *userName, char *password) { return (!strcmp (userName, USERNAME) && !strcmp (password, PASSWORD)); }
    // homeDir buffer must have at least 33 bytes
    char *getUserHomeDirectory (char *homeDir, char *userName) { 
                                                                 if (!strcmp (userName, "webserver"))         strcpy (homeDir, WEBSERVER_HOME_DIRECTORY);
                                                                 else if (!strcmp (userName, "telnetserver")) strcpy (homeDir, TELNETSERVER_HOME_DIRECTORY);
                                                                 else                                         strcpy (homeDir, USER_HOME_DIRECTORY);
                                                                 return homeDir;
                                                               }
  #endif

  #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT    // ----- UNIX_LIKE_USER_MANAGEMENT -----
  
    #include "file_system.h"  // user_management.h needs file_system.h to read/store configuration files
    #include "TcpServer.hpp"  // user_management.h needs SPIFFSsemaphore for SPIFFS file operations (already #included in file_system.h anyway) 
    
    #include <mbedtls/md.h>   // needed to calculate hashed passwords
  
    static char *__sha256__ (char *clearText) { // converts clearText into 265 bit SHA, returns character representation in hexadecimal format of hash value
      byte shaByteResult [32];
      static char shaCharResult [65];
      mbedtls_md_context_t ctx;
      mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
      mbedtls_md_init (&ctx);
      mbedtls_md_setup (&ctx, mbedtls_md_info_from_type (md_type), 0);
      mbedtls_md_starts (&ctx);
      mbedtls_md_update (&ctx, (const unsigned char *) clearText, strlen (clearText));
      mbedtls_md_finish (&ctx, shaByteResult);
      mbedtls_md_free (&ctx);
      for (int i = 0; i < 32; i++) sprintf (shaCharResult + 2 * i, "%02x", (int) shaByteResult [i]);
      return shaCharResult;  
    }
  
    void usersInitialization () {                                            // creates user management files with "root", "webadmin" and "webserver" users
                                                                             // only 3 fields are used: user name, hashed password and home directory

      String fileContent = "";
      
      // create /etc/passwd if it doesn't exist
      readEntireFile (&fileContent, "/etc/passwd");
      if (fileContent == "") {
        Serial.printf ("[user_management] /etc/passwd does not exist or it is empty, creating a new one ... ");

        fileContent = "root:x:0:::/:\r\n"
                      "webserver::100:::/var/www/html/:\r\n"    // system user needed just to determine home directory
                      "telnetserver::101:::/var/telnet/:\r\n"   // system user needed just to determine home directory
                      "webadmin:x:1000:::/var/www/html/:\r\n";
                      // TO DO: add additional users if needed

        if (writeEntireFile (fileContent, "/etc/passwd")) Serial.printf ("created.\n");
        else                                              Serial.printf ("error creating /etc/passwd.\n");
      }

      // create /etc/shadow if it doesn't exist
      readEntireFile (&fileContent, "/etc/shadow");
      if (fileContent == "") {
        Serial.printf ("[user_management] /etc/shadow does not exist or it is empty, creating a new one ... ");

        fileContent = "root:$5$" + String (__sha256__ ("rootpassword")) + ":::::::\r\n"
                      "webadmin:$5$" + String (__sha256__ ("webadminpassword")) + ":::::::\r\n";
                      // TO DO: add additional users if needed or change default passwords

        if (writeEntireFile (fileContent, "/etc/shadow")) Serial.printf ("created.\n");
        else                                              Serial.printf ("error creating /etc/shadow.\n");        
      }
    }
    
    bool checkUserNameAndPassword (char *userName, char *password) { // scan through /etc/shadow file for (user name, pasword) pair and return true if found
      
      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      
      File file;
      if ((bool) (file = SPIFFS.open ("/etc/shadow", FILE_READ)) && !file.isDirectory ()) { // read /etc/shadow file: https://www.cyberciti.biz/faq/understanding-etcshadow-file/
        do {
          char line [256]; int i = 0; while (i < sizeof (line) - 1 && file.available () && (line [i] = (char) file.read ()) >= ' ') {if (line [i] == ':') line [i] = ' '; i++;} line [i] = 0;
          if (*line < ' ') continue;
          char name [33]; char sha256Password [68];
          if (2 != sscanf (line, "%32s %67s", name, &sha256Password)) {
            Serial.printf ("[user_management] bad format of /etc/shadow file\n"); 
            file.close (); 

            xSemaphoreGive (SPIFFSsemaphore);
            
            return false;
          } // failure
          if (!strcmp (userName, name)) { // user name matches
            // if (getUserHomeDirectory (userName)) { // home directory is set so user must be valid
              if (!strcmp (sha256Password + 3, __sha256__ (password))) { // password matches
                file.close (); 
                
                xSemaphoreGive (SPIFFSsemaphore);
                
                return true; // success
              }
            // }
          }
        } while (file.available ());
        file.close (); 
        
        xSemaphoreGive (SPIFFSsemaphore);
        
        return false; // failure
      } else {
        
        xSemaphoreGive (SPIFFSsemaphore);
        
        Serial.printf ("[user_management] can't read /etc/shadow file\n"); 
        return false;
      } // failure
    } 

    // scans through /etc/passwd file for userName and returns home direcory of the user or NULL if not found
    // homeDir buffer sholud have at least 33 bytes (31 for longest SPIFFS directory name and terminating 0 and 1 for closing /)
    // SPIFFS_OBJ_NAME_LEN = 32
    char *getUserHomeDirectory (char *homeDir, char *userName) { 
      *homeDir = 0;

      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
      
      File file;
      if ((bool) (file = SPIFFS.open ("/etc/passwd", FILE_READ)) && !file.isDirectory ()) { // read /etc/passwd file: https://www.cyberciti.biz/faq/understanding-etcpasswd-file-format/
        do {
          char line [256]; int i = 0; while (i < sizeof (line) - 1 && file.available () && (line [i] = (char) file.read ()) >= ' ') i++; line [i] = 0;
          if (*line < ' ') continue;
          if (char *q = strchr (line, ':')) {
            *q = 0;
            if (!strcmp (userName, line)) if (char *p = strchr (q + 1, ':')) if (q = strchr (p + 1, ':')) if (p = strchr (q + 1, ':')) if (q = strchr (p + 1, ':')) if (p = strchr (q + 1, ':')) {
              *p = 0;
              if (strlen (q + 1) < 31) strcpy (homeDir, q + 1);
            }
          }
          if (*homeDir) {
            file.close (); 

            xSemaphoreGive (SPIFFSsemaphore);
            
            if (*(homeDir + strlen (homeDir) - 1) != '/') strcat (homeDir, "/"); 
            return homeDir; 
          } // success
        } while (file.available ());
        file.close (); 
        
        xSemaphoreGive (SPIFFSsemaphore);
        
        return NULL; // failure
      } else {
        Serial.printf ("[user_management] can't read /etc/passwd\n"); 
        
        xSemaphoreGive (SPIFFSsemaphore);
        
        return NULL;
      } // failure
    }
    
    bool passwd (String userName, String newPassword) {
      // --- remove userName from /etc/shadow ---
      String fileContent;
      bool retVal = true;
      
      if (__readEntireFileWithoutSemaphore__ (&fileContent, "/etc/shadow")) {
        fileContent.trim (); // just in case ...
        char c; int l; 
        while ((l = fileContent.length ()) && (c = fileContent.charAt (l - 1)) && (c == '\n' || c == '\r')) fileContent.remove (l - 1); // just in case ...
        // Serial.println (fileContent);
  
        // --- find the line with userName in fileContent and replace it ---
        int i = fileContent.indexOf (userName + ":"); // find userName
        if (i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) { // userName found at i
          int j = fileContent.indexOf ("\n", i); // find end of line
          if (j > 0) fileContent = fileContent.substring (0, i) + userName + ":$5$" + String (__sha256__ ((char *) newPassword.c_str ())) + ":::::::\r\n" + fileContent.substring (j + 1) + "\r\n"; // end of line found
          else       fileContent = fileContent.substring (0, i) + userName + ":$5$" + String (__sha256__ ((char *) newPassword.c_str ())) + ":::::::\r\n"; // end of line not found
          // Serial.println (fileContent);
  
          if (!__writeEntireFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
            Serial.printf ("[user_management] passwd: error writing /etc/shadow\n");
            retVal = false;
          }
        } else { // userName not found in fileContent
          Serial.printf ("[user_management] passwd: userName does not exist in /etc/shadow\n");
          retVal = false;
        }
      } else { // reading a file wasn't successfull
        Serial.printf ("[user_management] passwd: error reading /etc/shadow\n");
        retVal = false;
      }
      // --- finished processing /etc/shadow ---
      
      return retVal;
    }

    // adds userName, userId, userHomeDirectory into /etc/passwd and /etc/shadow, returns success (it is possible that only the first insert is successfull which leaves database in an undefined state)
    bool userAdd (String userName, String userId, String userHomeDirectory) {
      String fileContent;
      bool retVal = true;

      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);

      // --- add userName, userId and userHomeDirectory into /etc/passwd ---
      if (__readEntireFileWithoutSemaphore__ (&fileContent, "/etc/passwd")) {
        // Serial.println (fileContent);

        // check if userName or userId already exist
        int i = fileContent.indexOf (userName + ":");
        if (!(i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) && fileContent.indexOf (":" + userId + ":") < 0) { // userName and userId not found
          fileContent.trim (); // just in case ...
          char c; int l; 
          while ((l = fileContent.length ()) && (c = fileContent.charAt (l - 1)) && (c == '\n' || c == '\r')) fileContent.remove (l - 1); // just in case ...
          fileContent += "\r\n" + String (userName) + ":x:" + String (userId) + ":::" + String (userHomeDirectory) + ":\r\n";
          // Serial.println (fileContent);

          if (!__writeEntireFileWithoutSemaphore__ (fileContent, "/etc/passwd")) { // writing a file wasn't successfull
            Serial.printf ("[user_management] userAdd: error writing /etc/passwd\n");
            retVal = false;
          }

            // --- add default password into /etc/shadow ---
            if (__readEntireFileWithoutSemaphore__ (&fileContent, "/etc/shadow")) {
              // Serial.println (fileContent);
      
              // chekc if userName already exist
              int i = fileContent.indexOf (userName + ":");
              if (!(i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) ) { // userName not found
                fileContent.trim (); // just in case ...
                char c; int l; 
                while ((l = fileContent.length ()) && (c = fileContent.charAt (l - 1)) && (c == '\n' || c == '\r')) fileContent.remove (l - 1); // just in case ...
                fileContent += "\r\n" + userName + ":$5$" + String (__sha256__ ("changeimmediatelly")) + ":::::::\r\n";
                // Serial.println (fileContent);
      
                if (!__writeEntireFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
                  Serial.printf ("[user_management] userAdd: error writing /etc/shadow\n");
                  retVal = false;
                }
              } else {
                Serial.printf ("[user_management] userAdd: userName already exist in /etc/shadow\n");
                retVal = false;                      
              }
            } else { // reading a file wasn't successfull
              Serial.printf ("[user_management] userAdd: error reading /etc/shadow\n");
              retVal = false;
            }
            // --- finished processing /etc/shadow ---

        } else {
          Serial.printf ("[user_management] userAdd: userName or userId already exist in /etc/passwd\n");
          retVal = false;                      
        }
      } else { // reading a file wasn't successfull
        Serial.printf ("[user_management] userAdd: error reading /etc/passwd\n");
        retVal = false;
      }
      // --- finished processing /etc/passwd ---
      
      xSemaphoreGive (SPIFFSsemaphore);
      
      return retVal;
    }

    // deletes userName from /etc/passwd and /etc/shadow, returns success (it is possible that only the first deletion is successfull which leaves database in an undefined state)
    bool userDel (String userName) {
      String fileContent;
      bool retVal = true;

      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);

      // --- remove userName from /etc/passwd ---
      if (__readEntireFileWithoutSemaphore__ (&fileContent, "/etc/passwd")) {
        // Serial.println (fileContent);
 
        // find the line with userName in fileContent and remove it
        int i = fileContent.indexOf (userName + ":"); // find userName
        if (i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) { // userName found at i
          int j = fileContent.indexOf ("\n", i); // find end of line
          if (j > 0) fileContent = fileContent.substring (0, i) + fileContent.substring (j + 1); // end of line found
          else       fileContent = fileContent.substring (0, i); // end of line not found
          // Serial.println (fileContent);

          if (!__writeEntireFileWithoutSemaphore__ (fileContent, "/etc/passwd")) { // writing a file wasn't successfull
            Serial.printf ("[user_management] userDel: error writing /etc/passwd\n");
            retVal = false;            
          } else {

            // --- remove userName from /etc/shadow ---
            if (__readEntireFileWithoutSemaphore__ (&fileContent, "/etc/shadow")) {
              // Serial.println (fileContent);
       
              // --- find the line with userName in fileContent and remove it ---
              int i = fileContent.indexOf (userName + ":"); // find userName
              if (i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) { // userName found at i
                int j = fileContent.indexOf ("\n", i); // find end of line
                if (j > 0) fileContent = fileContent.substring (0, i) + fileContent.substring (j + 1); // end of line found
                else       fileContent = fileContent.substring (0, i); // end of line not found
                // Serial.println (fileContent);
      
                if (!__writeEntireFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
                  Serial.printf ("[user_management] userDel: error writing /etc/shadow\n");
                  retVal = false;
                }
              } else { // userName not found in fileContent
                Serial.printf ("[user_management] userDel: userName does not exist in /etc/shadow\n");
                retVal = false;
              }
            } else { // reading a file wasn't successfull
              Serial.printf ("[user_management] userDel: error reading /etc/shadow\n");
              retVal = false;
            }
            // --- finished processing /etc/shadow ---

          }
        } else { // userName not found in fileContent
          Serial.printf ("[user_management] userDel: userName does not exist in /etc/passwd\n");
          retVal = false;
        }
      } else { // reading a file wasn't successfull
        Serial.printf ("[user_management] userDel: error reading /etc/passwd\n");
        retVal = false;
      }
      // --- finished processing /etc/passwd ---
      
      xSemaphoreGive (SPIFFSsemaphore);
      
      return retVal;
    }
    
  #endif
  
#endif
