/*

    user_management.h 
  
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
    User_management initially creates user management files:
  
      /etc/passwd        - modify the code below with your users
      /etc/shadow        - modify the code below with your passwords
  
   Functions needed to manage users can be found in this module.
  
    Unix user management references:
            - https://www.cyberciti.biz/faq/understanding-etcpasswd-file-format/
            - https://www.cyberciti.biz/faq/understanding-etcshadow-file/          

    November 26, 2021, Bojan Jurca

   Nomenclature used in webServer.hpp - for easier understaning of the code:

      - "buffer size" is the number of bytes that can be placed in a buffer. 

                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".      

      - "max length" is the number of characters that can be placed in a variable.

                  In case of C 0-terminated strings the terminating 0 character is not included so the buffer should be at least 1 byte larger.
 */


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>


#ifndef __USER_MANAGEMENT__
  #define __USER_MANAGEMENT__


    // TUNNING PARAMETERS

    // choose the way ESP32 server is going to handle users
          #define NO_USER_MANAGEMENT        1   // everyone is allowed to use FTP anf Telnet
          #define HARDCODED_USER_MANAGEMENT 2   // define user name nad password that will be hard coded into program
          #define UNIX_LIKE_USER_MANAGEMENT 3   // user names and passwords will be stored in UNIX like configuration files and checked by user_management.h functions
    // one of the above
    #ifndef USER_MANAGEMENT
      #define USER_MANAGEMENT UNIX_LIKE_USER_MANAGEMENT // by default
    #endif

    #define USER_PASSWORD_MAX_LENGTH 64   // the number of characters of longest user name or password 
    #define MAX_ETC_PASSWD_SIZE 2 * 1024  // 2 KB will usually do - some functions read the whole /etc/passwd file in the memory 
    #define MAX_ETC_SHADOW_SIZE 2 * 1024  // 2 KB will usually do - some functions read the whole /etc/shadow file in the memory

    #define DEFAULT_ROOT_PASSWORD "rootpassword"
    #define DEFAULT_WEBADMIN_PASSWORD "webadminpassword"
    #define DEFAULT_USER_PASSWORD "changeimmediatelly"


    // ----- functions and variables in this modul -----

    void initializeUsers ();
    bool checkUserNameAndPassword (char *, char *);
    bool getUserHomeDirectory (char *, size_t, char *);
    String getUserHomeDirectory (char *userName);
    #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT 
      bool passwd (char *, char *, bool, bool);
      char *userAdd (char *, char *, char *, bool, bool);
      char *userDel (char *);
    #endif


    // ----- CODE -----

    #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT
      // needed for storing sha of passwords
      #include <mbedtls/md.h>

      #ifndef __SHA256__
        #define __SHA256__
          bool sha256 (char *buffer, size_t bufferSize, const char *clearText) { // converts clearText to 256 bit SHA, returns character representation in hexadecimal format of hash value
            *buffer = 0;
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
            if (bufferSize > strlen (shaCharResult)) { strcpy (buffer, shaCharResult); return true; }
            return false; 
          }
      #endif    

      #include "file_system.h"  // we need file_system.h to read and store configuration files /etc/passwd and /etc/shadow

      #ifndef __STR_BETWEEN__  
        #define __STR_BETWEEN__
        String strBetween (String input, String openingBracket, String closingBracket) { // returns content inside of opening and closing brackets
          int i = input.indexOf (openingBracket);
          if (i >= 0) {
            input = input.substring (i +  openingBracket.length ());
            i = input.indexOf (closingBracket);
            if (i >= 0) {
              input = input.substring (0, i);
              input.trim ();
              return input;
            }
          }
          return "";
        }
  
        char *strBetween (char *buffer, size_t bufferSize, char *src, const char *left, const char *right) { // copies substring of src between left and right to buffer or "" if not found or buffer too small, return bufffer
          *buffer = 0;
          char *l, *r;
          if ((l = strstr (src, left))) {  
            l += strlen (left);
            if ((r = strstr (l, right))) { 
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
      
    #endif
  
    #if USER_MANAGEMENT == NO_USER_MANAGEMENT // no user management at all
  
      void initializeUsers () {;}                                                     // don't need to initialize users in this mode, we are not going to use user name and password at all
      bool checkUserNameAndPassword (char *userName, char *password) { return true; } // everyone can logg in
      bool getUserHomeDirectory (char *buffer, size_t bufferSize, char *userName) {   // must always end with '/'    
                                                                                    if (bufferSize) {
                                                                                      *buffer = 0; strcpy (buffer, "/"); return true; 
                                                                                    }
                                                                                    return false;
                                                                                  }
      String getUserHomeDirectory (char *userName) { return "/"; } // always ends with /
    #endif
  
    #if USER_MANAGEMENT == HARDCODED_USER_MANAGEMENT // only 'root' user with hard-coded password
      void initializeUsers () {;}                                                     // don't need to initialize users in this mode, we are not going to use user name and password at all
      bool checkUserNameAndPassword (char *userName, char *password) { return (!strcmp (userName, "root") && !strcmp (password, DEFAULT_ROOT_PASSWORD)); }
      bool getUserHomeDirectory (char *buffer, size_t bufferSize, char *userName) { // always ends with /
                                                                                    if (bufferSize) {
                                                                                      *buffer = 0; 
                                                                                      if (!strcmp (userName, "root")) strcpy (buffer, "/"); return true; 
                                                                                    }
                                                                                    return false;
                                                                                  }
      String getUserHomeDirectory (char *userName) { return !strcmp (userName, "root") ? "/" : ""; } // always ends with /
    #endif

  #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT // Unix like user managmenet with /etc/passwd and /etc/shadow files

    void initializeUsers () { // creates user management files with root, webadmin, webserver and telnetserver users, if they don't exist - only 3 fields are used: user name, hashed password and home directory
      if (!__fileSystemMounted__) {
        Serial.printf ("[%10lu] [user management] file system is not mounted, can't read or write user configuration files.\n", millis ()); 
        return; // if there is no file system, we can not write configuration files and read from them
      }
      // create dirctory structure
      if (!isDirectory ("/etc")) { fileSystem.mkdir ("/etc"); }
      // create /etc/passwd if it doesn't exist
      {
        if (!isFile ("/etc/passwd")) {
          Serial.printf ("[%10lu] [user_management] /etc/passwd does not exist, creating default one ... ", millis ());
          bool created = false;
          File f = fileSystem.open ("/etc/passwd", FILE_WRITE);
          if (f) {
            char *defaultContent = (char *) "root:x:0:::/:\r\n"
                                            "webadmin:x:1000:::/var/www/html/:\r\n";
            created = (f.printf (defaultContent) == strlen (defaultContent));                                
            f.close ();
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (defaultContent); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif                        
          }
          Serial.printf (created ? "created\n" : "error\n");
        }
      }
      // create /etc/shadow if it doesn't exist
      {
        if (!isFile ("/etc/shadow")) {
          Serial.printf ("[%10lu] [user_management] /etc/shadow does not exist, creating default one ... ", millis ());
          bool created = false;
          File f = fileSystem.open ("/etc/shadow", FILE_WRITE);
          if (f) {
            char rootpassword [65]; sha256 (rootpassword, sizeof (rootpassword), DEFAULT_ROOT_PASSWORD);
            char webadminpassword [65]; sha256 (webadminpassword, sizeof (webadminpassword), DEFAULT_WEBADMIN_PASSWORD);
            char defaultPasswords [256]; sprintf (defaultPasswords, "root:$5$%s:::::::\r\n"
                                                                    "webadmin:$5$%s:::::::\r\n", rootpassword, webadminpassword);
            created = f.printf (defaultPasswords) == strlen (defaultPasswords);
            f.close ();
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (defaultPasswords); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif            
          }
          Serial.printf (created ? "created\n" : "error\n");
        }
      }
    }

    bool checkUserNameAndPassword (char *userName, char *password) { // scan through /etc/shadow file for (user name, pasword) pair and return true if found
      if (!__fileSystemMounted__) return false;
      
      // initial checking
      if (strlen (userName) > USER_PASSWORD_MAX_LENGTH || strlen (password) > USER_PASSWORD_MAX_LENGTH) return false;   
      
      char srchStr [USER_PASSWORD_MAX_LENGTH + 65 + 6];
      sprintf (srchStr, "%s:$5$", userName); sha256 (srchStr + strlen (srchStr), 65, password); strcat (srchStr, ":"); // we get something like "root:$5$de362bbdf11f2df30d12f318eeed87b8ee9e0c69c8ba61ed9a57695bbd91e481:"
      File f = fileSystem.open ("/etc/shadow", FILE_READ); 
      if (f) { 
        char line [USER_PASSWORD_MAX_LENGTH + 65 + 6]; // this should do, we don't even need the whole line which looks something like "root:$5$de362bbdf11f2df30d12f318eeed87b8ee9e0c69c8ba61ed9a57695bbd91e481:::::::"
        int i = 0;
        while (f.available ()) {
          char c = f.read ();
          #ifdef __PERFMON__
            __perfFSBytesWritten__ += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
          #endif            
          if (c >= ' ' && i < sizeof (line) - 1) line [i++] = c; // append line 
          else {
            line [i] = 0; i = 0; // terminate the line string
            i = 0; // the next time start from beginning of the line
            if (strstr (line, srchStr) == line) { // the begginning of line matches with user name and sha of password
              f.close ();
              return true; // success
            }
          }
        }
        f.close ();
      }
      return false;
    } 

    bool getUserHomeDirectory (char *buffer, size_t bufferSize, char *userName) { // always ends with /
      if (!__fileSystemMounted__) return false;
      
      *buffer = 0;
      char srchStr [USER_PASSWORD_MAX_LENGTH + 2]; 
      sprintf (srchStr, "%s:", userName);
      File f = fileSystem.open ("/etc/passwd", FILE_READ); 
      if (f) { 
        char line [USER_PASSWORD_MAX_LENGTH + 32 + FILE_PATH_MAX_LENGTH + 1]; // this should do for one line which looks something like "webserver::100:::/var/www/html/:"
        int i = 0;
        while (f.available ()) {
          char c = f.read ();
          #ifdef __PERFMON__
            __perfFSBytesWritten__ += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
          #endif                      
          if (c >= ' ' && i < sizeof (line) - 1) line [i++] = c; 
          else {
            line [i] = 0; // terminate the line string
            i = 0; // the next time start from beginning of the line
            if (strstr (line, srchStr) == line) { // userName matches
              char *homeDirectory = strstr (line, ":::");
              if (homeDirectory) {
                char format [16]; sprintf (format, ":::%%%i[^:]", bufferSize - 1); // we get something like ":::%255[^:]"
                if (sscanf (homeDirectory, format, buffer) == 1) {
                  f.close ();
                  if (*buffer) { // make sure home directory ends with /
                    int l = strlen (buffer);
                    if (buffer [l - 1] == '/') { 
                      return true; // home directory ends with /
                    } else { 
                      if (l < USER_PASSWORD_MAX_LENGTH) { buffer [l] = '/'; buffer [l+1] = 0; return true; } // home directory now ends with /
                      else return false; // home direcotry doesn't end with / and there is not enough space to add it
                    } 
                  } else return false; // home directory is empty  
                }
              }
            }
          }
        }
        f.close ();
      }
      return false; // can't open /etc/passwd
    }    

    String getUserHomeDirectory (char *userName) { // always ends with /
      char homeDirectory [USER_PASSWORD_MAX_LENGTH + 1]; 
      if (getUserHomeDirectory (homeDirectory, sizeof (homeDirectory), userName)) return homeDirectory;
      return "";
    }     

    // bool passwd (userName, newPassword) assignes a new password for the user by writing it's sha value into /etc/shadow file, return success
    // __ignoreIfUserDoesntExist__ and __dontWriteNewPassword__ arguments are only used internaly by userDel function
    bool passwd (char *userName, char *newPassword, bool __ignoreIfUserDoesntExist__ = false, bool __dontWriteNewPassword__ = false) {
      if (!__fileSystemMounted__) return false;
      
      // initial checking
      if (strlen (newPassword) > USER_PASSWORD_MAX_LENGTH) return false;   
      
      // read /etc/shadow
      char buffer [MAX_ETC_SHADOW_SIZE + 3]; buffer [0] = '\n'; 
      int i = 1;
      File f = fileSystem.open ("/etc/shadow", FILE_READ); 
      if (f) { 
        while (f.available () && i <= MAX_ETC_SHADOW_SIZE) if ((buffer [i] = f.read ()) != '\r') i++; buffer [i++] = '\n'; buffer [i] = 0; // read the whole file in C string ignoring \r
        if (f.available ()) { f.close (); return false; } // /etc/shadow too large
        f.close ();
      } else return false; // can't read /etc/shadow
      // find user's record in the buffer
      char srchStr [USER_PASSWORD_MAX_LENGTH + 6];
      sprintf (srchStr, "\n%s:$5$", userName); // we get something like \nroot:$5$
      char *u = strstr (buffer, srchStr);
      if (!u && !__ignoreIfUserDoesntExist__) return false; // user not found in /etc/shadow

      // write all the buffer except user's record back to /etc/shadow and then add a new record for the user
      f = fileSystem.open ("/etc/shadow", FILE_WRITE);
      if (f) {
        char *lineBeginning = buffer + 1;
        char *lineEnd;
        while ((lineEnd = strstr (lineBeginning, "\n"))) {
          *lineEnd = 0;
          if (lineBeginning != u + 1 && lineEnd - lineBeginning > 13) { // skip user's record and empty (<= 13) records
            if (f.printf ("%s\r\n", lineBeginning) < strlen (lineBeginning) + 2) { 
              close (f); 
              return false; // can't write /etc/shadow - this is bad because we have already corrupted it :/
            } 
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (lineBeginning) + 2; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif                        
          }
          lineBeginning = lineEnd + 1;
        }
        if (!__dontWriteNewPassword__) {
          // add user's record
          strcpy (buffer, srchStr + 1);
          sha256 (buffer + strlen (buffer), sizeof (buffer) - strlen (buffer), newPassword);
          if (f.printf ("%s:::::::\r\n", buffer) < strlen ("%s:::::::\r\n")) { 
            close (f); 
            return false; // can't write /etc/shadow - this is bad because we have already corrupted it :/
          }    
        }     
        f.close (); 
      } else return false; // can't write /etc/shadow
      return true; // success      
    }

    char *userDel (char *userName);
    
    // char *userAdd (userName, userId, userHomeDirectory) adds userName, userId, userHomeDirectory to /etc/passwd and /etc/shadow, creates home directory and returns success or error message
    // __ignoreIfUserExists__ and __dontWriteNewUser__ arguments are only used internaly by userDel function
    char *userAdd (char *userName, char *userId, char *userHomeDirectory, bool __ignoreIfUserExists__ = false, bool __dontWriteNewUser__ = false) {
      if (!__fileSystemMounted__) return (char *) "File system not mounted.";
      
      // initial checking
      if (strlen (userName) < 1)                             return (char *) "Missing user name.";
      if (strlen (userName) > USER_PASSWORD_MAX_LENGTH)      return (char *) "User name too long.";   
      if (strstr (userName, ":"))                            return (char *) "User name may not contain ':' character.";
      if (atoi (userId) <= 0)                                return (char *) "User id should be > 0."; // this information is not really used, 0 is reserverd for root in case of further development
      if (strlen (userHomeDirectory) < 1)                    return (char *) "Missing user's home directory.";
      if (strlen (userHomeDirectory) > FILE_PATH_MAX_LENGTH) return (char *) "User's home directory name too long.";    
      char homeDirectory [FILE_PATH_MAX_LENGTH + 2]; strcpy (homeDirectory, userHomeDirectory); if (homeDirectory [(strlen (homeDirectory) - 1)] != '/') strcat (homeDirectory, "/");
      if (strlen (homeDirectory) > FILE_PATH_MAX_LENGTH)     return (char *) "User's home directory name too long.";    

      // read /etc/passwd
      char buffer [MAX_ETC_PASSWD_SIZE + 3]; buffer [0] = '\n'; 
      int i = 1;
      File f = fileSystem.open ("/etc/passwd", FILE_READ); 
      if (f) { 
        while (f.available () && i <= MAX_ETC_PASSWD_SIZE) if ((buffer [i] = f.read ()) != '\r') i++; buffer [i++] = '\n'; buffer [i] = 0; // read the whole file in C string ignoring \r
        if (f.available ()) { f.close (); return (char *) "/etc/shadow is too large."; } 
        f.close ();
      } else return (char *) "can't read /etc/passwd."; 
      // find user's record in the buffer
      char srchStr [USER_PASSWORD_MAX_LENGTH + 3];
      sprintf (srchStr, "\n%s:", userName); // we get something like \nroot:
      char *u = strstr (buffer, srchStr);
      if (u && !__ignoreIfUserExists__) return (char *) "User with this name already exists.";
      if (!__dontWriteNewUser__ && strlen (buffer) + strlen (userName) + strlen (userId) + strlen (homeDirectory) + 10 > MAX_ETC_PASSWD_SIZE) return (char *) "Can't add a user because /etc/passwd file is already too long.";

      // write all the buffer back to /etc/passwd and then add a new record for the new user
      f = fileSystem.open ("/etc/passwd", FILE_WRITE);
      if (f) {
        char *lineBeginning = buffer + 1;
        char *lineEnd;
        while ((lineEnd = strstr (lineBeginning, "\n"))) {
          *lineEnd = 0;
          if (lineBeginning != u + 1 && lineEnd - lineBeginning > 2) { // skip skip user's record and empty (<= 2) records
            if (f.printf ("%s\r\n", lineBeginning) < strlen (lineBeginning) + 2) { 
              close (f); 
              return (char *) "Can't write /etc/passwd."; // this is bad because we have already corrupted it :/
            } 
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (lineBeginning) + 2; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif                        
          }
          lineBeginning = lineEnd + 1;
        }
        if (!__dontWriteNewUser__) {
          // add user's record
          strcpy (buffer, srchStr + 1);
          if (f.printf ("%s:x:%s:::%s:\r\n", buffer, userId, homeDirectory) < strlen ("%s:x:%s:::%s:\r\n")) { 
            close (f); 
            return (char *) "Can't write /etc/passwd."; // this is bad because we have already corrupted it :/
          }
        }         
        f.close (); 
      } else return (char *) "Can't write /etc/passwd."; 

      if (__dontWriteNewUser__) return (char *) "User deleted.";
      
      // write password in /etc/shadow file
      if (!passwd (userName, (char *) DEFAULT_USER_PASSWORD, true)) {
        userDel (userName); // try to roll-back the changes we made so far 
        return (char *) "Can't write /etc/shadow.";
      } 

      // crate user's home directory
      bool b = false;
      for (int i = 0; homeDirectory [i] && homeDirectory [i]; i++) if (i && homeDirectory [i] == '/') { 
        homeDirectory [i] = 0; b = fileSystem.mkdir (homeDirectory); homeDirectory [i] = '/'; 
      }
      if (!b) return (char *) "User created with default password '" DEFAULT_USER_PASSWORD "' but couldn't create user's home directory.";

      return (char *) "User created with default password '" DEFAULT_USER_PASSWORD "'.";
    }

    // char *userDel (userName) deletes userName from /etc/passwd and /etc/shadow, deletes home directory and returns success or error message
    char *userDel (char *userName) {
      if (!__fileSystemMounted__) return (char *) "File system not mounted.";
      
      // delete user's password from /etc/shadow file
      if (!passwd (userName, (char *) "", true, true)) return (char *) "Can't write /etc/shadow.";

      // get use's home directory from /etc/passwd
      char homeDirectory [FILE_PATH_MAX_LENGTH + 1];
      bool gotHomeDirectory = getUserHomeDirectory (homeDirectory, sizeof (homeDirectory), userName);
      // delete user from /etc/passwd
      if (strcmp (userAdd (userName, (char *) "32767", (char *) "$", true, true), "User deleted.")) return (char *) "Can't write /etc/passwd.";

      // remove user's home directory
      bool homeDirectoryRemoved = false; 
      if (gotHomeDirectory) {
        bool firstDirectory = true;
        for (int i = strlen (homeDirectory); i; i--) if (homeDirectory [i] == '/') { 
          homeDirectory [i] = 0; 
          if (!fileSystem.rmdir (homeDirectory)) break;
          if (firstDirectory) homeDirectoryRemoved = true;
          firstDirectory = false;
        }
        // if we've got home directory we can assume that the user existed prior to calling userDel
        if (homeDirectoryRemoved) return (char *) "User deleted.";
        else                      return (char *) "User deleted but couldn't remove user's home directory.";
      } else {
        // if we haven't got home directory we can assume that the user did not exist prior to calling userDel, at least not regurally, but we have corrected /etc/passwd and /etc/shadow now
        return (char *) "User does not exist.";
      }
    }

  #endif
  
#endif
