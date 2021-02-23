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
 *          - added fileSystemSemaphore to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            simplified installation,
 *            April 13, 2019, Bojan Jurca
 *          - fixed bug in getUserHomeDirectory - it got additional parameter homeDir
 *            added functions userAdd and userDel
 *            September 4th, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca     
 *          - port from SPIFFS to FAT file system, adjustment for Arduino 1.8.13
 *            October 10, 2020, Bojan Jurca
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
  #include "common_functions.h"

  #define USER_PASSWORD_MAX_LENGTH 64 // the number of characters of longest user name or password (not counting closing 0)
  
  #if USER_MANAGEMENT == NO_USER_MANAGEMENT           // ----- NO_USER_MANAGEMENT -----
    
    inline void initializeUsersAtFirstCall () {;}        // don't need to initialize users in this mode, we are not going to use user name and password at all
    bool checkUserNameAndPassword (String& userName, String& password) { return true; } // everyone can logg in
    String getUserHomeDirectory (String userName) {
                                                    if (userName == "webserver")    return "/var/www/html/";
                                                    if (userName == "telnetserver") return "/var/telnet/";
                                                                                    return  "/";
                                                  }
  #endif  

  #if USER_MANAGEMENT == HARDCODED_USER_MANAGEMENT    // ----- HARDCODED_USER_MANAGEMENT -----

    #ifndef ROOT_PASSWORD
      #define ROOT_PASSWORD "rootpassword"                 // change according to your needs
    #endif


    inline void initializeUsersAtFirstCall () {;}        // don't need to initialize users in this mode
    bool checkUserNameAndPassword (String userName, String password) { return (userName == "root" && password == ROOT_PASSWORD); }
    String getUserHomeDirectory (String userName) { 
                                                    if (userName == "webserver")    return "/var/www/html/";
                                                    if (userName == "telnetserver") return "/var/telnet/";
                                                                                    return  "/";
                                                  }

  #endif

  #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT    // ----- UNIX_LIKE_USER_MANAGEMENT -----
  
    #include "file_system.h"  // user_management.h needs file_system.h to read/store configuration files
    
    #include "mbedtls/md.h"   // needed to calculate hashed passwords
  
    String __sha256__ (const char *clearText) { // converts clearText into 265 bit SHA, returns character representation in hexadecimal format of hash value
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
      return String (shaCharResult);  
    }
  
    void initializeUsersAtFirstCall () {                                     // creates user management files with root, webadmin, webserver and telnetserver users, if they don't exist
                                                                             // only 3 fields are used: user name, hashed password and home directory
      if (!__fileSystemMounted__) {
        Serial.printf ("[%10lu] [user management] file system is not mounted, can't read or write user configuration files.\n", millis ()); 
        return; // if there is no file system we can not write configuration files
      }

      String fileContent = "";
      
      // create /etc/passwd if it doesn't exist
      readFile (fileContent, "/etc/passwd");
      if (fileContent == "") {
        FFat.mkdir ("/etc"); // location of this file
        FFat.mkdir ("/var"); FFat.mkdir ("/var/www"); FFat.mkdir ("/var/www/html"); // webserver home directory
        FFat.mkdir ("/var/telnet"); // telnetserver home directory
        
        Serial.printf ("[%10lu] [user_management] /etc/passwd does not exist or it is empty, creating a new one ... ", millis ());

        fileContent = "root:x:0:::/:\r\n"
                      "webserver::100:::/var/www/html/:\r\n"    // system user needed just to determine home directory 
                      "telnetserver::101:::/var/telnet/:\r\n"   // system user needed just to determine home directory 
                      "webadmin:x:1000:::/var/www/html/:\r\n";  //  
                      // TO DO: add additional users if needed

        if (writeFile (fileContent, "/etc/passwd")) Serial.printf ("created.\n");
        else                                        Serial.printf ("error.\n");
      }

      // create /etc/shadow if it doesn't exist
      readFile (fileContent, "/etc/shadow");
      if (fileContent == "") {
        Serial.printf ("[%10lu] [user_management] /etc/shadow does not exist or it is empty, creating a new one ... ", millis ());

        fileContent = "root:$5$" + __sha256__ ((char *) "rootpassword") + ":::::::\r\n"
                      "webadmin:$5$" + __sha256__ ((char *) "webadminpassword") + ":::::::\r\n";
                      // TO DO: add additional users if needed or change default passwords

        if (writeFile (fileContent, "/etc/shadow")) Serial.printf ("created.\n");
        else                                        Serial.printf ("error.\n");        
      }
    }
    
    bool checkUserNameAndPassword (String& userName, String& password) { // scan through /etc/shadow file for (user name, pasword) pair and return true if found
      // /etc/shadow file: https://www.cyberciti.biz/faq/understanding-etcshadow-file/
      return (between (between ("\n" + readTextFile ("/etc/shadow") + "\n", "\n" + userName + ":", "\n"), "$5$", ":") == __sha256__ ((char *) password.c_str ()));
    } 

    String getUserHomeDirectory (String userName) { // always ends with /
      String directory = between (between ("\n" + readTextFile ("/etc/passwd") + "\n", "\n" + userName + ":", "\n"), ":::", ":");  
      if (directory.substring (directory.length () - 1) != "/") directory += '/';
      return directory;
    }
    
    bool passwd (String userName, String newPassword) {
      // --- remove userName from /etc/shadow ---
      String fileContent;
      bool retVal = true;
      
      if (__readFileWithoutSemaphore__ (fileContent, "/etc/shadow")) {
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
  
          if (!__writeFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
            Serial.printf ("[%10lu] [user_management] passwd: error writing /etc/shadow\n", millis ());
            retVal = false;
          }
        } else { // userName not found in fileContent
          Serial.printf ("[%10lu] [user_management] passwd: userName does not exist in /etc/shadow\n", millis ());
          retVal = false;
        }
      } else { // reading a file wasn't successfull
        Serial.printf ("[%10lu] [user_management] passwd: error reading /etc/shadow\n", millis ());
        retVal = false;
      }
      // --- finished processing /etc/shadow ---
      
      return retVal;
    }

    // adds userName, userId, userHomeDirectory into /etc/passwd and /etc/shadow, creates home directory and returns success or error message
    String userAdd (String userName, String userId, String userHomeDirectory) {
      String fileContent;
      String retVal = "User created.";
      char c; int l;
      char dName [FILE_PATH_MAX_LENGTH + 1]; 
      bool b;

      if (strlen (userName.c_str ()) > USER_PASSWORD_MAX_LENGTH) return "User name too long.";
      userHomeDirectory = fullDirectoryPath (userHomeDirectory, "/");
      if (userHomeDirectory == "") return "Invalid home directory name.";
      if (strlen (userHomeDirectory.c_str ()) > FILE_PATH_MAX_LENGTH) return "Home directory name too long.";

      // --- add userName, userId and userHomeDirectory into /etc/passwd ---
      if (!__readFileWithoutSemaphore__ (fileContent, "/etc/passwd")) {
        return "Can't read /etc/passwd";
      }
      if (("\n" + fileContent).indexOf ("\n" + userName + ":") > -1) {
        return "User " + userName + " already exists.";
      }
      if (fileContent.indexOf (":" + userId + ":") > -1) {
        return "User " + userId + " already exists.";
      }
      fileContent.trim (); // just in case ...
      while ((l = fileContent.length ()) && (c = fileContent.charAt (l - 1)) && (c == '\n' || c == '\r')) fileContent.remove (l - 1); // just in case ...
      fileContent += "\r\n" + String (userName) + ":x:" + String (userId) + ":::" + String (userHomeDirectory) + ":\r\n";
      if (!__writeFileWithoutSemaphore__ (fileContent, "/etc/passwd")) { // writing a file wasn't successfull
        return "Can't write /etc/passwd";
      }

      // --- add default password into /etc/shadow ---
      if (!__readFileWithoutSemaphore__ (fileContent, "/etc/shadow")) {
        return "Can't read /etc/shadow";          
      }
      if (("\n" + fileContent).indexOf ("\n" + userName + ":") > -1) {
        return "User " + userName + " already has password. Delete the user and try again.";
      }
      fileContent.trim (); // just in case ...
      while ((l = fileContent.length ()) && (c = fileContent.charAt (l - 1)) && (c == '\n' || c == '\r')) fileContent.remove (l - 1); // just in case ...
      fileContent += "\r\n" + userName + ":$5$" + String (__sha256__ ((char *) "changeimmediatelly")) + ":::::::\r\n";
      if (!__writeFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
        return "Can't write /etc/shadow";
      }

      // --- crate user home directory ---
      b = false;
      strcpy (dName, userHomeDirectory.c_str ());
      for (int i = 0; dName [i] && dName [i]; i++) if (i && dName [i] == '/') { 
        dName [i] = 0; 
        b = FFat.mkdir (dName);
        dName [i] = '/'; 
      }
      if (!b) retVal = "User created but couldn't create user's home directory " + userHomeDirectory;
      
      return retVal;
    }

    // deletes userName from /etc/passwd and /etc/shadow, deletes home directory and returns success or error message
    String userDel (String userName) {
      String fileContent;
      String retVal = "User deleted.";
      int i, j;
      String userHomeDirectory = fullDirectoryPath (getUserHomeDirectory (userName), "/");      
      char dName [FILE_PATH_MAX_LENGTH + 1]; 
      bool b, first;

      // --- remove userName from /etc/passwd ---
      if (!__readFileWithoutSemaphore__ (fileContent, "/etc/passwd")) {
        return "Can't read /etc/passwd";          
      }
      i = fileContent.indexOf (userName + ":"); // find userName
      if (i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) { // userName found at i
        j = fileContent.indexOf ("\n", i); // find end of line
        if (j > 0) fileContent = fileContent.substring (0, i) + fileContent.substring (j + 1); // end of line found
        else       fileContent = fileContent.substring (0, i); // end of line not found
        if (!__writeFileWithoutSemaphore__ (fileContent, "/etc/passwd")) { // writing a file wasn't successfull
          return "Can't write /etc/passwd";         
        }
      } else {
        return "User " + userName + " does not exists.";          
      }

      // --- remove userName from /etc/shadow ---
      if (!__readFileWithoutSemaphore__ (fileContent, "/etc/shadow")) {
        return "Can't read /etc/shadow";          
      }
      i = fileContent.indexOf (userName + ":"); // find userName
      if (i == 0 || (i > 0 && fileContent.charAt (i - 1) == '\n')) { // userName found at i
        j = fileContent.indexOf ("\n", i); // find end of line
        if (j > 0) fileContent = fileContent.substring (0, i) + fileContent.substring (j + 1); // end of line found
        else       fileContent = fileContent.substring (0, i); // end of line not found
        if (!__writeFileWithoutSemaphore__ (fileContent, "/etc/shadow")) { // writing a file wasn't successfull
          return "Can't write /etc/shadow"; 
        }
      }

      // --- delete user's home directory ---
      b = false; first = true;
      if (strlen (userHomeDirectory.c_str ()) <= FILE_PATH_MAX_LENGTH) {
        strcpy (dName, userHomeDirectory.c_str ());
        for (int i = strlen (dName); i; i--) if (dName [i] == '/')  { 
          dName [i] = 0; 
          if (!FFat.rmdir (dName)) break;
          if (first) b = true;
          first = false;
        }          
      }
      if (!b) retVal = "User deleted but couldn't remove user's home directory " + userHomeDirectory;
          
      return retVal;
    }
    
  #endif
  
#endif
