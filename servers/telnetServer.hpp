/*

    telnetServer.hpp

    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino

    Telnet server can handle some commands itself (for example uname, uptime) but the calling program can also provide its own 
    telnetCommandHandlerCallback function to handle some commands itself. A simple telnet client is also implemented as one of the built-in
    telnet commands but it doesn't expose an applicaton program interface.
  
    March 12, 2024, Bojan Jurca

    Nomenclature used here for easier understaning of the code:

     - "connection" normally applies to TCP connection from when it was established to when it is terminated

                  There is normally only one TCP connection per telnet session. These terms are pretty much interchangeable when we are talking about telnet.

     - "session" applies to user interaction between login and logout

                  The first thing that the user does when a TCP connection is established is logging in and the last thing is logging out. It TCP
                  connection drops for some reason the user is automatically logged out.

      - "buffer size" is the number of bytes that can be placed in a buffer. 
      
                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".

    Telnet protocol is basically a straightforward non synchronized transfer of text data in both directions, from client to server and vice versa.
    This implementation also adds processing command lines by detecting the end-of-line, parsing command line to arguments and then executing
    some already built-in commands and sending replies back to the client. In addition the calling program can provide its own command handler to handle
    some commands itself. Special telnet commands, so called IAC ("Interpret As Command") commands are placed within text stream and should be
    extracted from it and processed by both, the server and the client. All IAC commands start with character 255 (IAC character) and are followed
    by other characters. The number of characters following IAC characters varies from command to command. For example: the server may request
    the client to report its window size by sending IAC (255) DO (253) NAWS (31) and then the client replies with IAC (255) SB (250) NAWS (31) following
    with 2 bytes of window width and 2 bytes of window height and then IAC (255) SE (240).
    
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    // hard reset by triggering watchdog
    #include <esp_int_wdt.h>
    #include <esp_task_wdt.h>
    // fixed size strings
    #include "std/Cstring.hpp"
    #include "ESP32_ping.hpp"


#ifndef __TELNET_SERVER__
    #define __TELNET_SERVER__

    #ifdef SHOW_COMPILE_TIME_INFORMATION
        #ifndef __FILE_SYSTEM__
            #pragma message "Compiling telnetServer.hpp without file system (fileSystem.hpp), some commands, like ls, vi, pwd, ... will not be available"  
        #endif
        #ifndef __TIME_FUNCTIONS__
            #pragma message "Compiling telnetServer.hpp without time functions (time_functions.h), some commands, like ntpdate, crontab, ... will not be available"  
        #endif
        #ifndef __NETWORK__
            #pragma message "Compiling telnetServer.hpp without network functions (network.h), some commands, like ifconfig, arp, ... will not be available"  
        #endif
        #ifndef __HTTP_CLIENT__
            #pragma message "Compiling telnetServer.hpp without HTTP client (httpClient.h), curl command will not be available"  
        #endif
        #ifndef __SMTP_CLIENT__
            #pragma message "Compiling telnetServer.hpp without SMTP client (smtpClient.h), sendmail command will not be available"  
        #endif
        #ifndef __FTP_CLIENT__ 
            #pragma message "Compiling telnetServer.hpp without FTP client (ftpClient.h), ftpput and ftpget commands will not be available"  
        #endif
    #endif

    // TUNNING PARAMETERS

    #define TELNET_CONNECTION_STACK_SIZE 10 * 1024              // each TCP connection
    // #define TELNET_SERVER_CORE 1 // 1 or 0                   // #define TELNET_SERVER_CORE if you want telnetServer to run on specific core
    #define TELNET_CMDLINE_BUFFER_SIZE 128                      // reading and temporary keeping telnet command lines
    #define TELNET_CONNECTION_TIME_OUT 300000                   // 300000 ms = 5 min, 0 for infinite
    #define TELNET_SESSION_MAX_ARGC 24                          // max number of arguments in command line

    #define telnetServiceUnavailable "Telnet service is currently unavailable.\r\n"

    #ifndef HOSTNAME
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "HOSTNAME was not defined previously, #defining the default MyESP32Server in telnetServer.hpp"
        #endif
        #define "MyESP32Server"                                 // use default if not defined previously
    #endif
    #ifndef MACHINETYPE
        // replace MACHINETYPE with your information if you want, it is only used in uname telnet command
        #if CONFIG_IDF_TARGET_ESP32
            #define MACHINETYPE                   "ESP32"   
        #elif CONFIG_IDF_TARGET_ESP32S2
            #define MACHINETYPE                   "ESP32-S2"    
        #elif CONFIG_IDF_TARGET_ESP32S3
            #define MACHINETYPE                   "ESP32-S3"
        #elif CONFIG_IDF_TARGET_ESP32C3
            #define MACHINETYPE                   "ESP32-C3"        
        #elif CONFIG_IDF_TARGET_ESP32C6
            #define MACHINETYPE                   "ESP32-C6"
        #elif CONFIG_IDF_TARGET_ESP32H2
            #define MACHINETYPE                   "ESP32-H2"
        #else
            #define MACHINETYPE                   "ESP32 (other)"
        #endif
    #endif

    #include "std/vector.hpp"                               // for vi and tree commands only

    #include "version_of_servers.h"                         // version of this software, used in uname command
    #define UNAME string (MACHINETYPE " (") + string ((int) ESP.getCpuFreqMHz ()) + " MHz) " HOSTNAME " SDK: " + ESP.getSdkVersion () + " " VERSION_OF_SERVERS " compiled at: " __DATE__ " " __TIME__  + " with C++: " + string ((unsigned int) __cplusplus) // + " IDF:" + String (ESP_IDF_VERSION_MAJOR) + "." + String (ESP_IDF_VERSION_MINOR) + "." + String (ESP_IDF_VERSION_PATCH)


    // ----- CODE -----

    #ifndef __USER_MANAGEMENT__
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Implicitly including userManagement.hpp (needed for login on to Telnet server)"
        #endif
      #include "userManagement.hpp"      // for logging in
    #endif


    // define some IAC telnet commands
    #define __IAC__               0Xff   // 255 as number
    #define IAC                   "\xff" // 255 as string
    #define __DONT__              0xfe   // 254 as number
    #define DONT                  "\xfe" // 254 as string
    #define __DO__                0xfd   // 253 as number
    #define DO                    "\xfd" // 253 as string
    #define __WONT__              0xfc   // 252 as number
    #define WONT                  "\xfc" // 252 as string
    #define __WILL__              0xfb   // 251 as number
    #define WILL                  "\xfb" // 251 as string
    #define __SB__                0xfa   // 250 as number
    #define SB                    "\xfa" // 250 as string
    #define __SE__                0xf0   // 240 as number
    #define SE                    "\xf0" // 240 as string
    #define __LINEMODE__          0x22   //  34 as number
    #define LINEMODE              "\x22" //  34 as string
    #define __NAWS__              0x1f   //  31 as number
    #define NAWS                  "\x1f" //  31 as string
    #define __SUPPRESS_GO_AHEAD__ 0x03   //   3 as number
    #define SUPPRESS_GO_AHEAD     "\x03" //   3 as string
    #define __ECHO__              0x01   //   1 as number
    #define ECHO                  "\x01" //   1 as string 

    // control telnetServer critical sections
    static SemaphoreHandle_t __telnetServerSemaphore__ = xSemaphoreCreateMutex (); 

    // log what is going on within telnetServer
    byte telnetServerConcurentTasks = 0;
    byte telnetServerConcurentTasksHighWatermark = 0;


    // ----- telnetConnection class -----

    class telnetConnection {

      public:

        // telnetConnection state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          RUNNING = 2        
        };

        STATE_TYPE state () { return __state__; }

        telnetConnection ( // the following parameters will be handeled by telnetConnection instance
                         int connectionSocket,
                         String (* telnetCommandHandlerCallback) (int argc, char *argv [], telnetConnection *tcn), // telnetCommandHandler callback function provided by calling program
                         char *clientIP, char *serverIP
                       )  {
                            // create a local copy of parameters for later use
                            __connectionSocket__ = connectionSocket;
                            __telnetCommandHandlerCallback__ = telnetCommandHandlerCallback;
                            strncpy (__clientIP__, clientIP, sizeof (__clientIP__)); __clientIP__ [sizeof (__clientIP__) - 1] = 0; // copy client's IP since connection may live longer than the server that created it
                            strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0; // copy server's IP since connection may live longer than the server that created it
                            // handle connection in its own thread (task)       
                            #define tskNORMAL_PRIORITY 1
                            #ifdef TELNET_SERVER_CORE
                                BaseType_t taskCreated = xTaskCreatePinnedToCore (__connectionTask__, "telnetConnection", TELNET_CONNECTION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL, TELNET_SERVER_CORE);
                            #else
                                BaseType_t taskCreated = xTaskCreate (__connectionTask__, "telnetConnection", TELNET_CONNECTION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL);
                            #endif
                            if (pdPASS != taskCreated) {
                                #ifdef __DMESG__
                                    dmesgQueue << "[telnetConnection] xTaskCreate error";
                                #endif
                            } else {
                                __state__ = RUNNING;

                                xSemaphoreTake (__telnetServerSemaphore__, portMAX_DELAY);
                                    telnetServerConcurentTasks++;
                                    if (telnetServerConcurentTasks > telnetServerConcurentTasksHighWatermark)
                                        telnetServerConcurentTasksHighWatermark = telnetServerConcurentTasks;
                                xSemaphoreGive (__telnetServerSemaphore__);

                                return; // success

                            }
                            #ifdef __DMESG__
                                dmesgQueue << "[telnetConnection] xTaskCreate error";
                            #endif
                          }

        ~telnetConnection ()  {
                                  xSemaphoreTake (__telnetServerSemaphore__, portMAX_DELAY);
                                      telnetServerConcurentTasks--;
                                  xSemaphoreGive (__telnetServerSemaphore__);

                                  closeConnection ();
                              }

        int getSocket () { return __connectionSocket__; }

        char *getClientIP () { return __clientIP__; }

        char *getServerIP () { return __serverIP__; }

        bool connectionTimeOut () { return __telnet_connection_time_out__ && millis () - __lastActive__ >= __telnet_connection_time_out__; }
        
        void closeConnection () {
                                  int connectionSocket;
                                  xSemaphoreTake (__telnetServerSemaphore__, portMAX_DELAY);
                                    connectionSocket = __connectionSocket__;
                                    __connectionSocket__ = -1;
                                  xSemaphoreGive (__telnetServerSemaphore__);
                                  if (connectionSocket > -1) close (connectionSocket);
                                }

        // telnet session related variables
        char *getUserName () { return __userName__; }
        #ifdef __FILE_SYSTEM__
          char *getHomeDir () { return __homeDir__; }
          char *getWorkingDir () { return __workingDir__; }
        #endif
        uint16_t getClientWindowWidth () { return __clientWindowWidth__; }
        uint16_t getClientWindowHeight () { return __clientWindowHeight__; }

        // writing output to telnet with error logging (dmesg)
        int sendTelnet (const char *buf, size_t len) { 
            int i = sendAll (__connectionSocket__, buf, len, __telnet_connection_time_out__);
            if (i <= 0) {
                #ifdef __DMESG__
                    dmesgQueue << "[telnetConnection] send error: " << errno << " " << strerror (errno);
                #endif
            }
            return i;
        }

        int sendTelnet (const char *buf) { return sendTelnet (buf, strlen (buf)); }


        // reading input from Telnet client with extraction of IAC commands
        char recvTelnetChar (bool onlyPeek = false) { // returns valid character or 0 in case of error, extracts IAC commands from stream
          // if there is a character that has been just peeked previously return it now
          if (__peekedOnlyChar__) { char c = __peekedOnlyChar__; __peekedOnlyChar__ = 0; return c; }
          // if we are just peeking check the socket
          if (onlyPeek) {
            char c; if (recv (__connectionSocket__, &c, sizeof (c), MSG_PEEK) == -1) return 0; // meaning that no character is pending to be read
          }
          // read the next character from the socket
        tryReadingCharAgain:
          // if time-out occured return 0 
          if (__telnet_connection_time_out__ && millis () - __lastActive__ >= __telnet_connection_time_out__) return 0; // time-out

          char charRead = 0;
          char c [2];
          if (recvAll (__connectionSocket__, c, sizeof (c), NULL, __telnet_connection_time_out__) <= 0) return 0;
          
          switch (c [0]) {
              case 3:       charRead = 3; break;    // Ctrl-C
              case 4:                               // Ctrl-D
              case 26:      charRead = 4; break;    // Ctrl-Z
              case 8:       charRead = 8; break;    // BkSpace
              case 127:     charRead = 127; break;  // Del
              case 9:       charRead = 9; break;    // Tab
              case 10:      charRead = 10; break;   // Enter
              case __IAC__: 
                              // read the next character
                              if (recvAll (__connectionSocket__, c, sizeof (c), NULL, __telnet_connection_time_out__) <= 0) return 0;
                              switch (c [0]) {
                                case __SB__: 
                                              // read the next character
                                              if (recvAll (__connectionSocket__, c, sizeof (c), NULL, __telnet_connection_time_out__) <= 0) return 0;
                                              if (c [0] == __NAWS__) { 
                                                // read the next 4 bytes indication client window size
                                                char c [5];
                                                if (recvAll (__connectionSocket__, c, sizeof (c), NULL, __telnet_connection_time_out__) < 4) return 0;
                                                __clientWindowWidth__ = (uint16_t) c [0] << 8 | (uint8_t) c [1]; 
                                                __clientWindowHeight__ = (uint16_t) c [2] << 8 | (uint8_t) c [3];
                                              }
                                              break;
                                // in the following cases the 3rd character is following, ignore this one too
                                case __WILL__:
                                case __WONT__:
                                case __DO__:
                                case __DONT__:
                                                if (recvAll (__connectionSocket__, c, sizeof (c), NULL, __telnet_connection_time_out__) <= 0) return 0;
                                default:        // ignore
                                                break;
                              } // switch level 2, after IAC
                              break;              
              default:      
                            __lastActive__ = millis (); // reset connection level countdown after sucessful read
                            if ((c [0] >= 19) && (c [0] < 240)) charRead = c [0]; // Ctrl-S <= c < SE, try to ignore control characters
                            break; // ignore other characters
          } // switch level 1

          if (onlyPeek) {
            if (charRead) { // if there is a regular character that we have just read
              __peekedOnlyChar__ = charRead;
            } else { // no regular characer read (jet), check if there is something more waiting
              if (recv (__connectionSocket__, &charRead, sizeof (charRead), MSG_PEEK) == -1) return 0; // meaning that no character is pending to be read
              else goto tryReadingCharAgain;
            }
          } else {
            if (!charRead) goto tryReadingCharAgain;
          }
          
          return charRead;
        }

        // reads incoming stream, returns last character read or 0-error, 3-Ctrl-C, 4-EOF, 10-Enter, ...
        char recvTelnetLine (char *buf, size_t len, bool trim = true) {
          if (!len) return 0;
          int characters = 0;
          buf [0] = 0;

          while (true) { // read and process incomming data in a loop 
            char c = recvTelnetChar ();
            switch (c) {
                case 0:   return 0; // Error
                case 3:   return 3; // Ctrl-C
                case 4:             // EOF - Ctrl-D (UNIX)
                case 26:  return 4; // EOF - Ctrl-Z (Windows)
                case 8:   // backspace - delete last character from the buffer and from the screen
                case 127: // in Windows telent.exe this is del key, but putty reports backspace with code 127, so let's treat it the same way as backspace
                          if (characters && buf [characters - 1] >= ' ') {
                            buf [-- characters] = 0;
                            if (__echo__ && sendTelnet ("\x08 \x08") <= 0) return 0; // delete the last character from the screen
                          }
                          break;                            
                case 10:  // LF
                          if (trim) {
                            while (buf [0] == ' ') strcpy (buf, buf + 1); // ltrim
                            int i; for (i = 0; buf [i] > ' '; i++); buf [i] = 0; // rtrim
                          }
                          if (__echo__ && sendTelnet ("\r\n") <= 0) return 0; // echo CRLF to the screen
                          return 10;
                case 13:  // CR
                          break; // ignore 
                case 9:   // tab - treat as 2 spaces
                          c = ' ';
                          if (characters < len - 1) {
                            buf [characters] = c; buf [++ characters] = 0;
                            if (__echo__ && sendTelnet (&c, sizeof (c)) <= 0) return 0; // echo last character to the screen
                          }                
                          // continue with default (repeat adding a space):
                default:  // fill the buffer 
                          if (characters < len - 1) {
                            buf [characters] = c; buf [++ characters] = 0;
                            if (__echo__ && sendTelnet (&c, sizeof (c)) <= 0) return 0; // echo last character to the screen
                          }
                          break;              
            } // switch
          } // while
          return 0;
        }
        
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        unsigned long __lastActive__ = millis (); // time-out detection        

        unsigned long __telnet_connection_time_out__ = TELNET_CONNECTION_TIME_OUT;
      
        int __connectionSocket__ = -1;
        String (*__telnetCommandHandlerCallback__) (int argc, char *argv [], telnetConnection *tcn) = NULL;
        char __clientIP__ [46] = "";
        char __serverIP__ [46] = "";

        char __peekedOnlyChar__ = 0;
        char __cmdLine__ [TELNET_CMDLINE_BUFFER_SIZE]; // characters cleared of IAC commands, 1 line at a time

        // telnet session related variables
        char __userName__ [USER_PASSWORD_MAX_LENGTH + 1] = "";
        #ifdef __FILE_SYSTEM__
          char __homeDir__ [FILE_PATH_MAX_LENGTH + 1] = "";
          char __workingDir__ [FILE_PATH_MAX_LENGTH + 1] = "";
        #endif
        char __prompt__ = 0;
        uint16_t __clientWindowWidth__ = 0;
        uint16_t __clientWindowHeight__ = 0;

        bool __echo__ = true;
        
        static void __connectionTask__ (void *pvParameters) {
          // get "this" pointer
          telnetConnection *ths = (telnetConnection *) pvParameters;           
          { // code block
            { // login procedure
              #if USER_MANAGEMENT == NO_USER_MANAGEMENT
                // prepare session defaults
                strcpy (ths->__userName__, "root");
                #ifdef __FILE_SYSTEM__
                    userManagement.getHomeDirectory (ths->__homeDir__, sizeof (ths->__homeDir__), ths->__userName__);
                    strcpy (ths->__workingDir__, ths->__homeDir__);
                #endif
                ths->__prompt__ = '#';
                #ifdef __DMESG__
                    dmesgQueue << "[telnetConnection] user logged in: " << ths->__userName__;
                #endif
                // tell the client to go into character mode, not to echo and send its window size, then say hello 
                sprintf (ths->__cmdLine__, IAC WILL ECHO IAC WILL SUPPRESS_GO_AHEAD IAC DO NAWS HOSTNAME " says hello to %s.\r\nWelcome %s, use \"help\" to display available commands.\r\n\n", ths->__clientIP__, ths->__userName__);
                if (ths->sendTelnet (ths->__cmdLine__) <= 0) goto endOfConnection;
                *ths->__cmdLine__ = 0;
              #else
                // tell the client to go into character mode, not to echo an send its window size, then say hello 
                sprintf (ths->__cmdLine__, IAC WILL ECHO IAC WILL SUPPRESS_GO_AHEAD IAC DO NAWS HOSTNAME " says hello to %s, please login.\r\nuser: ", ths->__clientIP__);
                if (ths->sendTelnet (ths->__cmdLine__) <= 0) {
                  #ifdef __DMESG__
                      dmesgQueue << "[telnetConnection] send error: " << errno << strerror (errno);
                  #endif
                  goto endOfConnection;
                }
                if (ths->recvTelnetLine (ths->__userName__, sizeof (ths->__userName__)) != 10) { 
                  goto endOfConnection;                
                }
                // if (!ths->__userName__ [0]) endOfConnection;
                char password [USER_PASSWORD_MAX_LENGTH + 1] = "";
                ths->__echo__ = false;
                if (ths->sendTelnet ("password: ") <= 0) goto endOfConnection;
                if (ths->recvTelnetLine (password, sizeof (password)) != 10) { 
                  goto endOfConnection;                
                }
                // if (!ths->__password__ [0]) endOfConnection;
                ths->__echo__ = true;
                // check user name and password
                if (!userManagement.checkUserNameAndPassword (ths->__userName__, password)) { 
                  #ifdef __DMESG__
                      dmesgQueue << "[telnetConnection] user failed login attempt: " << ths->__userName__;
                  #endif
                  delay (100);
                  ths->sendTelnet ("\r\nUsername and/or password incorrect.");
                  goto endOfConnection;                                
                }
                #ifdef __DMESG__
                    dmesgQueue << "[telnetConnection] user logged in: " << ths->__userName__;
                #endif
                // prepare session defaults
                #ifdef __FILE_SYSTEM__
                    userManagement.getHomeDirectory (ths->__homeDir__, sizeof (ths->__homeDir__), ths->__userName__);
                    strcpy (ths->__workingDir__, ths->__homeDir__);
                #endif
                ths->__prompt__ = strcmp (ths->__userName__, "root") ? '$' : '#';
                sprintf (ths->__cmdLine__, "\r\nWelcome %s, use \"help\" to display available commands.\r\n\n", ths->__userName__);
                if (ths->sendTelnet (ths->__cmdLine__) <= 0) goto endOfConnection;
                *ths->__cmdLine__ = 0;
              #endif
            } // login procedure
            // this is where telnet session really starts
            while (true) { // endless loop of reading and executing commands
              
                // write prompt and possibly \r\n
                sprintf (ths->__cmdLine__ + strlen (ths->__cmdLine__), "%c ", ths->__prompt__);
                if (ths->sendTelnet (ths->__cmdLine__) <= 0) goto endOfConnection;
                // read command line
                switch (ths->recvTelnetLine (ths->__cmdLine__, sizeof (ths->__cmdLine__), false)) {
                  case 3:   { // Ctrl-C, end the connection
                              ths->sendTelnet ("\r\nCtrl-C");
                              goto endOfConnection;
                            }
                  case 10:  { // Enter, parse command line

                              int argc = 0;
                              char *argv [TELNET_SESSION_MAX_ARGC];
  
                              char *q = ths->__cmdLine__ - 1;
                              while (true) {
                                char *p = q + 1; while (*p && *p <= ' ') p++;
                                if (*p) { // p points to the beginning of an argument
                                  bool quotation = false;
                                  if (*p == '\"') { quotation = true; p++; }
                                  argv [argc++] = p;
                                  q = p; 
                                  while (*q && (*q > ' ' || quotation)) if (*q == '\"') break; else q++; // q points after the end of an argument 
                                  if (*q) *q = 0; else break;
                                } else break;
                                if (argc == TELNET_SESSION_MAX_ARGC) break;
                              }

                              // process commandLine
                              if (argc) {
                                // ask telnetCommandHandler (if it is provided by the calling program) if it is going to handle this command, otherwise try to handle it internally
                                String s;
                                if (ths->__telnetCommandHandlerCallback__) s = ths->__telnetCommandHandlerCallback__ (argc, argv, ths);
                                if (!s) { // out of memory                               
                                  if (ths->sendTelnet ("Out of memory") <= 0) goto endOfConnection;
                                } else if (s != "") { // __telnetCommandHandlerCallback__ returned a reply                
                                  if (ths->sendTelnet (s.c_str ()) <= 0) goto endOfConnection;
                                } else { // // __telnetCommandHandlerCallback__ returned "" - handle the command internally
                                  string s = ths->internalTelnetCommandHandler (argc, argv, ths);
                                  if (ths->__connectionSocket__ == -1) goto endOfConnection; // in case of quit - quit command closes the socket itself
                                  if (s != "") if (ths->sendTelnet (s) <= 0) goto endOfConnection;                               
                                }
                                sprintf (ths->__cmdLine__, "\r\n"); // will be sent to client together with promtp sign  
                              } else {
                                *ths->__cmdLine__ = 0; // prepare buffer for the next user input                           
                              }
                              log_i ("stack high-water mark: %lu\n", uxTaskGetStackHighWaterMark (NULL));
                              break;
                            }
                  case 0:   {
                              if (errno == EAGAIN || errno == ENAVAIL) ths->sendTelnet ("\r\nTime-out");
                              goto endOfConnection;                
                            }
                  default:  // ignore
                            break;
                }
            
            } // endless loop of reading and executing commands

          } // code block
        endOfConnection:  
          #ifdef __DMESG__
              if (ths->__prompt__) dmesgQueue << "[telnetConnection] user logged out: " << ths->__userName__; // if prompt is set, we know that login was successful
          #endif
          // all variables are freed now, unload the instance and stop the task (in this order)
          delete ths;
          vTaskDelete (NULL);                
        }

        string internalTelnetCommandHandler (int argc, char *argv [], telnetConnection *tcn) {

          #define argv0Is(X) (argc > 0 && !strcmp (argv [0], X))
          #define argv1Is(X) (argc > 1 && !strcmp (argv [1], X))

               if (argv0Is ("help"))      { return argc == 1 ? __help__ (tcn) : "Wrong syntax, use help"; }
                                          
          else if (argv0Is ("clear"))     { return argc == 1 ? __clear__ () : "Wrong syntax, use clear"; }
                                          
          else if (argv0Is ("uname"))     { return argc == 1 ? __uname__ () : "Wrong syntax, use uname"; }

          else if (argv0Is ("free"))      { 
                                              if (argc == 1)                                        return __free__ (0, tcn);
                                              if (argc == 2) {
                                                  int n = atoi (argv [1]); if (n > 0 && n <= 3600)  return __free__ (n, tcn);
                                              }
                                              return "Wrong syntax, use free [<n>]   (where 0 < n <= 3600)";
                                          } 

          else if (argv0Is ("netstat"))   {
                                              if (argc == 1)                                        return __netstat__ (0, tcn);
                                              if (argc == 2) {
                                                  int n = atoi (argv [1]); if (n > 0 && n < 3600)   return __netstat__ (n, tcn);
                                              }
                                              return "Wrong syntax, use netstat [<n>]   (where 0 < n <= 3600)";
                                          }

          else if (argv0Is ("kill"))      { 
                                              if (!strcmp (__userName__, "root")) {
                                                if (argc == 2) {
                                                    int n = atoi (argv [1]); if (n >= LWIP_SOCKET_OFFSET && n < LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN) return __kill__ (n);
                                                }
                                                return string ("Wrong syntax, use kill <socket>   (where ") + string (LWIP_SOCKET_OFFSET) + " <= socket <= " + string (LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN - 1) + ")";
                                              }
                                              else return "Only root may close sockets.";
                                          }

          else if (argv0Is ("nohup"))     {
                                              if (argc == 1)                                    return __nohup__ (0);
                                              if (argc == 2) {
                                                int n = atoi (argv [1]); if (n > 0 && n < 3600) return __nohup__ (n);
                                              }
                                              return "Wrong syntax, use nohup [<n>]   (where 0 < n <= 3600)";
                                          }
                                        
          else if (argv0Is ("reboot"))    {
                                              if (argc == 1)                            return __reboot__ (true);
                                              if (argv1Is ("-h") || argv1Is ("-hard"))  return __reboot__ (false);
                                              return "Wrong syntax, use reboot [-hard]"; 
                                          }
                                      
          else if (argv0Is ("quit"))      { return argc == 1 ? __quit__ () : "Wrong syntax, use quit"; }

          else if (argv0Is ("date"))      {
                                              if (argc == 1)                                              return __getDateTime__ ();
                                                  if (argc == 4 && (argv1Is ("-s") || argv1Is ("-set")))  return __setDateTime__ (argv [2], argv [3]);
                                              return "Wrong syntax, use date [-set YYYY/MM/DD hh:mm:ss] (use hh in 24 hours time format)";
                                          }

        #ifdef __TIME_FUNCTIONS__

            else if (argv0Is ("uptime"))    { return argc == 1 ? __uptime__ () : "Wrong syntax, use uptime"; }

            else if (argv0Is ("ntpdate"))   {
                                                if (argc == 1) return __ntpdate__ ();
                                                if (argc == 2) return __ntpdate__ (argv [1]);
                                                return "Wrong syntax, use ntpdate [ntpServer]";
                                            }

            else if (argv0Is ("crontab"))   { return argc == 1 ? __cronTab__ (tcn) : "Wrong syntax, use crontab"; }

        #endif

        else if (argv0Is ("ping"))      { 
                                            if (argc == 2) return __ping__ (tcn, argv [1]);
                                            return "Wrong syntax, use ping <target computer>"; 
                                        }

        #ifdef __NETWORK__

            else if (argv0Is ("ifconfig"))  {
                                                  if (argc == 1) return ifconfig (__connectionSocket__);
                                                  return "Wrong syntax, use arp";              
                                            }

            else if (argv0Is ("arp"))       { return argc == 1 ? arp (__connectionSocket__) : "Wrong syntax, use arp"; }

            else if (argv0Is ("iw"))        { return argc == 1 ? iw (__connectionSocket__) : "Wrong syntax, use iw"; }

        #endif

        #ifdef __DMESG__

          else if (argv0Is ("dmesg"))     { 
                                              bool f = false;
                                              bool t = false;
                                              for (int i = 1; i < argc; i++) {
                                                      if (!strcmp (argv [i], "-f") || !strcmp (argv [i], "-follow")) f = true;
                                                  else if (!strcmp (argv [i], "-t") || !strcmp (argv [i], "-time")) t = true;
                                                  else return "Wrong syntax, use dmesg [-follow] [-time]";
                                              }
                                              return __dmesg__ (f, t, tcn);
                                          }
                                      
        #endif

          else if (argv0Is ("telnet"))    { 
                                              if (argc == 2)                                return __telnet__ (argv [1], 23, tcn);
                                              if (argc == 3) {
                                                  int port = atoi (argv [2]); if (port > 0) return __telnet__ (argv [1], port, tcn);
                                              }
                                              return "Wrong syntax, use telnet <server> [port]";
                                          } 

        #ifdef __HTTP_CLIENT__

          else if (argv0Is ("curl"))      { 
                                              if (argc == 2) return __curl__ ("GET", argv [1], tcn);
                                              if (argc == 3) return __curl__ (argv [1], argv [2], tcn);
                                              return "Wrong syntax, use curl [method] http://url";
                                          } 
                                      
        #endif

        #ifdef __SMTP_CLIENT__

          else if (argv0Is ("sendmail"))  { 
                                              char *smtpServer = (char *) "";
                                              char *smtpPort = (char *) "";
                                              char *userName = (char *) "";
                                              char *password = (char *) "";
                                              char *from = (char *) "";
                                              char *to = (char *) ""; 
                                              char *subject = (char *) ""; 
                                              char *message = (char *) "";
                                            
                                              for (int i = 1; i < argc - 1; i += 2) {
                                                      if (!strcmp (argv [i], "-S")) smtpServer = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-P")) smtpPort = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-u")) userName = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-p")) password = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-f")) from = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-t")) to = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-s")) subject = argv [i + 1];
                                                  else if (!strcmp (argv [i], "-m")) message = argv [i + 1];
                                                  else return "Wrong syntax, use sendmail [-S smtpServer] [-P smtpPort] [-u userName] [-p password] [-f from address] [t to address list] [-s subject] [-m messsage]";
                                              }

                                              return sendMail (message, subject, to, from, password, userName, atoi (smtpPort), smtpServer);
                                          }
  
        #endif

        #ifdef __FTP_CLIENT__ 

          else if (argv0Is ("ftpput"))    { 
                                              char *ftpServer = (char *) "";
                                              char *ftpPort = (char *) "";
                                              char *userName = (char *) "";
                                              char *password = (char *) "";
                                              char *localFileName = NULL;
                                              char *remoteFileName = NULL;
                                              bool syntaxError = false;
                                            
                                              for (int i = 1; i < argc && !syntaxError; i++) {
                                                       if (!strcmp (argv [i], "-S")) { if (i < argc - 1) ftpServer = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-P")) { if (i < argc - 1) ftpPort   = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-u")) { if (i < argc - 1) userName  = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-p")) { if (i < argc - 1) password  = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (*argv [i] != '-')  {
                                                                                  if (!localFileName) localFileName = argv [i];
                                                                                  else if (!remoteFileName) remoteFileName = argv [i];
                                                                                  else syntaxError = true;
                                                                              }
                                                  else syntaxError = true;
                                              }
                                              if (!remoteFileName) remoteFileName = localFileName;
                                              if (!localFileName) syntaxError = true;
                                              if (syntaxError) return "Wrong syntax, use ftpput [-S ftpServer] [-P ftpPort] [-u userName] [-p password] <localFileName> <remoteFileName>";
                                              return __ftpPut__ (localFileName, remoteFileName, password, userName, atoi (ftpPort), ftpServer);
                                          }

          else if (argv0Is ("ftpget"))    { 
                                              char *ftpServer = (char *) "";
                                              char *ftpPort = (char *) "";
                                              char *userName = (char *) "";
                                              char *password = (char *) "";
                                              char *localFileName = NULL;
                                              char *remoteFileName = NULL;
                                              bool syntaxError = false;
                                            
                                              for (int i = 1; i < argc && !syntaxError; i++) {
                                                      if (!strcmp (argv [i], "-S")) { if (i < argc - 1) ftpServer = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-P")) { if (i < argc - 1) ftpPort   = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-u")) { if (i < argc - 1) userName  = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (!strcmp (argv [i], "-p")) { if (i < argc - 1) password  = argv [i++ + 1]; else syntaxError = true; }
                                                  else if (*argv [i] != '-')  {
                                                                                  if (!localFileName) localFileName = argv [i];
                                                                                  else if (!remoteFileName) remoteFileName = argv [i];
                                                                                  else syntaxError = true;
                                                                              }
                                                  else syntaxError = true;
                                              }
                                              if (!remoteFileName) remoteFileName = localFileName;
                                              if (!localFileName) syntaxError = true;
                                              if (syntaxError) return "Wrong syntax, use ftpget [-S ftpServer] [-P ftpPort] [-u userName] [-p password] <localFileName> <remoteFileName>";
                                              return __ftpGet__ (localFileName, remoteFileName, password, userName, atoi (ftpPort), ftpServer);
                                          }
        #endif

        #ifdef __FILE_SYSTEM__        
      
          #if FILE_SYSTEM == FILE_SYSTEM_FAT

            else if (argv0Is ("mkfs.fat")) { 
                                              if (argc == 1) {
                                                  if (!strcmp (__userName__, "root")) return __mkfs__ (tcn);
                                                  else                                return "Only root may format disk.";
                                              }                                       return "Wrong syntax, use mkfs.fat";
                                          } 
          
          #endif
          #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS

            else if (argv0Is ("mkfs.littlefs"))  { 
                                              if (argc == 1) {
                                                  if (!strcmp (__userName__, "root")) return __mkfs__ (tcn);
                                                  else                                return "Only root may format disk.";
                                              }                                       return "Wrong syntax, use mkfs.littlefs";
                                          } 
          
          #endif

          else if (argv0Is ("fs_info"))   { return argc == 1 ? __fs_info__ () : "Wrong syntax, use fs_info"; }

          else if (argv0Is ("diskstat"))  {
                                              if (argc == 1)                                        return __diskstat__ (0, tcn);
                                              if (argc == 2) {
                                                  int n = atoi (argv [1]); if (n > 0 && n < 3600)   return __diskstat__ (n, tcn);
                                              }
                                              return "Wrong syntax, use diskstat [<n>]   (where 0 < n <= 3600)";
                                          }          

          else if (argv0Is ("ls"))        {
                                              if (argc == 1)  return __ls__ (__workingDir__, tcn);
                                              if (argc == 2)  return __ls__ (argv [1], tcn);
                                                              return "Wrong syntax, use ls [<directoryName>]";
                                          }

          else if (argv0Is ("tree"))      {
                                              if (argc == 1)  return __tree__ (__workingDir__, tcn); 
                                              if (argc == 2)  return __tree__ (argv [1], tcn); 
                                                              return "Wrong syntax, use tree [<directoryName>]";
                                          }

          else if (argv0Is ("cat"))       {
                                              if (argc == 2)                  return __catFileToClient__ (argv [1], tcn);
                                              if (argc == 3 && argv1Is (">")) return __catClientToFile__ (argv [2], tcn);
                                                                              return "Wrong syntax, use cat [>] <fileName>";
                                          }

          else if (argv0Is ("tail"))      {
                                              if (argc == 2)                                            return __tail__ (argv [1], false, tcn);
                                              if (argc == 3 && (argv1Is ("-f") || argv1Is ("-follow"))) return __tail__ (argv [2], true, tcn);
                                                                                                        return "Wrong syntax, use tail [-follow] <fileName>";
                                          }
                                          
          else if (argv0Is ("vi"))        {  
                                              if (argc == 2)  {
                                                  unsigned long telnet_connection_time_out = __telnet_connection_time_out__;
                                                  __telnet_connection_time_out__ = 0; // infinite
                                                  string s = __vi__ (argv [1], tcn);
                                                  __telnet_connection_time_out__ = telnet_connection_time_out;
                                                  return s;
                                              }
                                              return "Wrong syntax, use vi <fileName>";
                                          }

          else if (argv0Is ("mkdir"))     { return argc == 2 ? __mkdir__ (argv [1]) : "Wrong syntax, use mkdir <directoryName>"; } 

          else if (argv0Is ("rmdir"))     { return argc == 2 ? __rmdir__ (argv [1]) : "Wrong syntax, use rmdir <directoryName>"; } 

          else if (argv0Is ("cd"))        { return argc == 2 ? __cd__ (argv [1])      : "Wrong syntax, use cd <directoryName>"; }
          else if (argv0Is ("cd.."))      { return argc == 1 ? __cd__ ("..") : "Wrong syntax, use cd <directoryName>"; } 

          else if (argv0Is ("pwd"))       { return argc == 1 ? __pwd__ () : "Wrong syntax, use pwd"; }

          else if (argv0Is ("mv"))        { return argc == 3 ? __mv__ (argv [1], argv [2]) : "Wrong syntax, use mv <existing fileName or directoryName> <new fileName or directoryName>"; }
                                           
          else if (argv0Is ("cp"))        { return argc == 3 ? __cp__ (argv [1], argv [2]) : "Wrong syntax, use cp <existing fileName> <new fileName>"; }

          else if (argv0Is ("rm"))        { return  argc == 2 ? __rm__ (argv [1]) : "Wrong syntax, use rm <fileName>"; }
                                          
        #endif

        #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT
  
            else if (argv0Is ("useradd")) { 
                                              if (strcmp (__userName__, "root"))                                      return "Only root may add users.";
                                              if (argc == 6 && !strcmp (argv [1], "-u") && !strcmp (argv [3], "-d"))  return userManagement.userAdd (argv [5], argv [2], argv [4]);
                                                                                                                      return "Wrong syntax, use useradd -u userId -d userHomeDirectory userName (where userId > 1000)";
                                          }
                                          
            else if (argv0Is ("userdel")) { 
                                              if (strcmp (__userName__, "root"))  return "Only root may delete users.";
                                              if (argc != 2)                      return "Wrong syntax. Use userdel userName";
                                              if (!strcmp (argv [1], "root"))     return "You don't really want to to this.";
                                                                                  return userManagement.userDel (argv [1]);
                                          }
  
          else if (argv0Is ("passwd"))    { 
                                              if (argc == 1)                                                             return __passwd__ (__userName__);
                                              if (argc == 2) {
                                                if (!strcmp (__userName__, "root") || !strcmp (argv [1], __userName__))  return __passwd__ (argv [1]); 
                                                else                                                                     return "You may change only your own password.";
                                              }
                                              return "";
                                          }
                                                      
        #endif
                                      
            else return "Invalid command, use \"help\" to display available commands.";
        }

        const char *__help__ (telnetConnection *tcn) {
          char *h = (char *)  "Suported commands:"
                              "\r\n      help"
                              "\r\n      clear"
                              "\r\n      uname"
                              "\r\n      free [<n>]    (where 0 < n <= 3600)"
                              "\r\n      nohup [<n>]   (where 0 < n <= 3600)"
                              "\r\n      reboot [-h]   (-h for hard reset)"
                              "\r\n      quit"
                              #ifdef __DMESG__
                                "\r\n  dmesg circular queue:"
                                "\r\n      dmesg [-follow] [-time]"
                              #endif
                              "\r\n  time commands:"
                              #ifdef __TIME_FUNCTIONS__                                
                                "\r\n      uptime"
                              #endif
                              "\r\n      date [-set <YYYY/MM/DD hh:mm:ss>]"
                              #ifdef __TIME_FUNCTIONS__                                
                                "\r\n      ntpdate [<ntpServer>]"
                                "\r\n      crontab"
                              #endif
                                "\r\n  network commands:"
                                "\r\n      ping <terget computer>"
                              #ifdef __NETWORK__
                                "\r\n      ifconfig"
                                "\r\n      arp"
                                "\r\n      iw"
                                "\r\n      netstat [<n>]   (where 0 < n <= 3600)"
                                "\r\n      kill <socket>   (where socket is a valid socket)"
                              #endif
                                "\r\n      telnet <server> [port]"
                              #ifdef __HTTP_CLIENT__
                                "\r\n      curl [method] http://url"
                              #endif
                              #ifdef __FTP_CLIENT__ 
                                "\r\n      ftpput [-S ftpServer] [-P ftpPort] [-u userName] [-p password] <localFileName> <remoteFileName>"
                                "\r\n      ftpget [-S ftpServer] [-P ftpPort] [-u userName] [-p password] <localFileName> <remoteFileName>"
                              #endif
                              #ifdef __SMTP_CLIENT__
                                "\r\n      sendmail [-S smtpServer] [-P smtpPort] [-u userName] [-p password] [-f from address] [t to address list] [-s subject] [-m messsage]"
                              #endif
                              #ifdef __FILE_SYSTEM__
                                "\r\n  file commands:"
                                #if FILE_SYSTEM == FILE_SYSTEM_FAT
                                "\r\n      mkfs.fat"
                                #endif
                                #if FILE_SYSTEM == FILE_SYSTEM_LITTLEFS
                                "\r\n      mkfs.littlefs"
                                #endif
                                "\r\n      fs_info"
                                "\r\n      ls [<directoryName>]"
                                "\r\n      tree [<directoryName>]"
                                "\r\n      mkdir <directoryName>"
                                "\r\n      rmdir <directoryName>"
                                "\r\n      cd <directoryName or ..>"
                                "\r\n      pwd"
                                "\r\n      cat [>] <fileName>"
                                "\r\n      vi <fileName>"
                                "\r\n      cp <existing fileName> <new fileName>"
                                "\r\n      mv <existing fileName or directoryName> <new fileName or directoryName>"
                                "\r\n      rm <fileName>"
                                "\r\n      diskstat [<n>]   (where 0 < n <= 3600)"
                              #endif
                              #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT
                                "\r\n  user management:"                                        
                                "\r\n       useradd -u <userId> -d <userHomeDirectory> <userName>"
                                "\r\n       userdel <userName>"
                                "\r\n       passwd [<userName>]"
                              #endif
                              #ifdef __FILE_SYSTEM__
                                "\r\n  configuration files used by system:"
                                #ifdef __TIME_FUNCTIONS__
                                  "\r\n      /usr/share/zoneinfo                       (contains (POSIX) timezone information)"
                                  "\r\n      /etc/ntp.conf                             (contains NTP time servers names)"
                                  "\r\n      /etc/crontab                              (contains scheduled tasks)"
                                #endif
                                #ifdef __NETWORK__
                                  "\r\n      /network/interfaces                       (contains STA(tion) configuration)"
                                  "\r\n      /etc/wpa_supplicant/wpa_supplicant.conf   (contains STA(tion) credentials)"
                                  "\r\n      /etc/dhcpcd.conf                          (contains A(ccess) P(oint) configuration)"
                                  "\r\n      /etc/hostapd/hostapd.conf                 (contains A(ccess) P(oint) credentials)"
                                #endif
                                #ifdef __FTP_CLIENT__
                                  "\r\n      /etc/ftp/ftpclient.cf                     (contains ftpput and ftpget default settings)"
                                #endif
                                #ifdef __SMTP_CLIENT__
                                  "\r\n      /etc/mail/sendmail.cf                     (contains sendMail default settings)"
                                #endif
                                #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT
                                  "\r\n      /etc/passwd                               (contains users' accounts information)"
                                  "\r\n      /etc/shadow                               (contains users' passwords)"
                                #endif
                              #endif
                              ;
            sendTelnet (h);
            return "";                                            
        }

        const char *__clear__ () { return "\x1b[2J"; } // ESC[2J = clear screen

        string __uname__ () { return UNAME; }

        const char *__free__ (unsigned long delaySeconds, telnetConnection *tcn) {
            bool firstTime = true;
            int currentLine = 0;
            char s [128];
            do { // follow         
                if (firstTime || (tcn->getClientWindowHeight ()  && currentLine >= tcn->getClientWindowHeight ())) {
                    sprintf (s, "%sFree heap       Max block Free PSRAM\r\n-------------------------------------------", firstTime ? "" : "\r\n");
                    if (sendTelnet (s) <= 0) return "";
                    currentLine = 2; // we have just displayed 2 lines (header)
                }
                if (!firstTime && delaySeconds) { // wait and check if user pressed a key
                    unsigned long startMillis = millis ();
                    while (millis () - startMillis < (delaySeconds * 1000)) {
                        delay (100);
                        if (tcn->recvTelnetChar (true)) {
                            tcn->recvTelnetChar (false); // read pending character
                            return ""; // return if user pressed Ctrl-C or any key
                        } 
                        if (tcn->connectionTimeOut ()) { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; }
                    }
                }
                sprintf (s, "\r\n%10lu   %10lu   %10lu  bytes", (unsigned long) ESP.getFreeHeap (), (unsigned long) heap_caps_get_largest_free_block (MALLOC_CAP_DEFAULT), (unsigned long) ESP.getFreePsram ());
                if (sendTelnet (s) <= 0) return "";
                firstTime = false;
                currentLine ++; // we have just displayed the next line (data)
            } while (delaySeconds);
            return "";
        }

        const char *__netstat__ (unsigned long delaySeconds, telnetConnection *tcn) {
            // variables for delta calculation
            additionalNetworkInformation_t lastAdditionalNetworkInformationn = {}; 
            additionalNetworkInformation_t lastAdditionalSocketInformation [MEMP_NUM_NETCONN] = {}; 

            char s [256];

            do {

                // clear screen
                if (delaySeconds) if (sendTelnet ("\x1b[2J") <= 0) return "";

                // display totals
                sprintf (s, "total bytes received and sent:                       %10lu      %10lu\r\n", additionalNetworkInformation.bytesReceived - lastAdditionalNetworkInformationn.bytesReceived, additionalNetworkInformation.bytesSent - lastAdditionalNetworkInformationn.bytesSent);

                // update variables for delta calculation
                lastAdditionalNetworkInformationn = additionalNetworkInformation;

                if (sendTelnet (s) <= 0) return "";

                // display header
                sprintf (s, "\r\n"
                            "socket  local address        remote address      bytes received      bytes sent      inactive\r\n"
                            "------------------------------------------------------------------------------------------------------------------------");
                if (sendTelnet (s) <= 0) return "";

                // scan through sockets
                for (int i = LWIP_SOCKET_OFFSET; i < LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN; i++) { // socket numbers are only within this range
                    char socketIP [46]; struct sockaddr_in socketAddress = {}; socklen_t len = sizeof (socketAddress);
                    // get server side address first
                    if (getsockname (i, (struct sockaddr *) &socketAddress, &len) != -1) {
                        inet_ntoa_r (socketAddress.sin_addr, socketIP, sizeof (socketIP));
                        sprintf (s, "\r\n  %2i    %s:%i                                                                       ", i, socketIP, ntohs (socketAddress.sin_port));
                        // get client side address next
                        if (getpeername (i, (struct sockaddr *) &socketAddress, &len) != -1) {
                            inet_ntoa_r (socketAddress.sin_addr, socketIP, sizeof (socketIP));
                            if (additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived < lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived || additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent < lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent) 
                                lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET] = {0, 0};

                            sprintf (s + 31, "%s:%i               ", socketIP, ntohs (socketAddress.sin_port));
                            sprintf (s + 57, "%8lu      %10lu      %6lu s", additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived, additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent, (millis () - additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastActiveMillis) / 1000);

                            switch (additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastUsedAs) {
                                case __TELNET_SERVER_SOCKET__:  strcat (s, i == tcn->getSocket () ? " Telnet server (this conn.)" : " Telnet server");
                                                                break;
                                case __TELNET_CLIENT_SOCKET__:  strcat (s, " Telnet client");
                                                                break;
                                case __HTTP_SERVER_SOCKET__:    strcat (s, " HTTP server");
                                                                break;
                                case __HTTP_CLIENT_SOCKET__:    strcat (s, " HTTP client");
                                                                break;
                                case __FTP_SERVER_SOCKET__:     strcat (s, " FTP server ctrl. conn.");
                                                                break;
                                case __FTP_CLIENT_SOCKET__:     strcat (s, " FTP client ctrl. conn.");
                                                                break;
                                case __FTP_DATA_SOCKET__:       strcat (s, " FTP data conn.");
                                                                break;
                                case __SMTP_CLIENT_SOCKET__:    strcat (s, " SMTP client");
                                                                break;
                            }
                            
                        } else { // socket without a peer address - it must be a listening socket

                            switch (additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastUsedAs) {
                                case __LISTENING_SOCKET__:      sprintf (s + 87, "%6lu s Listening socket", (millis () - additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastActiveMillis) / 1000);                                
                                                                break;
                                case __NTP_SOCKET__:            sprintf (s + 57, "%8lu      %10lu      %6lu s NTP client", additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived, additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent, (millis () - additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastActiveMillis) / 1000);
                                                                break;
                                case __PING_SOCKET__:           sprintf (s + 57, "%8lu      %10lu      %6lu s Ping client", additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived, additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent - lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent, (millis () - additionalSocketInformation [i - LWIP_SOCKET_OFFSET].lastActiveMillis) / 1000);
                                                                break;
                            }
                            
                        }

                        // update variables for delta calculation
                        lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived = additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesReceived;
                        lastAdditionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent = additionalSocketInformation [i - LWIP_SOCKET_OFFSET].bytesSent;

                        if (sendTelnet (s) <= 0) return "";
                    }
                }

                // wait for a key press
                unsigned long startMillis = millis ();
                while (millis () - startMillis < (delaySeconds * 1000)) {
                    delay (100);
                    if (tcn->recvTelnetChar (true)) {
                        tcn->recvTelnetChar (false); // read pending character
                        return ""; // return if user pressed Ctrl-C or any key
                    } 
                    if (tcn->connectionTimeOut ()) { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; }
                }

            } while (delaySeconds);
            return "";
        }

        string __kill__ (int i) { // i = socket number
          if (close (i) < 0) {
            #ifdef __DMESG__
                dmesgQueue << "[telnet] close() error: " << errno << " " << strerror (errno);
            #endif
            return string ("Error: ") + string (errno) + " " + strerror (errno);
          }
          return "socked closed.";
        }

        string __nohup__ (unsigned long timeOutSeconds) {
            __telnet_connection_time_out__ = timeOutSeconds * 1000;
            return timeOutSeconds ? string ("Time-out set to ") + string (timeOutSeconds) + " seconds." : "Time-out set to infinite.";
        }

        const char *__reboot__ (bool softReboot) {
            sendTelnet ("Rebooting ...");
            delay (250);
            if (softReboot) {
                ESP.restart ();
            } else {
                // cause WDT reset
                esp_task_wdt_init (1, true);
                esp_task_wdt_add (NULL);
                while (true);
            }
            return "";
        }
  
        const char *__quit__ () {
            closeConnection ();
            return "";  
        }
  
        string __getDateTime__ () {
            time_t t = time (NULL);
            if (t < 1687461154) return "The time has not been set yet."; // 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this
            struct tm st;
            localtime_r (&t, &st);
            string s;
            sprintf (s.c_str (), "%04i/%02i/%02i %02i:%02i:%02i", 1900 + st.tm_year, 1 + st.tm_mon, st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec);
            return s;
        }
    
        string __setDateTime__ (char *dt, char *tm) {
            int Y, M, D, h, m, s;
            if (sscanf (dt, "%i/%i/%i", &Y, &M, &D) == 3 && Y >= 1900 && M >= 1 && M <= 12 && D >= 1 && D <= 31 && sscanf (tm, "%i:%i:%i", &h, &m, &s) == 3 && h >= 0 && h <= 23 && m >= 0 && m <= 59 && s >= 0 && s <= 59) { // TO DO: we still do not catch all possible errors, for exmple 30.2.1966
                struct tm tm;
                tm.tm_year = Y - 1900; tm.tm_mon = M - 1; tm.tm_mday = D;
                tm.tm_hour = h; tm.tm_min = m; tm.tm_sec = s;
                time_t t = mktime (&tm); // time in local time
                if (t != -1) {
                    #ifdef __TIME_FUNCTIONS__
                        setTimeOfDay (t);
                    #else
                        // repeat most of the things what setTimeOfDay does (except setting __startupTime__) here
                        struct timeval newTime = {t, 0};
                        settimeofday (&newTime, NULL); 
                    #endif
                    return __getDateTime__ ();          
                }
            }
            return "Wrong format of date/time specified.";
        }         

      #ifdef __TIME_FUNCTIONS__

        string __uptime__ () {
            string s;
            char c [15];
            time_t t = time ();
            time_t uptime;
            if (t) { // if time is set
                struct tm strTime = localTime (t);
                sprintf (c, "%02i:%02i:%02i", strTime.tm_hour, strTime.tm_min, strTime.tm_sec);
                s = string (c) + " up ";     
            } else { // time is not set (yet), just read how far clock has come untill now
                s = "Up ";
            }
            uptime = getUptime ();
            int seconds = uptime % 60; uptime /= 60;
            int minutes = uptime % 60; uptime /= 60;
            int hours = uptime % 24;   uptime /= 24; // uptime now holds days
            if (uptime) s += string ((unsigned long) uptime) + " days, ";
            sprintf (c, "%02i:%02i:%02i", hours, minutes, seconds);
            s += c;
            return s;
        }

        string __ntpdate__ (char *ntpServer = NULL) {
            string r;
            if (ntpServer) {
                char *p = ntpDate (ntpServer);
                if (*p) return p; // ntpDate reported an error
            } else {
                char *p = ntpDate ();
                if (*p) return p; // ntpDate reported an error
            }
            ascTime (localTime (time (NULL)), r.c_str ());
            return string ("Time synchronized, currrent time is ") + r + ".";
        }

        const char *__cronTab__ (telnetConnection *tcn) {
            xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
                if (!__cronTabEntries__) {
                    sendTelnet ("crontab is empty.");
                } else {
                    for (int i = 0; i < __cronTabEntries__; i ++) {
                        string s;
                        char c [30];
                        if (__cronEntry__ [i].second == ANY)      s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].second);      s += c; }
                        if (__cronEntry__ [i].minute == ANY)      s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].minute);      s += c; }
                        if (__cronEntry__ [i].hour == ANY)        s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].hour);        s += c; }
                        if (__cronEntry__ [i].day == ANY)         s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].day);         s += c; }
                        if (__cronEntry__ [i].month == ANY)       s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].month);       s += c; }
                        if (__cronEntry__ [i].day_of_week == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].day_of_week); s += c; }
                        if (__cronEntry__ [i].lastExecuted) {
                                                                  ascTime (localTime (__cronEntry__ [i].lastExecuted), c);
                                                                  s += " "; s += c; s += " "; 
                                                            } else { 
                                                                  s += " (not executed yet)  "; 
                                                            }
                        if (__cronEntry__ [i].readFromFile)       s += " from /etc/crontab  "; else s += " entered from code  ";
                        s += __cronEntry__ [i].cronCommand;
                                                                  s += "\r\n";
                        if (sendTelnet (s.c_str ()) <= 0) break;
                    }
                }
            xSemaphoreGiveRecursive (__cronSemaphore__);
            return "";
        }

      #endif

          const char *__ping__ (telnetConnection *tcn, const char *pingTarget) {

              // overload onReceive member function to get notified about intermediate results
              class telnet_ping : public esp32_ping {
                  public:
                      telnetConnection *tcn;

                      void onStart () {
                          // remember some information that netstat telnet command would use
                          additionalSocketInformation [tcn->getSocket () - LWIP_SOCKET_OFFSET] = { __PING_SOCKET__, 0, 0, millis (), millis () };
                      }

                      void onSend (int bytes) {
                          // remember some information that netstat telnet command would use
                          additionalNetworkInformation.bytesSent += bytes;
                          additionalSocketInformation [tcn->getSocket () - LWIP_SOCKET_OFFSET].bytesSent += bytes;
                          additionalSocketInformation [tcn->getSocket () - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
                      }

                      void onReceive (int bytes) {
                          // remember some information that netstat telnet command would use
                          additionalNetworkInformation.bytesReceived += bytes;
                          additionalSocketInformation [tcn->getSocket () - LWIP_SOCKET_OFFSET].bytesReceived += bytes;
                          additionalSocketInformation [tcn->getSocket () - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();

                          // display intermediate results
                          char buf [100];
                          if (elapsed_time ())
                              sprintf (buf, "\r\nReply from %s: bytes = %i time = %.3fms", target ().toString().c_str(), size (), elapsed_time ());
                          else
                              sprintf (buf, "\r\nReply from %s: time-out\n", target ().toString().c_str());
                          if (tcn->sendTelnet (buf) <= 0) stop ();
                          return;
                      }

                      void onWait () {
                          // stop waiting if a key is pressed
                          if (tcn->recvTelnetChar (true)) {
                              tcn->recvTelnetChar (false); // read pending character
                              stop ();
                          } 
                          return;
                      }
              };

              telnet_ping tping;
              tping.tcn = tcn;
              tping.ping (pingTarget, PING_DEFAULT_COUNT, PING_DEFAULT_INTERVAL, PING_DEFAULT_SIZE, PING_DEFAULT_TIMEOUT);
              char buf [200];
              if (tping.error () != ERR_OK) {
                  sprintf (buf, "%i\n", tping.error ());
              } else {
                  sprintf (buf, "Ping statistics for %s:\r\n"
                                    "    Packets: Sent = %i, Received = %i, Lost = %i", tping.target ().toString ().c_str (), tping.sent (), tping.received (), tping.lost ());
                  if (tping.sent ()) {
                      sprintf (buf, " (%.2f%% loss)\r\nRound trip:\r\n"
                                    "   Min = %.3fms, Max = %.3fms, Avg = %.3fms, Stdev = %.3fms", (float) tping.lost () / (float) tping.sent () * 100, tping.min_time (), tping.max_time (), tping.mean_time (), sqrt (tping.var_time () / tping.received ()));
                  }
              }
              tcn->sendTelnet (buf);
              return "\r\n";
          }

      #ifdef __DMESG__
  
          const char * __dmesg__ (bool follow, bool trueTime, telnetConnection *tcn) {
              #ifndef __TIME_FUNCTIONS__
                  if (trueTime) return "-time option is not supported (time_functions.h is not included)";
              #endif

              string s;
              bool firstRecord = true;
              dmesgQueueEntry_t *lastBack = NULL;
              for (auto e: dmesgQueue) { // scan dmesgQueue with iterator where the locking mechanism is already implemented

                  if (firstRecord) firstRecord = false; else s = "\r\n";
                  char c [25];
                  if (trueTime && e.time > 1687461154) { // 2023/06/22 21:12:34, any valid time should be greater than this
                      struct tm slt; 
                      localtime_r (&e.time, &slt); 
                      strftime (c, sizeof (c), "[%y/%m/%d %H:%M:%S] ", &slt);
                  } else {
                      sprintf (c, "[%10lu] ", e.milliseconds);
                  }
                  s += c;
                  s += e.message;
                  lastBack = dmesgQueue.back (); // remember the last change in case -f is specified
                  if (sendTelnet (s) <= 0) break;
              
              }

            // -follow?
            while (follow) {
                delay (100);

                // stop waiting if a key is pressed
                if (tcn->recvTelnetChar (true)) {
                    tcn->recvTelnetChar (false); // read pending character
                    return ""; // return if user pressed Ctrl-C or any key
                  } 
                if (tcn->connectionTimeOut ()) { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; }

                // read the new dmsg entries if they appeared
                if (lastBack != dmesgQueue.back ()) { // changed
                    auto e = dmesgQueue.begin (lastBack);
                    ++ e; // skip one message - it has already been displayed
                    while (e != dmesgQueue.end ()) {
                      
                        s = "\r\n";
                        char c [25];

                        if (trueTime && (*e).time > 1687461154) { // 2023/06/22 21:12:34, any valid time should be greater than this
                            struct tm slt; 
                            localtime_r (&(*e).time, &slt); 
                            strftime (c, sizeof (c), "[%y/%m/%d %H:%M:%S] ", &slt);
                        } else {
                            sprintf (c, "[%10lu] ", (*e).milliseconds);
                        }
                        s += c;
                        s += (*e).message;
                        lastBack = dmesgQueue.back (); // remember the last change
                        if (sendTelnet (s) <= 0) break;

                        ++ e;
                    }
                }                  
            }

            return "";
        }      

    #endif

      struct __telnetSharedMemory__ {
          int socketTowardsServer;
          int socketTowardsClient;
          unsigned long time_out;
          bool threadTowardsServerFinished;
          bool threadTowardsClientFinished;
          bool clientError;
          bool clientTimeOut;
        };
        
        string __telnet__ (char *server, int port, telnetConnection *tcn) {
          // get server address
          struct hostent *he = gethostbyname (server);
          if (!he) return string ("gethostbyname() error: ") + string (h_errno) + " " + hstrerror (h_errno);
          // create socket
          int connectionSocket = socket (PF_INET, SOCK_STREAM, 0);
          if (connectionSocket == -1) return string ("socket() error: ") + string (errno) + " " + strerror (errno);

          // remember some information that netstat telnet command would use
          additionalSocketInformation [connectionSocket - LWIP_SOCKET_OFFSET] = { __TELNET_CLIENT_SOCKET__, 0, 0, millis (), millis () };
          
          // make the socket not-blocking so that time-out can be detected
          if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
              close (connectionSocket);
              return string ("fcntl() error: ") + string (errno) + " "  + strerror (errno);
          }
          // connect to server
          struct sockaddr_in serverAddress;
          serverAddress.sin_family = AF_INET;
          serverAddress.sin_port = htons (port);
          serverAddress.sin_addr.s_addr = *(in_addr_t *) he->h_addr; 
          if (connect (connectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
              if (errno != EINPROGRESS) {
                  close (connectionSocket);
                  return string ("connect() error: ") + string (errno) + " "  + strerror (errno);
                }
            } // it is likely that socket is not connected yet at this point
          // send information about IP used to connect to server back to client
          {
              char serverIP [14 + 46 + 4] = "Connecting to ";
              inet_ntoa_r (serverAddress.sin_addr, serverIP + 14, 46);
              strcat (serverIP, " ...");
              if (sendTelnet (serverIP) <= 0) return "";
          }

          // we need 2 tasks, one for data transfer client -> ESP32 -> server and the other client <- ESP32 <- server
          // for client -> ESP32 -> server direction we are going to use current task  
          // for client <- ESP32 <- server direction we'll start a new one
          struct __telnetSharedMemory__ sharedMemory = {connectionSocket, __connectionSocket__, __telnet_connection_time_out__, false, false, false, false};
          #define tskNORMAL_PRIORITY 1


          #ifdef TELNET_SERVER_CORE
              BaseType_t taskCreated = xTaskCreatePinnedToCore ([] (void *param)  { // client <- ESP32 <- server data transfer
                                                                                    struct __telnetSharedMemory__ *sharedMemory = (struct __telnetSharedMemory__ *) param;
                                                                                    while (!sharedMemory->threadTowardsServerFinished) { // while the other task is running
                                                                                        char buffer [512];
                                                                                        int readTotal = recvAll (sharedMemory->socketTowardsServer, buffer, sizeof (buffer), NULL, sharedMemory->time_out);
                                                                                        if (readTotal <= 0) {
                                                                                            // error while reading data from server, we don't care if it is time-out or something else
                                                                                            break;
                                                                                        } else {
                                                                                            if (sendAll (sharedMemory->socketTowardsClient, buffer, readTotal, sharedMemory->time_out) <= 0) {
                                                                                                // error while writing data to client, we don't care if it is time-out or something else
                                                                                                #ifdef __DMESG__
                                                                                                    dmesgQueue << "[telnetConnection] send (to other telnet server) error: " << errno << " " << strerror (errno);
                                                                                                #endif
                                                                                                break;
                                                                                            }
                                                                                        }
                                                                                        delay (1);
                                                                                    }
                                                                                    sharedMemory->threadTowardsClientFinished = true; // signal the other task that this task has stopped
                                                                                    vTaskDelete (NULL);
                                                                                  }, 
                                                                __func__, 
                                                                4068, 
                                                                &sharedMemory,
                                                                tskNORMAL_PRIORITY,
                                                                NULL,
                                                                TELNET_SERVER_CORE);
          #else
              BaseType_t taskCreated = xTaskCreate ([] (void *param)  { // client <- ESP32 <- server data transfer
                                                                        struct __telnetSharedMemory__ *sharedMemory = (struct __telnetSharedMemory__ *) param;
                                                                        while (!sharedMemory->threadTowardsServerFinished) { // while the other task is running
                                                                            char buffer [512];
                                                                            int readTotal = recvAll (sharedMemory->socketTowardsServer, buffer, sizeof (buffer), NULL, sharedMemory->time_out);
                                                                            if (readTotal <= 0) {
                                                                                // error while reading data from server, we don't care if it is time-out or something else
                                                                                break;
                                                                            } else {
                                                                                if (sendAll (sharedMemory->socketTowardsClient, buffer, readTotal, sharedMemory->time_out) <= 0) {
                                                                                    // error while writing data to client, we don't care if it is time-out or something else
                                                                                    #ifdef __DMESG__
                                                                                        dmesgQueue << "[telnetConnection] send (to other telnet server) error: " << errno << " " << strerror (errno);
                                                                                    #endif
                                                                                    break;
                                                                                }
                                                                            }
                                                                            delay (1);
                                                                        }
                                                                        sharedMemory->threadTowardsClientFinished = true; // signal the other task that this task has stopped
                                                                        vTaskDelete (NULL);
                                                                      }, 
                                __func__, 
                                4068, 
                                &sharedMemory,
                                tskNORMAL_PRIORITY,
                                NULL);
          #endif
          if (pdPASS != taskCreated) {
            close (connectionSocket);
            return "xTaskCreate error";
          } 
                                                        { // client -> ESP32 -> server data transfer
                                                          while (!sharedMemory.threadTowardsClientFinished) { // while the other task is running
                                                              char buffer [512];
                                                              int readTotal = recvAll (sharedMemory.socketTowardsClient, buffer, sizeof (buffer), NULL, __telnet_connection_time_out__);
                                                              if (readTotal <= 0) {
                                                                  // error while reading data from client
                                                                  if (errno != EAGAIN && errno != ENAVAIL) sharedMemory.clientTimeOut = true;
                                                                  else                                     sharedMemory.clientError = true;
                                                              } else {
                                                                  if (sendAll (sharedMemory.socketTowardsServer, buffer, readTotal, __telnet_connection_time_out__) <= 0) {
                                                                      // error while writing data to server, we don't care if it is time-out or something else
                                                                      #ifdef __DMESG__
                                                                          dmesgQueue << "[telnetConnection] send (to other telnet server) error: " << errno << " " << strerror (errno);
                                                                      #endif
                                                                      break;
                                                                  }
                                                              }
                                                              delay (1);
                                                          }
                                                          sharedMemory.threadTowardsServerFinished = true; // signal the other task that this task has stopped
                                                          // wait the other task to finish
                                                          while (!sharedMemory.threadTowardsClientFinished) delay (1);
                                                        }

          close (connectionSocket);
          if (sharedMemory.clientTimeOut) { tcn->closeConnection (); return ""; }
          if (sharedMemory.clientError)   { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; }
          // tell the client to go into character mode, not to echo and send its window size, just in case the other server has changed that
          return IAC WILL ECHO IAC WILL SUPPRESS_GO_AHEAD IAC DO NAWS "\r\nConnection to host lost.";
      }

      #ifdef __HTTP_CLIENT__

        string __curl__ (const char *method, char *url, telnetConnection *tcn) {
            char server [65];
            char addr [128] = "/";
            int port = 80;
            if (sscanf (url, "http://%64[^:]:%i/%126s", server, &port, addr + 1) < 2) { // we haven't got at least server, port and the default address
                if (sscanf (url, "http://%64[^/]/%126s", server, addr + 1) < 1) { // we haven't got at least server and the default address  
                    return "Wrong url, use form of http://server/address or http://server:port/address";
                }
            }
            if (port <= 0) return "Wrong port number, it should be > 0";
            String r = httpClient (server, port, addr, method);
            if (!r) return "Out of memory";
            sendTelnet (r.c_str ());
            return "";            
        }

      #endif

      #ifdef __FILE_SYSTEM__        

        bool telnetUserHasRightToAccessFile (char *fullPath) { return strstr (fullPath, __homeDir__) == fullPath; }
        bool telnetUserHasRightToAccessDirectory (char *fullPath) { return telnetUserHasRightToAccessFile (string (fullPath) + "/"); }
        
        #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
            const char *__mkfs__ (telnetConnection *tcn) {
                if (sendTelnet ("formatting FAT file system, please wait ... ") <= 0) return ""; 
                fileSystem.unmount ();
                if (fileSystem.formatFAT ()) {
                                                          return "formatted.";
                    if (fileSystem.mountFAT (false))      return "\r\nFAT file system mounted,\r\nreboot now to create default configuration files\r\nor you can create them yorself before rebooting.";
                    else                                  return "formatted but failed to mount the file system."; 
                } else                                    return "failed.";
                return "";
            }
        #endif

        #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
            const char *__mkfs__ (telnetConnection *tcn) {
                if (sendTelnet ("formatting LittleFs file system, please wait ... ") <= 0) return ""; 
                fileSystem.unmount ();
                if (fileSystem.formatLittleFs ()) {
                                                          return "formatted.";
                    if (fileSystem.mountLittleFs (false)) return "\r\nLittleFs file system mounted,\r\nreboot now to create default configuration files\r\nor you can create them yorself before rebooting.";
                    else                                  return "formatted but failed to mount the file system."; 
                } else                                    return "failed.";
                return "";
            }
        #endif
  
        string __fs_info__ () {
            if (!fileSystem.mounted ()) return "File system not mounted. You may have to format flash disk first.";
            string s = ""; // string = fsString<350> so we have enough space
            
            #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
                sprintf (s, "FAT file system --------------------\r\n"
                            "Total space:      %10lu K bytes\r\n"
                            "Total space used: %10lu K bytes\r\n"
                            "Free space:       %10lu K bytes\r\n",
                            FFat.totalBytes () / 1024, 
                            FFat.usedBytes () / 1024, 
                            (FFat.totalBytes () - FFat.usedBytes ()) / 1024
                      );
            #endif

            #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
                sprintf (s, "LittleFS file system:\r\n"
                            "Total space:      %10i K bytes\r\n"
                            "Total space used: %10i K bytes\r\n"
                            "Free space:       %10i K bytes\r\n",
                            LittleFS.totalBytes () / 1024, 
                            LittleFS.usedBytes () / 1024, 
                            (LittleFS.totalBytes () - LittleFS.usedBytes ()) / 1024
                      );
            #endif

            #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
                // if (sendTelnet ((char *) s) <= 0) return ""; 

                const char *cardTypeText = "unknown";
                uint8_t cardType = SD.cardType ();
                switch (cardType) {
                    case CARD_NONE: cardTypeText = "no SD card attached"; break;
                    case CARD_MMC:  cardTypeText = "MMC"; break;
                    case CARD_SD:   cardTypeText = "SDSC"; break;
                    case CARD_SDHC: cardTypeText = "SDHC"; break;
                }

                sprintf (s + strlen (s), "SD card ----------------------------\r\n"
                                         "Type:   %20s\r\n", cardTypeText);
            #endif

            return s;
        } 

        const char *__diskstat__ (unsigned long delaySeconds, telnetConnection *tcn) {
            // variables for delta calculation
            diskTrafficInformationType lastDiskTrafficInformation = {}; 

            char s [256];

            // display header
            sprintf (s, "\r\n"
                        "bytes read   bytes written\r\n"
                        "--------------------------");
            if (sendTelnet (s) <= 0) return "";

            do {
                sprintf (s, "\r\n  %8lu        %8lu", diskTrafficInformation.bytesRead, diskTrafficInformation.bytesWritten);
                if (sendTelnet (s) <= 0) return "";

                // update variables for delta calculation
                lastDiskTrafficInformation = diskTrafficInformation;

                // wait for a key press
                unsigned long startMillis = millis ();
                while (millis () - startMillis < (delaySeconds * 1000)) {
                    delay (100);
                    if (tcn->recvTelnetChar (true)) {
                        tcn->recvTelnetChar (false); // read pending character
                        return ""; // return if user pressed Ctrl-C or any key
                    } 
                    if (tcn->connectionTimeOut ()) { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; }
                }

            } while (delaySeconds);
            return "";
        }        
  
        string __ls__ (char *directoryName, telnetConnection *tcn) {
            if (!fileSystem.mounted ())                     return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (directoryName, __workingDir__);
            if (fp == "" || !fileSystem.isDirectory (fp))   return "Invalid directory name.";
            if (!telnetUserHasRightToAccessDirectory (fp))  return "Access denyed.";

            // display information about files and subdirectories
            bool firstRecord = true;
            File d = fileSystem.open (fp); 
            if (!d) return "Out of resources";
            for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
                string fullFileName = fp;
                if (fullFileName [fullFileName.length () - 1] != '/') fullFileName += '/'; fullFileName += f.name ();
                if (sendTelnet (firstRecord ? fileSystem.fileInformation (fullFileName) : string ("\r\n") + fileSystem.fileInformation (fullFileName)) <= 0) { d.close (); return "Out of memory"; }
                firstRecord = false;
            }
            d.close ();
            return "";            
        }
  
        // iteratively scan directory tree, recursion takes too much stack
        const char *__tree__ (char *directoryName, telnetConnection *tcn) {
            if (!fileSystem.mounted ())                     return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (directoryName, __workingDir__);
            if (fp == "" || !fileSystem.isDirectory (fp))   return "Invalid directory name.";
            if (!telnetUserHasRightToAccessDirectory (fp))  return "Access denyed.";

            // keep directory names saved in vector for later recursion   
            vector<string> dirList (5); 
            if (dirList.push_back (fp) != OK) return "Out of memory";
            bool firstRecord = true;
            while (dirList.size () != 0) {

                // 1. take the first directory from dirList
                fp = dirList [0];
                dirList.erase (0);

                // 2. display directory info
                 if (sendTelnet (firstRecord ? fileSystem.fileInformation (fp, true) : string ("\r\n") + fileSystem.fileInformation (fp, true)) <= 0) return "Out of memory";
                 firstRecord = false;

                // 3. display information about files and remember subdirectories in dirList
                File d = fileSystem.open (fp); 
                if (!d) return "Out of resources";
                for (File f = d.openNextFile (); f; f = d.openNextFile ()) {
                    if (f.isDirectory ()) {
                        // save directory name for later recursion
                        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS // f.name contains only a file name without path
                            string dp = fp; if (dp [dp.length () - 1] != '/') dp += '/'; dp += f.name ();                          
                            if (dirList.push_back (dp) != OK) { d.close (); return "Out of memory"; }
                        #else                                                                                       // f.name contains full file path
                            if (dirList.push_back (f.name ()) != OK) { d.close (); return "Out of memory"; }
                        #endif
                    } else {
                        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) || FILE_SYSTEM == FILE_SYSTEM_LITTLEFS // f.name contains only a file name without path
                            string dp = fp; if (dp [dp.length () - 1] != '/') dp += '/'; dp += f.name ();
                            if (sendTelnet (string ("\r\n") + fileSystem.fileInformation (dp, true)) <= 0) { d.close (); return "Out of memory"; }
                        #else                                                                                         // f.name contains full file path
                            if (sendTelnet (string ("\r\n") + fileSystem.fileInformation (f.name (), true)) <= 0) { d.close (); return "Out of memory"; }
                        #endif                              
                    }
                }
                d.close ();
            }
            return "";    
        }

        string __catFileToClient__ (char *fileName, telnetConnection *tcn) {
            if (!fileSystem.mounted ())               return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (fileName, __workingDir__);
            if (fp == "" || !fileSystem.isFile (fp))  return "Invalid file name.";
            if (!telnetUserHasRightToAccessFile (fp)) return "Access denyed.";

            char buff [1500]; // MTU = 1500 (maximum transmission unit), TCP_SND_BUF = 5744 (a maximum block size that ESP32 can send), FAT cluster size = n * 512. 1500 seems the best option
            *buff = 0;
            
            File f;
            if ((bool) (f = fileSystem.open (fp, "r"))) {
                int i = strlen (buff);
                while (f.available ()) {
                    switch (*(buff + i) = f.read ()) {
                      case '\r':  // ignore
                                  break;
                      case '\n':  // LF-CRLF conversion
                                  *(buff + i ++) = '\r'; 
                                  *(buff + i ++) = '\n';
                                  break;
                      default:
                                  i ++;                  
                    }
                    if (i >= sizeof (buff) - 2) { 

                                                  diskTrafficInformation.bytesRead += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                                                  if (sendTelnet (buff, i) <= 0) { f.close (); return ""; }
                                                  i = 0; 
                                                }
                    }
                    if (i)                      { 

                                                    diskTrafficInformation.bytesRead += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                                                    if (sendTelnet (buff, i) <= 0) { f.close (); return ""; }
                                                }
              } else {
                  f.close (); return string ("Can't read ") + fp;
              }
              f.close ();
              return "";
        }

        string __catClientToFile__ (char *fileName, telnetConnection *tcn) {
          if (!fileSystem.mounted ())                   return "File system not mounted. You may have to format flash disk first.";
          string fp = fileSystem.makeFullPath (fileName, __workingDir__);
          if (fp == "" || fileSystem.isDirectory (fp))  return "Invalid file name.";
          if (!telnetUserHasRightToAccessFile (fp))     return "Access denyed.";

          File f;
          if ((bool) (f = fileSystem.open (fp, "w"))) {
            while (char c = recvTelnetChar ()) { 
              switch (c) {
                case 0: // Error
                case 3: // Ctrl-C
                        f.close (); return fp + " not fully written.";
                case 4: // Ctrl-D or Ctrl-Z
                        f.close (); return string ("\r\n") + fp + " written.";
                case 13: // ignore
                        break;
                case 10: // LF -> CRLF conversion
                        if (f.write ((uint8_t *) "\r\n", 2) != 2) { 
                          f.close (); return string ("Can't write ") + fp;
                        } 

                        diskTrafficInformation.bytesWritten += 2; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                        // echo
                        if (sendTelnet ("\r\n") <= 0) return "";
                        break;
                default: // character 
                        if (f.write ((uint8_t *) &c, 1) != 1) { 
                          f.close (); return string ("Can't write ") + fp;
                        } 

                        diskTrafficInformation.bytesWritten += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                        // echo
                        if (sendTelnet (&c, 1) <= 0) return "";
                        break;
              }
            }
            f.close ();
          } else {
            return string ("Can't write ") + fp;
          }
          return "";
        }

        string __tail__ (char *fileName, bool follow, telnetConnection *tcn) {
            if (!fileSystem.mounted ())               return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (fileName, __workingDir__);
            if (fp == "" || !fileSystem.isFile (fp))  return "Invalid file name.";
            if (!telnetUserHasRightToAccessFile (fp)) return "Access denyed.";

            File f;
            size_t filePosition = 0;
            do {
                if ((bool) (f = fileSystem.open (fp, "r"))) {
                    // if this is the first time skip to (almost) the end of file
                    if (filePosition == 0) {
                        size_t fileSize = f.size ();
                        // move to 512 bytes before the end of the file
                        f.seek (fileSize > 512 ? fileSize - 512 : 0);
                        // move past next \n
                        while (f.available () && f.read () != '\n') {

                          diskTrafficInformation.bytesRead += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                        }
                        filePosition = f.position ();
                    } else {
                        f.seek (filePosition);
                    }
                    if (f.size () > filePosition) {
                        // display the rest of the file
                        while (f.available ()) {
                            char c;
                            switch ((c = f.read ())) {
                              case '\r':  // ignore
                                          break;
                              case '\n':  // LF-CRLF conversion
                                          if (sendTelnet ("\r\n") <= 0) { f.close (); return ""; }
                                          break;
                              default:
                                          if (sendTelnet (&c, sizeof (c)) <= 0) { f.close (); return ""; }
                          }

                          diskTrafficInformation.bytesRead += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                      }
                  }
                  filePosition = f.position (); 
                  f.close ();
              } else {
                  return string ("Can't read ") + fp;
              }
              // follow?
              if (follow) {
                  unsigned long startMillis = millis ();
                  while (millis () - startMillis < 1000) {
                      if (tcn->recvTelnetChar (true)) {
                          tcn->recvTelnetChar (false); // read pending character
                          f.close ();
                          return ""; // return if user pressed Ctrl-C or any key
                      }
                      if (tcn->connectionTimeOut ()) { sendTelnet ("\r\nTime-out"); tcn->closeConnection (); return ""; } 
                      delay (100);
                    }
                } else {
                    return "";
                }
            } while (true);
            return "";
        }

        string __mkdir__ (char *directoryName) { 
            if (!fileSystem.mounted ())                     return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (directoryName, __workingDir__);
            if (fp == "")                                   return "Invalid directory name.";
            if (!telnetUserHasRightToAccessDirectory (fp))  return "Access denyed.";
    
            if (fileSystem.makeDirectory (fp))              return fp + " made.";
                                                            return string ("Can't make ") + fp;
        }
  
        string __rmdir__ (char *directoryName) { 
            if (!fileSystem.mounted ())                     return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (directoryName, __workingDir__);
            if (fp == "" || !fileSystem.isDirectory (fp))   return "Invalid directory name.";
            if (!telnetUserHasRightToAccessDirectory (fp))  return "Access denyed.";
            if (fp == __homeDir__)                          return "You can't remove your home directory.";
            if (fp == __workingDir__)                       return "You can't remove your working directory.";
    
            if (fileSystem.removeDirectory (fp))            return fp + " removed.";
                                                            return string ("Can't remove ") + fp;
        }      

        string __cd__ (const char *directoryName) { 
            if (!fileSystem.mounted ())                     return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (directoryName, __workingDir__);
            if (fp == "" || !fileSystem.isDirectory (fp))   return "Invalid directory name.";
            if (!telnetUserHasRightToAccessDirectory (fp))  return "Access denyed.";
    
            strcpy (__workingDir__, fp);                    return string ("Your working directory is ") + fp;
        }

        string __pwd__ () { 
            if (!fileSystem.mounted ()) return "File system not mounted. You may have to format flash disk first.";
            // remove extra /
            string s (__workingDir__);
            if (s [s.length () - 1] == '/') s [s.length () - 1] = 0;
            if (s == "") s = "/"; 
            return string ("Your working directory is ") + s;
        }

        string __mv__ (char *srcFileOrDirectory, char *dstFileOrDirectory) { 
            if (!fileSystem.mounted ())                       return "File system not mounted. You may have to format flash disk first.";
            string fp1 = fileSystem.makeFullPath (srcFileOrDirectory, __workingDir__);
            string fp2;
            if (fp1 == "")                                    return "Invalid source file or directory name.";
            if (fileSystem.isDirectory (fp1)) {
                if (!telnetUserHasRightToAccessDirectory (fp1)) return "Access to source file or directory denyed.";
                fp2 = fileSystem.makeFullPath (dstFileOrDirectory, __workingDir__);
                if (fp2 == "")                                  return "Invalid destination file or directory name.";
                if (!telnetUserHasRightToAccessDirectory (fp1)) return "Access destination file or directory denyed.";            
            } else if (fileSystem.isFile (fp1)) {
                if (!telnetUserHasRightToAccessFile (fp1))      return "Access to source file or directory denyed.";
                string fp2 = fileSystem.makeFullPath (dstFileOrDirectory, __workingDir__);
                if (fp2 == "")                                  return "Invalid destination file or directory name.";
                if (!telnetUserHasRightToAccessFile (fp1))      return "Access destination file or directory denyed.";
            } else {
                                                                return "Invalid source file or directory name.";
            }
            
            if (fileSystem.rename (fp1, fp2))                 return string ("Renamed to ") + fp2;
                                                              return string ("Can't rename ") + fp1;
        }

        string __cp__ (char *srcFileName, char *dstFileName) { 
          if (!fileSystem.mounted ())                 return "File system not mounted. You may have to format flash disk first.";
          string fp1 = fileSystem.makeFullPath (srcFileName, __workingDir__);
          if (fp1 == "")                              return "Invalid source file name.";
          if (!telnetUserHasRightToAccessFile (fp1))  return "Access to source file denyed.";
          string fp2 = fileSystem.makeFullPath (dstFileName, __workingDir__);
          if (fp2 == "")                              return "Invalid destination file name.";
          if (!telnetUserHasRightToAccessFile (fp1))  return "Access destination file denyed.";

          File f1, f2;
          string retVal = "File copied.";
          
          if (!(bool) (f1 = fileSystem.open (fp1, "r"))) { return string ("Can't read ") + fp1; }
          if (f1.isDirectory ()) { f1.close (); return  string ("Can't read ") + fp1; }
          if (!(bool) (f2 = fileSystem.open (fp2, "w"))) { f1.close (); return string ("Can't write ") + fp2; }

          int bytesReadTotal = 0;
          int bytesWrittenTotal = 0;
          #define CP_BUFF_SIZE 2 * 1024
          char buff [CP_BUFF_SIZE];
          do {
              int bytesReadThisTime = f1.read ((uint8_t *) buff, sizeof (buff));
              if (bytesReadThisTime == 0) break; // finished, success
              bytesReadTotal += bytesReadThisTime;

              diskTrafficInformation.bytesRead += bytesReadThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

              int bytesWrittenThisTime = f2.write ((uint8_t *) buff, bytesReadThisTime);
              bytesWrittenTotal += bytesWrittenThisTime;

              diskTrafficInformation.bytesWritten += bytesWrittenThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

              if (bytesWrittenThisTime != bytesReadThisTime) { retVal = string ("Can't write ") + fp2; break; } 
          } while (true);
          
          f2.close ();
          f1.close ();
          return retVal;
        }

        string __rm__ (char *fileName) {
            if (!fileSystem.mounted ())               return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (fileName, __workingDir__);
            if (fp == "" || !fileSystem.isFile (fp))  return "Invalid file name.";
            if (!telnetUserHasRightToAccessFile (fp)) return "Access denyed.";

            if (fileSystem.deleteFile (fp))           return fp + " deleted.";
            else                                      return string ("Can't delete ") + fp;
        }

        // not really a vi but small and simple text editor
        string __vi__ (char *fileName, telnetConnection *tcn) {
          // a good reference for telnet ESC codes: https://en.wikipedia.org/wiki/ANSI_escape_code
          // and: https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
          if (!fileSystem.mounted ())               return "File system not mounted. You may have to format flash disk first.";

          if (!fileSystem.mounted ())               return "File system not mounted. You may have to format flash disk first.";
          string fp = fileSystem.makeFullPath (fileName, __workingDir__);
          if (fp == "")                             return "Invalid file name.";
          if (!telnetUserHasRightToAccessFile (fp)) return "Access denyed.";
  
          // 1. create a new file one if it doesn't exist (yet)
          if (!fileSystem.isFile (fp)) {
            File f = fileSystem.open (fp, "w");
            if (f) { f.close (); if (sendTelnet ("\r\nFile created.") <= 0) return ""; }
            else return string ("Can't create ") + fp;
          }
  
          // 2. read the file content into internal vi data structure (lines of Strings)
          #define MAX_LINES 9999 // there are 4 places reserved for line numbers on client screen, so MAX_LINES can go up to 9999
          #define LEAVE_FREE_HEAP 32 * 1024 // always keep at least 32 KB free to other processes so vi command doesn't block everything else from working
          vector<String> lines (25); // vector of Arduino Strings
          // lines.reserve (1000);
          bool dirty = false;
          {
            File f = fileSystem.open (fp, "r");
            if (f) {
              if (!f.isDirectory ()) {
                while (f.available ()) { 
                  if (ESP.getFreeHeap () < LEAVE_FREE_HEAP) { // always leave at least 32 KB free for other things that may be running on ESP32
                    f.close (); 
                    return "Out of memory"; 
                  }
                  char c = (char) f.read ();

                  diskTrafficInformation.bytesRead += 1; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  switch (c) {
                    case '\r':  break; // ignore
                    case '\n':  if (ESP.getFreeHeap () < LEAVE_FREE_HEAP || lines.push_back ("") != OK) { // always leave at least 32 KB free for other things that may be running on ESP32
                                  f.close (); 
                                  return fp + " is too long. Out of memory"; 
                                }
                                if (lines.size () >= MAX_LINES) { 
                                  f.close (); 
                                  return fp + " has too many lines for this simple text editor."; 
                                }
                                break; 
                    case '\t':  if (!lines.size ()) if (lines.push_back ("") != OK) return "Out of memory"; // vi editor (the code below) needs at least 1 (empty) line where text can be entered
                                if (!lines [lines.size () - 1].concat ("    ")) return "Out of memory"; // treat tab as 4 spaces, check success of concatenation
                                break;
                    default:    if (!lines.size ()) if (lines.push_back ("") != OK) return "Out of memory"; // vi editor (the code below) needs at least 1 (empty) line where text can be entered
                                if (!lines [lines.size () - 1].concat (c)) return "Out of memory"; // check success of concatenation
                                break;
                  }
                }
                f.close ();
              } else return "Can't edit a directory.";
              f.close ();
            } else return string ("Can't read ") + fp;
          }    
          if (!lines.size ()) if (lines.push_back ("") != OK) return "Out of memory"; // vi editor (the code below) needs at least 1 (empty) line where text can be entered
  
          // 3. discard any already pending characters from client
          while (recvTelnetChar (true)) recvTelnetChar (false);
  
          // 4. get information about client window size
          if (__clientWindowWidth__) { // we know that client reports its window size, ask for latest information (the user might have resized the window since beginning of telnet session)
            if (sendTelnet (IAC DO NAWS) <= 0) return "";
            // client will reply in the form of: IAC (255) SB (250) NAWS (31) col1 col2 row1 row1 IAC (255) SE (240) but this will be handeled internally by readTelnet function
          } else { // just assume the defaults and hope that the result will be OK
            __clientWindowWidth__ = 80; 
            __clientWindowHeight__ = 24;   
          }
          recvTelnetChar (true);
          if (__clientWindowWidth__ < 44 || __clientWindowHeight__ < 5) return "Clinent telnet window is too small for vi.";
  
          // 5. edit 
          int textCursorX = 0;  // vertical position of cursor in text
          int textCursorY = 0;  // horizontal position of cursor in text
          int textScrollX = 0;  // vertical text scroll offset
          int textScrollY = 0;  // horizontal text scroll offset
  
          bool redrawHeader = true;
          bool redrawAllLines = true;
          bool redrawLineAtCursor = false; 
          bool redrawFooter = true;
          string message = string (" ") + string (lines.size ()) + " lines ";
  
                                // clear screen
                                if (sendTelnet ("\x1b[2J") <= 0) return "";  // ESC[2J = clear screen
  
          while (true) {
            // a. redraw screen 
            string s;
         
            if (redrawHeader)   { 
                                  s = "\x1b[H----+"; s.rPad (__clientWindowWidth__ - 26, '-'); s += " Save: Ctrl-S, Exit: Ctrl-X -"; // ESC[H = move cursor home
                                  if (sendTelnet (s) <= 0) return "";  
                                  redrawHeader = false;
                                }
            if (redrawAllLines) {
  
                                  // Redrawing algorithm: straight forward idea is to scan screen lines with text i ∈ [2 .. __clientWindowHeight__ - 1], calculate text line on
                                  // this possition: i - 2 + textScrollY and draw visible part of it: line [...].substring (textScrollX, __clientWindowWidth__ - 5 + textScrollX), __clientWindowWidth__ - 5).
                                  // When there are frequent redraws the user experience is not so good since we often do not have enough time to redraw the whole screen
                                  // between two key strokes. Therefore we'll always redraw just the line at cursor position: textCursorY + 2 - textScrollY and then
                                  // alternate around this line until we finish or there is another key waiting to be read - this would interrupt the algorithm and
                                  // redrawing will repeat after ne next character is processed.
                                  {
                                    int nextScreenLine = textCursorY + 2 - textScrollY;
                                    int nextTextLine = nextScreenLine - 2 + textScrollY;
                                    bool topReached = false;
                                    bool bottomReached = false;
                                    for (int i = 0; (!topReached || !bottomReached) && !(recvTelnetChar (true)) ; i++) { // if user presses any key redrawing stops (bettwr user experienca while scrolling)
                                      if (i % 2 == 0) { nextScreenLine -= i; nextTextLine -= i; } else { nextScreenLine += i; nextTextLine += i; }
                                      if (nextScreenLine == 2) topReached = true;
                                      if (nextScreenLine == __clientWindowHeight__ - 1) bottomReached = true;
                                      if (nextScreenLine > 1 && nextScreenLine < __clientWindowHeight__) {
                                        // draw nextTextLine at nextScreenLine 
                                        if (nextTextLine < lines.size ())
                                          sprintf (s, "\x1b[%i;0H%4i|", nextScreenLine, nextTextLine + 1);  // display line number - users would count lines from 1 on, whereas program counts them from 0 on
                                        else
                                          sprintf (s, "\x1b[%i;0H    |", nextScreenLine);  // line numbers will not be displayed so the user will know where the file ends                                        
                                        // ESC[line;columnH = move cursor to line;column, it is much faster to append line with spaces then sending \x1b[0J (delte from cursor to the end of screen)
                                        size_t escCommandLength = s.length ();
                                        if (nextTextLine < lines.size ())
                                          if (lines [nextTextLine].length () >= textScrollX)                                        
                                            s += lines [nextTextLine].c_str () + textScrollX;
                                        s.rPad (__clientWindowWidth__ + escCommandLength, ' ');
                                        s.erase (__clientWindowWidth__ - 5 + escCommandLength);
                                        if (sendTelnet (s) <= 0) return "";
                                      }
                                    }
                                    if (topReached && bottomReached) redrawAllLines = false; // if we have drown all the lines we don't have to run this code again 
                                  }
                                  
            } else if (redrawLineAtCursor) {
                                  // calculate screen line from text cursor position
                                  // ESC[line;columnH = move cursor to line;column (columns go from 1 to clientWindowsColumns - inclusive), it is much faster to append line with spaces then sending \x1b[0J (delte from cursor to the end of screen)
                                  sprintf (s, "\x1b[%i;6H", textCursorY + 2 - textScrollY);
                                  size_t escCommandLength = s.length ();
                                  if (lines [textCursorY].length () >= textScrollX)
                                    s += lines [textCursorY].c_str () + textScrollX;
                                  s.rPad (__clientWindowWidth__ + escCommandLength, ' ');
                                  s.erase (__clientWindowWidth__ - 5 + escCommandLength);
                                  if (sendTelnet (s) <= 0) return ""; 
                                  redrawLineAtCursor = false;
                                }
            if (redrawFooter)   {                                  
                                  sprintf (s, "\x1b[%i;0H----+", __clientWindowHeight__);
                                  s.rPad (__clientWindowWidth__ + s.length () - 5, '-');                                  
                                  if (sendTelnet (s) <= 0) return "";
                                  redrawFooter = false;
                                }
            if (message != "")  {
                                  sprintf (s, "\x1b[%i;2H%s", __clientWindowHeight__, message);
                                  if (sendTelnet (s) <= 0) return ""; 
                                  message = ""; redrawFooter = true; // we'll clear the message the next time screen redraws
                                }

            // b. restore cursor position - calculate screen coordinates from text coordinates
            {
              // ESC[line;columnH = move cursor to line;column
              sprintf (s, "\x1b[%i;%iH", textCursorY - textScrollY + 2, textCursorX - textScrollX + 6);
              if (sendTelnet (s) <= 0) return ""; // ESC[line;columnH = move cursor to line;column
            }
  
            // c. read and process incoming stream of characters
            char c = 0;
            delay (1);
            if (!(c = recvTelnetChar ())) return "";
            switch (c) {
              case 24:  // Ctrl-X
                        if (dirty) {
                          string tmp = string ("\x1b[") + string (__clientWindowHeight__) + ";2H Save changes (y/n)? ";
                          if (sendTelnet (tmp) <= 0) return ""; 
                          redrawFooter = true; // overwrite this question at next redraw
                          while (true) {                                                     
                            if (!(c = recvTelnetChar ())) return "";
                            if (c == 'y') goto saveChanges;
                            if (c == 'n') break;
                          }
                        } 
                        {
                          string tmp = string ("\x1b[") + string (__clientWindowHeight__) + ";2H Share and Enjoy ----\r\n";
                          if (sendTelnet (tmp) <= 0) return "";
                        }                        
                        return ""; 
              case 19:  // Ctrl-S
        saveChanges:
                        // save changes to fp
                        {
                          bool e = false;
                          File f = fileSystem.open (fp, "w");
                          if (f) {
                            if (!f.isDirectory ()) {
                              for (int i = 0; i < lines.size (); i++) {
                                int toWrite = strlen (lines [i].c_str ()); if (i) toWrite += 2;
                                if (toWrite != f.write (i ? (uint8_t *) ("\r\n" + lines [i]).c_str (): (uint8_t *) lines [i].c_str (), toWrite)) { e = true; break; }

                                diskTrafficInformation.bytesWritten += toWrite; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                              }
                            }
                            f.close ();
                          }
                          if (e) { message = " Could't save changes "; } else { message = " Changes saved "; dirty = false; }
                        }
                        break;
              case 27:  // ESC [A = up arrow, ESC [B = down arrow, ESC[C = right arrow, ESC[D = left arrow, 
                        if (!(c = recvTelnetChar ())) return "";
                        switch (c) {
                          case '[': // ESC [
                                    if (!(c = recvTelnetChar ())) return "";
                                    switch (c) {
                                      case 'A':  // ESC [ A = up arrow
                                                if (textCursorY > 0) textCursorY--; 
                                                if (textCursorX > lines [textCursorY].length ()) textCursorX = lines [textCursorY].length ();
                                                break;                          
                                      case 'B':  // ESC [ B = down arrow
                                                if (textCursorY < lines.size () - 1) textCursorY++;
                                                if (textCursorX > lines [textCursorY].length ()) textCursorX = lines [textCursorY].length ();
                                                break;
                                      case 'C':  // ESC [ C = right arrow
                                                if (textCursorX < lines [textCursorY].length ()) textCursorX++;
                                                else if (textCursorY < lines.size () - 1) { textCursorY++; textCursorX = 0; }
                                                break;
                                      case 'D':  // ESC [ D = left arrow
                                                if (textCursorX > 0) textCursorX--;
                                                else if (textCursorY > 0) { textCursorY--; textCursorX = lines [textCursorY].length (); }
                                                break;        
                                      case '1': // ESC [ 1 = home
                                                if (!(c = recvTelnetChar ())) return ""; // read final '~'
                                                textCursorX = 0;
                                                break;
                                      case '4': // ESC [ 4 = end
                                                if (!(c = recvTelnetChar ())) return "";  // read final '~'
                                                textCursorX = lines [textCursorY].length ();
                                                break;
                                      case '5': // ESC [ 5 = pgup
                                                if (!(c = recvTelnetChar ())) return ""; // read final '~'
                                                textCursorY -= (__clientWindowHeight__ - 2); if (textCursorY < 0) textCursorY = 0;
                                                if (textCursorX > lines [textCursorY].length ()) textCursorX = lines [textCursorY].length ();
                                                break;
                                      case '6': // ESC [ 6 = pgdn
                                                if (!(c = recvTelnetChar ())) return ""; // read final '~'
                                                textCursorY += (__clientWindowHeight__ - 2); if (textCursorY >= lines.size ()) textCursorY = lines.size () - 1;
                                                if (textCursorX > lines [textCursorY].length ()) textCursorX = lines [textCursorY].length ();
                                                break;  
                                      case '3': // ESC [ 3 
                                                if (!(c = recvTelnetChar ())) return "";
                                                switch (c) {
                                                  case '~': // ESC [ 3 ~ (126) - putty reports del key as ESC [ 3 ~ (126), since it also report backspace key as del key let' treat del key as backspace                                                                 
                                                            goto backspace;
                                                  default:  // ignore
                                                            break;
                                                }
                                                break;
                                      default:  // ignore
                                                break;
                                    }
                                    break;
                           default: // ignore
                                    break;
                        }
                        break;
              case 8:   // Windows telnet.exe: back-space, putty does not report this code
        backspace:
                        if (textCursorX > 0) { // delete one character left of cursor position
                            int l1 = lines [textCursorY].length ();
                            lines [textCursorY].remove (textCursorX - 1, 1);
                            int l2 = lines [textCursorY].length (); if (l2 + 1 != l1) return "Out of memory"; // check success of character deletion
                          dirty = redrawLineAtCursor = true; // we only have to redraw this line
                          __telnet_connection_time_out__ = 0; // infinite
                          textCursorX--;
                        } else if (textCursorY > 0) { // combine 2 lines
                          textCursorY--;
                          textCursorX = lines [textCursorY].length (); 
                          if (!lines [textCursorY].concat (lines [textCursorY + 1])) return "Out of memory"; // check success of concatenation
                          lines.erase (textCursorY + 1); 
                          dirty = redrawAllLines = true; // we need to redraw all visible lines (at least lines from testCursorY down but we wont write special cede for this case)
                          __telnet_connection_time_out__ = 0; // infinite
                        }
                        break; 
              case 127: // Windows telnet.exe: delete, putty: backspace
                        if (textCursorX < lines [textCursorY].length ()) { // delete one character at cursor position
                            int l1 = lines [textCursorY].length ();
                          lines [textCursorY].remove (textCursorX, 1);
                            int l2 = lines [textCursorY].length (); if (l2 + 1 != l1) return "Out of memory"; // check success of character deletion
                          dirty = redrawLineAtCursor = true; // we only need to redraw this line
                          __telnet_connection_time_out__ = 0; // infinite
                        } else { // combine 2 lines
                          if (textCursorY < lines.size () - 1) {
                            if (!lines [textCursorY].concat (lines [textCursorY + 1])) return "Out of memory"; // check success of concatenation
                            lines.erase (textCursorY + 1); 
                            dirty = redrawAllLines = true; // we need to redraw all visible lines (at least lines from testCursorY down but we wont write special cede for this case)                          
                            __telnet_connection_time_out__ = 0; // infinite
                          }
                        }
                        break;
              case '\n': // enter
                        // split current line at cursor into textCursorY + 1 and textCursorY
                        if (lines.size () >= MAX_LINES || ESP.getFreeHeap () < LEAVE_FREE_HEAP || lines.insert (textCursorY + 1, lines [textCursorY].substring (textCursorX)) != OK) { 
                          message = " Out of memory or too many lines ";
                        } else {
                          lines [textCursorY] = lines [textCursorY].substring (0, textCursorX);
                            if (lines [textCursorY].length () != textCursorX) return "Out of memory"; // check success of string assignment
                          textCursorX = 0;
                          dirty = redrawAllLines = true; // we need to redraw all visible lines (at least lines from testCursorY down but we wont write special cede for this case)
                          __telnet_connection_time_out__ = 0; // infinite
                          textCursorY++;
                        }
                        break;
              case '\r':  // ignore
                        break;
              default:  // normal character
                        if (ESP.getFreeHeap () < LEAVE_FREE_HEAP) { 
                          message = " Out of memory ";
                        } else {
                          if (c == '\t') s = "    "; else s = string (c); // treat tab as 4 spaces
                            int l1 = lines [textCursorY].length (); 
                          lines [textCursorY] = lines [textCursorY].substring (0, textCursorX) + s + lines [textCursorY].substring (textCursorX); // inser character into line textCurrorY at textCursorX position
                            int l2 = lines [textCursorY].length (); if (l2 <= l1) return "Out of memory"; // check success of insertion of characters
                          dirty = redrawLineAtCursor = true; // we only have to redraw this line
                          __telnet_connection_time_out__ = 0; // infinite
                          textCursorX += s.length ();
                        }
                        break;
            }         
            // if cursor has moved - should we scroll?
            {
              if (textCursorX - textScrollX >= __clientWindowWidth__ - 5) {
                textScrollX = textCursorX - (__clientWindowWidth__ - 5) + 1; // scroll left if the cursor fell out of right client window border
                redrawAllLines = true; // we need to redraw all visible lines
              }
              if (textCursorX - textScrollX < 0) {
                textScrollX = textCursorX; // scroll right if the cursor fell out of left client window border
                redrawAllLines = true; // we need to redraw all visible lines
              }
              if (textCursorY - textScrollY >= __clientWindowHeight__ - 2) {
                textScrollY = textCursorY - (__clientWindowHeight__ - 2) + 1; // scroll up if the cursor fell out of bottom client window border
                redrawAllLines = true; // we need to redraw all visible lines
              }
              if (textCursorY - textScrollY < 0) {
                textScrollY = textCursorY; // scroll down if the cursor fell out of top client window border
                redrawAllLines = true; // we need to redraw all visible lines
              }
            }
          }
          return "";
        }
          
        #ifdef __FTP_CLIENT__
          string __ftpPut__ (char *localFileName, char *remoteFileName, char *password, char *userName, int ftpPort, char *ftpServer) {
            if (!fileSystem.mounted ())                   return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (localFileName, __workingDir__);
            if (fp == "" || fileSystem.isDirectory (fp))  return "Invalid local file name.";
            if (!telnetUserHasRightToAccessFile (fp))     return "Access to local file denyed.";
            return ftpPut (fp, remoteFileName, password, userName, ftpPort, ftpServer);
          }
  
          string __ftpGet__ (char *localFileName, char *remoteFileName, char *password, char *userName, int ftpPort, char *ftpServer) {
            if (!fileSystem.mounted ())                   return "File system not mounted. You may have to format flash disk first.";
            string fp = fileSystem.makeFullPath (localFileName, __workingDir__);
            if (fp == "" || fileSystem.isDirectory (fp))  return "Invalid local file name.";
            if (!telnetUserHasRightToAccessFile (fp))     return "Access to local file denyed.";
            return ftpGet (fp, remoteFileName, password, userName, ftpPort, ftpServer);
          }
        #endif

      #endif
      
      #if USER_MANAGEMENT == UNIX_LIKE_USER_MANAGEMENT      

       const char *__passwd__ (char *userName) {
          char password1 [USER_PASSWORD_MAX_LENGTH + 1];
          char password2 [USER_PASSWORD_MAX_LENGTH + 1];
                    
          if (!strcmp (__userName__, userName)) { // user changing password for himself
            // read current password
            __echo__ = false;
            if (sendTelnet ("Enter current password: ") <= 0)                                             return ""; 
            if (recvTelnetLine (password1, sizeof (password1), true) != 10) { __echo__ = true;            return "\r\nPassword not changed."; }
            __echo__ = true;
            // check if password is valid for user
            if (!userManagement.checkUserNameAndPassword (userName, password1))                           return "Wrong password."; 
          } else {                         // root is changing password for another user
            // check if user exists with getUserHomeDirectory
            char homeDirectory [FILE_PATH_MAX_LENGTH + 1];
            if (!userManagement.getHomeDirectory (homeDirectory, sizeof (homeDirectory), userName))       return "User name does not exist."; 
          }
          // read new password twice
          __echo__ = false;
          if (sendTelnet ("\r\nEnter new password: ") <= 0)                                               return ""; 
          if (recvTelnetLine (password1, sizeof (password1), true) != 10)   { __echo__ = true;            return "\r\nPassword not changed."; }
          if (sendTelnet ("\r\nRe-enter new password: ") <= 0)                                            return ""; 
          if (recvTelnetLine (password2, sizeof (password2), true) != 10)   { __echo__ = true;            return "\r\nPassword not changed."; }
          __echo__ = true;
          // check passwords
          if (strcmp (password1, password2))                                                              return "\r\nPasswords do not match.";
          // change password
          if (userManagement.passwd (userName, password1))                                                return "\r\nPassword changed.";
          else                                                                                            return "\r\nError changing password.";  
        }

      #endif
        
    };


    // ----- telnetServer class -----

    class telnetServer {                                             
    
      public:

        // telnetServer state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          STARTING = 1, 
          RUNNING = 2        
        };
        
        STATE_TYPE state () { return __state__; }
    
        telnetServer ( // the following parameters will be pased to each telnetConnection instance
                     String (* telnetCommandHandlerCallback) (int argc, char *argv [], telnetConnection *tcn) = NULL, // telnetCommandHandler callback function provided by calling program
                     // the following parameters will be handeled by telnetServer instance
                     const char *serverIP = "0.0.0.0",                                                                // Telnet server IP address, 0.0.0.0 for all available IP addresses
                     int serverPort = 23,                                                                             // Telnet server port
                     bool (*firewallCallback) (char *connectingIP) = NULL                                             // a reference to callback function that will be celled when new connection arrives 
                   )  { 
                        // create a local copy of parameters for later use
                        __telnetCommandHandlerCallback__ = telnetCommandHandlerCallback;
                        strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0;
                        __serverPort__ = serverPort;
                        __firewallCallback__ = firewallCallback;

                        // start listener in its own thread (task)
                        __state__ = STARTING;                        
                        #define tskNORMAL_PRIORITY 1
                        #ifdef TELNET_SERVER_CORE
                            BaseType_t taskCreated = xTaskCreatePinnedToCore (__listenerTask__, "telnetServer", 2 * 1024, this, tskNORMAL_PRIORITY, NULL, TELNET_SERVER_CORE);
                        #else
                            BaseType_t taskCreated = xTaskCreate (__listenerTask__, "telnetServer", 2 * 1024, this, tskNORMAL_PRIORITY, NULL);
                        #endif
                        if (pdPASS != taskCreated) {
                            #ifdef __DMESG__
                                dmesgQueue << "[telnetServer] xTaskCreate error";
                            #endif
                        } else {
                          // wait until listener starts accepting connections
                          while (__state__ == STARTING) delay (1); 
                          // when constructor returns __state__ could be eighter RUNNING (in case of success) or NOT_RUNNING (in case of error)
                        }
                      }
        
        ~telnetServer ()  {
                          // close listening socket
                          int listeningSocket;
                          xSemaphoreTake (__telnetServerSemaphore__, portMAX_DELAY);
                            listeningSocket = __listeningSocket__;
                            __listeningSocket__ = -1;
                          xSemaphoreGive (__telnetServerSemaphore__);
                          if (listeningSocket > -1) close (listeningSocket);

                          // wait until listener finishes before unloading so that memory variables are still there while the listener is running
                          while (__state__ != NOT_RUNNING) delay (1);
                        }
 
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        String (* __telnetCommandHandlerCallback__) (int argc, char *argv [], telnetConnection *tcn) = NULL;
        char __serverIP__ [46] = "0.0.0.0";
        int __serverPort__ = 23;
        bool (* __firewallCallback__) (char *connectingIP) = NULL;

        int __listeningSocket__ = -1;

        static void __listenerTask__ (void *pvParameters) {
          {
            // get "this" pointer
            telnetServer *ths = (telnetServer *) pvParameters;  
    
            // start listener
            ths->__listeningSocket__ = socket (PF_INET, SOCK_STREAM, 0);
            if (ths->__listeningSocket__ == -1) {
                #ifdef __DMESG__
                    dmesgQueue << "[telnetServer] socket error: " << errno << " " << strerror (errno);
                #endif
            } else {

              // remember some information that netstat telnet command would use
              additionalSocketInformation [ths->__listeningSocket__ - LWIP_SOCKET_OFFSET] = { __LISTENING_SOCKET__, 0, 0, millis (), millis () };

              // make address reusable - so we won't have to wait a few minutes in case server will be restarted
              int flag = 1;
              if (setsockopt (ths->__listeningSocket__, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag)) == -1) {
                  #ifdef __DMESG__
                      dmesgQueue << "[telnetServer] setsockopt error: " <<  errno << " " << strerror (errno);
                  #endif
              } else {
                // bind listening socket to IP address and port     
                struct sockaddr_in serverAddress; 
                memset (&serverAddress, 0, sizeof (struct sockaddr_in));
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_addr.s_addr = inet_addr (ths->__serverIP__);
                serverAddress.sin_port = htons (ths->__serverPort__);
                if (bind (ths->__listeningSocket__, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
                    #ifdef __DMESG__
                        dmesgQueue << "[telnetServer] bind error: " << errno << " " << strerror (errno);
                    #endif
               } else {
                 // mark socket as listening socket
                 if (listen (ths->__listeningSocket__, 4) == -1) {
                    #ifdef __DMESG__
                        dmesgQueue << "[tlnetServer] listen error: " << errno << " " << strerror (errno);
                    #endif
                 } else {

                  // remember some information that netstat telnet command would use
                  additionalSocketInformation [ths->__listeningSocket__ - LWIP_SOCKET_OFFSET] = { __LISTENING_SOCKET__, 0, 0, millis (), millis () };

                  // listener is ready for accepting connections
                  ths->__state__ = RUNNING;
                  #ifdef __DMESG__
                      dmesgQueue << "[telnetServer] listener is running on core " << xPortGetCoreID ();
                  #endif
                  while (ths->__listeningSocket__ > -1) { // while listening socket is opened

                      int connectingSocket;
                      struct sockaddr_in connectingAddress;
                      socklen_t connectingAddressSize = sizeof (connectingAddress);
                      log_i ("listener taks: stack high-water mark: %lu\n", uxTaskGetStackHighWaterMark (NULL));
                      connectingSocket = accept (ths->__listeningSocket__, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
                      if (connectingSocket == -1) {
                          #ifdef __DMESG__
                              if (ths->__listeningSocket__ > -1) dmesgQueue << "[telnetServer] accept error: " << errno << " " << strerror (errno);
                          #endif
                      } else {

                        // remember some information that netstat telnet command would use
                        additionalSocketInformation [ths->__listeningSocket__ - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
                        additionalSocketInformation [connectingSocket - LWIP_SOCKET_OFFSET] = { __TELNET_SERVER_SOCKET__, 0, 0, millis (), millis () };
                        
                        // get client's IP address
                        char clientIP [46]; inet_ntoa_r (connectingAddress.sin_addr, clientIP, sizeof (clientIP)); 
                        // get server's IP address
                        char serverIP [46]; struct sockaddr_in thisAddress = {}; socklen_t len = sizeof (thisAddress);
                        if (getsockname (connectingSocket, (struct sockaddr *) &thisAddress, &len) != -1) inet_ntoa_r (thisAddress.sin_addr, serverIP, sizeof (serverIP));
                        // port number could also be obtained if needed: ntohs (thisAddress.sin_port);
                        if (ths->__firewallCallback__ && !ths->__firewallCallback__ (clientIP)) {
                            #ifdef __DMESG__
                                dmesgQueue << "[telnetServer] firewall rejected connection from " << clientIP;
                            #endif
                            close (connectingSocket);
                        } else {
                          // make the socket non-blocking so that we can detect time-out
                          if (fcntl (connectingSocket, F_SETFL, O_NONBLOCK) == -1) {
                              #ifdef __DMESG__
                                  dmesgQueue << "[telnetServer] fcntl error: " << errno << " " << strerror (errno);
                              #endif
                              close (connectingSocket);
                          } else {
                                // create telnetConnection instence that will handle the connection, then we can lose reference to it - telnetConnection will handle the rest
                                telnetConnection *tcn = new (std::nothrow) telnetConnection (connectingSocket, ths->__telnetCommandHandlerCallback__, clientIP, serverIP);
                                if (!tcn) {
                                    #ifdef __DMESG__
                                        dmesgQueue << "[telnetConnection] service unavaliable";
                                    #endif
                                    sendAll (connectingSocket, telnetServiceUnavailable, TELNET_CONNECTION_TIME_OUT);
                                    close (connectingSocket); // normally telnetConnection would do this but if it is not created we have to do it here
                                } else {
                                  if (tcn->state () != telnetConnection::RUNNING) {
                                      #ifdef __DMESG__
                                          dmesgQueue << "[telnetConnection] service unavaliable";
                                      #endif
                                      sendAll (connectingSocket, telnetServiceUnavailable, TELNET_CONNECTION_TIME_OUT);
                                      delete (tcn); // normally telnetConnection would do this but if it is not running we have to do it here
                                  }
                                }

                          } // fcntl
                        } // firewall
                      } // accept
                      
                  } // while accepting connections
                  #ifdef __DMESG__
                      dmesgQueue << "[telnetServer] stopped";
                  #endif
          
                 } // listen
               } // bind
              } // setsockopt
              int listeningSocket;
              xSemaphoreTake (__telnetServerSemaphore__, portMAX_DELAY);
                listeningSocket = ths->__listeningSocket__;
                ths->__listeningSocket__ = -1;
              xSemaphoreGive (__telnetServerSemaphore__);
              if (listeningSocket > -1) close (listeningSocket);
            } // socket
            ths->__state__ = NOT_RUNNING;
          }
          // stop the listening thread (task)
          vTaskDelete (NULL); // it is listener's responsibility to close itself          
        }
          
    };

#endif
