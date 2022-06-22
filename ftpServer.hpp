/*

    ftpServer.hpp 
 
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
    FTP server reads and executes FTP commands. The transfer of files in active in passive mode is supported but some of the commands may 

    February, 14, 2022, Bojan Jurca

    Nomenclature used here for easier understaning of the code:

     - "connection" normally applies to TCP connection from when it was established to when it is terminated

                  There is normally only one control TCP connection per FTP session. Beside control connection FTP client and server open
                  also a data TCP connection when certain commands are involved (like LIST, RETR, STOR, ...).

     - "session" applies to user interaction between login and logout

                  The first thing that the user does when a TCP connection is established is logging in and the last thing is logging out. It TCP
                  connection drops for some reason the user is automatically logged out.
                  
      - "buffer size" is the number of bytes that can be placed in a buffer. 
      
                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".

    Handling FTP commands that always arrive through control TCP connection is pretty straightforward. The data transfer TCP connection, on the other 
    hand, can be initialized eighter by the server or the client. In the first case, the client starts a listener (thus acting as a server) and sends 
    its connection information (IP and port number) to the FTP server through the PORT command. The server then initializes data connection to the 
    client. This is called active data connection. Windows FTP.exe uses this kind of data transfer by default. In the second case, the client sends 
    a PASV command to the FTP server, then the server starts another listener (beside the control connection listener that is already running) and 
    sends its connection information (IP and port number) back to the client as a reply to the PASV command. The client then initializes data 
    connection to the server. This is called passive data connection. Windows Explorer uses this kind of data transfer.

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>


#ifndef __FTP_SERVER__
  #define __FTP_SERVER__

    // TUNNING PARAMETERS

    #define FTP_SERVER_STACK_SIZE 2 * 1024                      // TCP listener
    #define FTP_SESSION_STACK_SIZE 10 * 1024                    // TCP connection
    #define FTP_CMDLINE_BUFFER_SIZE 300                         // reading and temporary keeping FTP command lines
    #define FTP_CONTROL_CONNECTION_TIME_OUT 300000              // 300000 ms = 5 min 
    #define FTP_DATA_CONNECTION_TIME_OUT 3000                   // 3000 ms = 3 sec 

    #define ftpServiceUnavailable (char *) "421 FTP service is currently unavailable\r\n"

    #ifndef HOSTNAME
      #define "MyESP32Server"                               // use default if not defined previously
    #endif
    

    // ----- CODE -----

    #include "dmesg_functions.h"

    #include "user_management.h"    // for logging in
    #include "time_functions.h"     // to display file times, cronDaemon still needs to be run first 
    #include "file_system.h"        // FTP needs file system, of course but it still needs to be mounted first 


    // control ftpServer critical sections
    static SemaphoreHandle_t __ftpServerSemaphore__ = xSemaphoreCreateMutex (); 

    // cycle through set of port numbers when FTP server is working in pasive mode
    int __pasiveDataPort__ () {
      static int lastPasiveDataPort = 1024;                                                 // change pasive data port range if needed
      xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
        int pasiveDataPort = lastPasiveDataPort = (((lastPasiveDataPort + 1) % 16) + 1024); // change pasive data port range if needed
      xSemaphoreGive (__ftpServerSemaphore__);
      return pasiveDataPort;
    }


    // ----- ftpControlConnection class -----

    class ftpControlConnection {

      public:

        // ftpControlConnection state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          RUNNING = 2        
        };

        STATE_TYPE state () { return __state__; }

        ftpControlConnection ( // the following parameters will be handeled by ftpControlConnection instance
                         int connectionSocket,
                         char *clientIP, char *serverIP
                       )  {
                            // create a local copy of parameters for later use
                            __controlConnectionSocket__ = connectionSocket;
                            strncpy (__clientIP__, clientIP, sizeof (__clientIP__)); __clientIP__ [sizeof (__clientIP__) - 1] = 0; // copy client's IP since connection may live longer than the server that created it
                            strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0; // copy server's IP since connection may live longer than the server that created it
                            // handle connection in its own thread (task)       
                            #define tskNORMAL_PRIORITY 1
                            if (pdPASS != xTaskCreate (__controlConnectionTask__, "ftpControlConnection", FTP_SESSION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL)) {
                              dmesg ("[ftpControlConnection] xTaskCreate error");
                            } else {
                              __state__ = RUNNING;                            
                            }

                            #ifdef __PERFMON__
                              xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                                __perfOpenedFtpControlConnections__ ++;
                                __perfConcurrentFtpControlConnections__ ++;
                              xSemaphoreGive (__ftpServerSemaphore__);
                            #endif
                          }

        ~ftpControlConnection ()  {
                                closeControlConnection ();
                                closeDataConnection ();
                                #ifdef __PERFMON__
                                  xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                                    __perfConcurrentFtpControlConnections__ --;
                                  xSemaphoreGive (__ftpServerSemaphore__);
                                #endif
                            }

        bool controlConnectionTimeOut () { return millis () - __lastActive__ >= FTP_CONTROL_CONNECTION_TIME_OUT; }
        
        void closeControlConnection () { // both, control and data connection
                                  int connectionSocket;
                                  xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                                    connectionSocket = __controlConnectionSocket__;
                                    __controlConnectionSocket__ = -1;
                                  xSemaphoreGive (__ftpServerSemaphore__);
                                  if (connectionSocket > -1) close (connectionSocket);
                                }
                                
        void closeDataConnection () {
                                  int connectionSocket;
                                  xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                                    connectionSocket = __dataConnectionSocket__;
                                    __dataConnectionSocket__ = -1;
                                  xSemaphoreGive (__ftpServerSemaphore__);
                                  if (connectionSocket > -1) close (connectionSocket);                                  
                                }
                                        
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        unsigned long __lastActive__ = millis (); // time-out detection        
      
        int __controlConnectionSocket__ = -1;
        int __dataConnectionSocket__ = -1;
        char __clientIP__ [46] = "";
        char __serverIP__ [46] = "";

        char __cmdLine__ [FTP_CMDLINE_BUFFER_SIZE];

        // FTP session related variables
        char __userName__ [USER_PASSWORD_MAX_LENGTH + 1] = "";
        char __homeDir__ [FILE_PATH_MAX_LENGTH + 1] = "";
        char __workingDir__ [FILE_PATH_MAX_LENGTH + 1] = "";

        static void __controlConnectionTask__ (void *pvParameters) {
          // get "this" pointer
          ftpControlConnection *ths = (ftpControlConnection *) pvParameters;           
          { // code block

            // send welcome message to the client
            #if USER_MANAGEMENT == NO_USER_MANAGEMENT
              {
                String s = "220-" + String (HOSTNAME) + " FTP server - everyone is allowed to login\r\n220 \r\n";
                if (sendAll (ths->__controlConnectionSocket__, (char *) s.c_str (), s.length (), FTP_CONTROL_CONNECTION_TIME_OUT) <= 0) goto endOfConnection;
              }
            #else
              {
                String s = "220-" + String (HOSTNAME) + " FTP server - please login\r\n220 \r\n";
                if (sendAll (ths->__controlConnectionSocket__, (char *) s.c_str (), s.length (), FTP_CONTROL_CONNECTION_TIME_OUT) <= 0) goto endOfConnection;
              }
            #endif  

            // read and process FTP commands in an endless loop
            int receivedTotal = 0;
            do {
              receivedTotal = recvAll (ths->__controlConnectionSocket__, ths->__cmdLine__ + receivedTotal, FTP_CMDLINE_BUFFER_SIZE - 1 - receivedTotal, (char *) "\n", FTP_CONTROL_CONNECTION_TIME_OUT);
              if (receivedTotal <= 0) {
                dmesg ("[ftpControlConnection] recv error: ", errno);
                goto endOfConnection;
              }
               // DEBUG: Serial.printf ("cmdLine = |%s|, |%i|\n", ths->__cmdLine__, strlen (ths->__cmdLine__));

              // parse command line
              char *ftpCommand = ths->__cmdLine__;
              char *ftpArgument = (char *) "";
              while (*ftpCommand > ' ') ftpCommand ++;
              if (*ftpCommand) { 
                *ftpCommand = 0; 
                ftpArgument = ftpCommand + 1; 
                while (*ftpArgument >= ' ') ftpArgument ++; // long file names may have spaces
                *ftpArgument = 0;
                ftpArgument = ftpCommand + 1;
              }
              ftpCommand = ths->__cmdLine__;
              // DEBUG: Serial.printf ("ftpCommand = %s, ftpArgument = %s\n", ftpCommand, ftpArgument); 
              
              // process command line
              if (*ftpCommand) {

                String s = ths->internalFtpCommandHandler (ftpCommand, ftpArgument);
                if (ths->__controlConnectionSocket__ == -1) goto endOfConnection; // in case of quit
                // write the reply
                if (s != "" && sendAll (ths->__controlConnectionSocket__, (char *) s.c_str (), s.length (), FTP_CONTROL_CONNECTION_TIME_OUT) <= 0) {
                  dmesg ("[ftpControlConnection] send error: ", errno);
                  goto endOfConnection;
                }
              }
        // nextFtpCommand:
              receivedTotal = 0; // FTP client does not send another FTP command until it gets a reply from current one, meaning that cmdLine is empty now
              
            } while (ths->__controlConnectionSocket__ > -1); // while the connection is still opened

          } // code block
        endOfConnection:  
          if (*ths->__homeDir__) dmesg ("[ftpControlConnection] user logged out: ", ths->__userName__);
          // all variables are freed now, unload the instance and stop the task (in this order)
          delete ths;
          vTaskDelete (NULL);                
        }

        String internalFtpCommandHandler (char *ftpCommand, char *ftpArgument) {

          #define ftpCommandIs(X) (!strcmp (ftpCommand, X))

               if (ftpCommandIs ("QUIT"))                                                   return __QUIT__ ();
          else if (ftpCommandIs ("OPTS"))                                                   return __OPTS__ (ftpArgument);
          else if (ftpCommandIs ("USER"))                                                   return __USER__ (ftpArgument); // not entering user name may be OK for anonymous login  
          else if (ftpCommandIs ("PASS"))                                                   return __PASS__ (ftpArgument); // not entering password may be OK for anonymous login  
          else if (ftpCommandIs ("CWD"))                                                    return __CWD__  (ftpArgument);
          else if (ftpCommandIs ("PWD") || ftpCommandIs ("XPWD"))                           return __XPWD__ ();
          else if (ftpCommandIs ("TYPE"))                                                   return __TYPE__ (ftpArgument);
          else if (ftpCommandIs ("NOOP"))                                                   return __NOOP__ ();
          else if (ftpCommandIs ("SYST"))                                                   return __SYST__ ();
          else if (ftpCommandIs ("SIZE"))                                                   return __SIZE__ (ftpArgument);
          else if (ftpCommandIs ("PORT"))                                                   return __PORT__ (ftpArgument);
          else if (ftpCommandIs ("PASV"))                                                   return __PASV__ ();
          else if (ftpCommandIs ("LIST") || ftpCommandIs ("NLST"))                          return __NLST__ (*ftpArgument ? ftpArgument : __workingDir__);
          else if (ftpCommandIs ("XMKD") || ftpCommandIs ("MKD"))                           return __XMKD__ (ftpArgument);
          else if (ftpCommandIs ("XRMD") || ftpCommandIs ("RMD") || ftpCommandIs ("DELE"))  return __XRMD__ (ftpArgument);
          else if (ftpCommandIs ("RETR"))                                                   return __RETR__ (ftpArgument);
          else if (ftpCommandIs ("STOR"))                                                   return __STOR__ (ftpArgument);
          else if (ftpCommandIs ("RNFR"))                                                   return __RNFR__ (ftpArgument);
          else if (ftpCommandIs ("RNTO"))                                                   return __RNTO__ (ftpArgument);

          else return "502 command " + String (ftpCommand) + " not implemented\r\n";
        }

        String __QUIT__ ()  {
          // report client we are closing connection(s)
          sendAll (__controlConnectionSocket__, (char *) "221 closing connection\r\n", strlen ("221 closing connection\r\n"), FTP_CONTROL_CONNECTION_TIME_OUT);
          closeControlConnection ();
          closeDataConnection ();
          return "";
        }

        String __OPTS__ (char *opts) { // enable UTF8
          if (!strcmp (opts, "UTF8 ON"))  return F ("200 UTF8 enabled\r\n"); // by default, we don't have to do anything, just report to the client that it is ok to use UTF-8
                                          return F ("502 OPTS arguments not supported\r\n");
        }

        String __USER__ (char *userName) { // save user name and require password
          if (strlen (userName) < sizeof (__userName__)) strcpy (__userName__, userName);
          return F ("331 enter password\r\n");
        }

        String __PASS__ (char *password) { // login
          if (!checkUserNameAndPassword (__userName__, password)) { 
            dmesg ("[ftpControlConnection] user failed to login: ", __userName__);
            delay (100);
            return F ("530 user name or password incorrect\r\n"); 
          }
          // prepare session defaults
          getUserHomeDirectory (__homeDir__, sizeof (__homeDir__), __userName__);
          if (*__homeDir__) { // if logged in
            strcpy (__workingDir__, __homeDir__);
            dmesg ("[ftpControlConnection] user logged in: ", __userName__);

            // remove extra /
            String s (__homeDir__);
            if (s.charAt (s.length () - 1) == '/') s = s.substring (0, s.length () - 1); 
            if (s == "") s = "/"; 
            return "230 logged on, your home directory is \"" + s + "\"\r\n";
          } else { 
            dmesg ("[ftpControlConnection] user does not have a home directory: ", __userName__);
            return F ("530 user does not have a home directory\r\n"); 
          }
        }

        #define ftpUserHasRightToAccess(fullPath) (strstr(fullPath,__homeDir__)==fullPath) // user has a right to access file or directory if it begins with user's home directory

        String __CWD__ (char *directoryName) { 
          if (!*__homeDir__)                                                                            return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                                                                   return F ("421 file system not mounted\r\n");
          String fp = fullFilePath (directoryName, __workingDir__);
          if (fp == "" || !isDirectory (fp))                                                            return F ("501 invalid directory name\r\n");
          if (!ftpUserHasRightToAccess (fp.c_str ()))                                                   return F ("550 access denyed\r\n");

          // shoud be OK but check anyway:
          if (fp.length () < sizeof (__workingDir__)) strcpy (__workingDir__, (char *) fp.c_str ());    return "250 your working directory is " + fp + "\r\n";
        }

        String __XPWD__ () { 
          if (!*__homeDir__)          return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__) return F ("421 file system not mounted\r\n");

          // remove extra /
          String s (__workingDir__);
          if (s.charAt (s.length () - 1) == '/') s = s.substring (0, s.length () - 1); 
          if (s == "") s = "/"; 
                                      return "257 \"" + s + "\"\r\n";
        }
  
        String __TYPE__ (char *ftpType) { 
          return F ("200 ok\r\n"); // just say OK to whatever type it is
        }
  
        String __NOOP__ () { 
          return F ("200 ok\r\n");
        }

        String __SYST__ () {
          return F ("215 UNIX Type: L8\r\n"); // just say this is UNIX OS
        }

        String __SIZE__ (char *fileName) { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                 return F ("421 file system not mounted\r\n");
          String fp = fullFilePath (fileName, __workingDir__);
          if (fp == "" || !isFile (fp))               return F ("501 invalid file name\r\n");
          if (!ftpUserHasRightToAccess (fp.c_str ())) return F ("550 access denyed\r\n");

          unsigned long fSize = 0;
          File f = fileSystem.open (fp, FILE_READ);
          if (f) { fSize = f.size (); f.close (); }
                                                      return "213 " + String (fSize) + "\r\n";
        }

        String __PORT__ (char *dataConnectionInfo) { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");
          
          int ip1, ip2, ip3, ip4, p1, p2; // get IP and port that client used to set up data connection server
          if (6 == sscanf (dataConnectionInfo, "%i,%i,%i,%i,%i,%i", &ip1, &ip2, &ip3, &ip4, &p1, &p2)) {
            char activeDataIP [46];
            int activeDataPort;
            sprintf (activeDataIP, "%i.%i.%i.%i", ip1, ip2, ip3, ip4); 
            activeDataPort = p1 << 8 | p2;

            // connect to the FTP client that act as a data server now

            /* // we already heve client's IP so we don't have to reslove its name first
            struct hostent *he = gethostbyname (activeDataIP);
            if (!he) {
              dmesg ("[httpClient] gethostbyname() error: ", h_errno);
              return F ("425 can't open active data connection\r\n");
            }
            */
            // create socket
            __dataConnectionSocket__ = socket (PF_INET, SOCK_STREAM, 0);
            if (__dataConnectionSocket__ == -1) {
              dmesg ("[ftpActiveDataConnection] socket() error: ", errno);
                                                      return ftpServiceUnavailable;
            }
            // make the socket not-blocking so that time-out can be detected
            if (fcntl (__dataConnectionSocket__, F_SETFL, O_NONBLOCK) == -1) {
              dmesg ("[ftpActiveDataConnection] fcntl() error: ", errno);
              closeDataConnection ();              
                                                      return F ("425 can't open active data connection\r\n");
            }
            // connect to client that acts as a data server 
            struct sockaddr_in serverAddress;
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_port = htons (activeDataPort);
            serverAddress.sin_addr.s_addr = inet_addr (activeDataIP); // serverAddress.sin_addr.s_addr = *(in_addr_t *) he->h_addr; 
            if (connect (__dataConnectionSocket__, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
              if (errno != EINPROGRESS) {
                dmesg ("[ftpActiveDataConnection] connect() error: ", errno); 
                closeDataConnection ();
                                                      return F ("425 can't open active data connection\r\n");
              }
            } // it is likely that socket is not connected yet at this point (the socket is non-blocking)
                                                      return F ("200 port ok\r\n");
          } 
                                                      return F ("425 can't open active data connection\r\n");
        }
  
        String __PASV__ () { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");

          int ip1, ip2, ip3, ip4, p1, p2; // get (this) server IP and next free port
          if (4 != sscanf (__serverIP__, "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) { // shoul always succeed
            dmesg ("[ftpPasiveDataConnection] can't parse server IP: ", __serverIP__);
            closeDataConnection ();
                                                      return F ("425 can't open pasive data connection\r\n");                  
          }
          // get next free port
          int pasiveDataPort = __pasiveDataPort__ ();
          p2 = pasiveDataPort % 256;
          p1 = pasiveDataPort / 256;           

          // set up a new server for data connection and send connection information to the FTP client so it can connect to it
        
          __dataConnectionSocket__ = socket (PF_INET, SOCK_STREAM, 0);
          if (__dataConnectionSocket__ == -1) {
            dmesg ("[ftpPasiveDataConnection] socket() error: ", errno);
                                                      return ftpServiceUnavailable;
          }
          // make address reusable - so we won't have to wait a few minutes in case server will be restarted
          int flag = 1;
          setsockopt (__dataConnectionSocket__, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag));
          // bind listening socket to IP address and port
          struct sockaddr_in serverAddress;
          memset (&serverAddress, 0, sizeof (struct sockaddr_in));
          serverAddress.sin_family = AF_INET;
          serverAddress.sin_addr.s_addr = inet_addr (__serverIP__);
          serverAddress.sin_port = htons (pasiveDataPort);
          if (bind (__dataConnectionSocket__, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
            dmesg ("[ftpPasiveDataConnection] bind() error: ", errno);
            closeDataConnection ();
                                                      return F ("425 can't open pasive data connection\r\n");            
          }
          // mark socket as listening socket
          if (listen (__dataConnectionSocket__, 1) == -1) {
            dmesg ("[ftpPasiveDataConnection] listen() error: ", errno);
            closeDataConnection ();
                                                      return F ("425 can't open pasive data connection\r\n");                
          }
          // make socket not-blocking so that time-out can be detected
          if (fcntl (__dataConnectionSocket__, F_SETFL, O_NONBLOCK) == -1) {
            dmesg ("[ftpPasiveDataConnection] fcntl() error: ", errno);
            closeDataConnection ();
                                                      return F ("425 can't open pasive data connection\r\n");    
          }

          // everything is ready now to accept a connection from FTP client, we have to tell it how to connect
              
          String s = "227 entering passive mode (" + String (ip1) + "," + String (ip2) + "," + String (ip3) + "," + String (ip4) + "," + String (p1) + "," + String (p2) + ")\r\n";
          if (sendAll (__controlConnectionSocket__, (char *) s.c_str (), s.length (), FTP_CONTROL_CONNECTION_TIME_OUT) == -1) { 
            closeDataConnection ();
                                                      return ""; // error in control connection
          }

          // wait for a connection from the client

          unsigned long lastActive = millis ();
          int connectingSocket = -1;
          struct sockaddr_in connectingAddress;
          socklen_t connectingAddressSize = sizeof (connectingAddress);
          while (connectingSocket == -1 && millis () - lastActive < FTP_DATA_CONNECTION_TIME_OUT) {
            delay (1);
            connectingSocket = accept (__dataConnectionSocket__, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
          }
          if (connectingSocket == -1) {
            dmesg ("[ftpPasiveDataConnection] accept() time-out");
            closeDataConnection ();
            return ""; // no error/success replies any more // return F ("425 can't open pasive data connection\r\n");                
          }

          // close the listener and get connectingSocket ready

          int listeningSocket;
          xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
            listeningSocket = __dataConnectionSocket__;
            __dataConnectionSocket__ = connectingSocket;
          xSemaphoreGive (__ftpServerSemaphore__);
          if (listeningSocket > -1) close (listeningSocket);   
          // make the socket not-blocking so that time-out can be detected
          if (fcntl (__dataConnectionSocket__, F_SETFL, O_NONBLOCK) == -1) {
            dmesg ("[ftpPasiveDataConnection] fcntl() error: ", errno);
            closeDataConnection ();              
            return ""; // no error/success replies any more // return F ("425 can't open pasive data connection\r\n"); // it is probabbly too late to report this issue now
          }
           
          return ""; // nothing to send to FTP client any more regarding PASV command
        }

        // NLST is preceeded by PORT or PASV so data connection should already be opened
        String __NLST__ (char *directoryName) { 
          if (!*__homeDir__)                          { closeDataConnection (); return F ("530 not logged in\r\n"); }
          if (!__fileSystemMounted__)                 { closeDataConnection (); return F ("421 file system not mounted\r\n"); }
          String fp = fullFilePath (directoryName, __workingDir__);
          if (fp == "" || !isDirectory (fp))          { closeDataConnection (); return F ("501 invalid directory name\r\n"); }
          if (!ftpUserHasRightToAccess (fp.c_str ())) { closeDataConnection (); return F ("550 access denyed\r\n"); }

          if (__dataConnectionSocket__ == -1)         return F ("425 can't open data connection\r\n");    

          if (sendAll (__controlConnectionSocket__, (char *) "150 starting data transfer\r\n", strlen ("150 starting data transfer\r\n"), FTP_CONTROL_CONNECTION_TIME_OUT) == -1) { closeDataConnection (); return ""; } // if control connection is closed

            String s = listDirectory (fp); if (s > "") s += "\r\n";
            int bytesWritten = sendAll (__dataConnectionSocket__, (char *) s.c_str (), s.length (), FTP_DATA_CONNECTION_TIME_OUT);
            closeDataConnection ();
  
          if (bytesWritten == s.length ())            return F ("226 data transfer complete\r\n");
                                                      return F ("425 data transfer error\r\n");
        }
  
        String __RETR__ (char *fileName) { 
          if (!*__homeDir__)                          { closeDataConnection (); return F ("530 not logged in\r\n"); }
          if (!__fileSystemMounted__)                 { closeDataConnection (); return F ("421 file system not mounted\r\n"); }
          String fp = fullFilePath (fileName, __workingDir__);
          if (fp == "" || !isFile (fp))               { closeDataConnection (); return F ("501 invalid file name\r\n"); }
          if (!ftpUserHasRightToAccess (fp.c_str ())) { closeDataConnection (); return F ("550 access denyed\r\n"); }

          if (__dataConnectionSocket__ == -1)         return F ("425 can't open data connection\r\n");    

          if (sendAll (__controlConnectionSocket__, (char *) "150 starting data transfer\r\n", strlen ("150 starting data transfer\r\n"), FTP_CONTROL_CONNECTION_TIME_OUT) == -1) { closeDataConnection (); return ""; } // if control connection is closed

            int bytesReadTotal = 0; int bytesSentTotal = 0;
            File f = fileSystem.open (fp, FILE_READ);
            if (f) {
              // read data from file and transfer it through data connection
              #define BUFF_SIZE TCP_SND_BUF // TCP_SND_BUF = 5744, a maximum block size that ESP32 can send 
              char buff [BUFF_SIZE];
              do {
                int bytesReadThisTime = f.read ((uint8_t *) buff, BUFF_SIZE);
                if (bytesReadThisTime == 0) { break; } // finished, success
                bytesReadTotal += bytesReadThisTime;
                int bytesSentThisTime = sendAll (__dataConnectionSocket__, buff, bytesReadThisTime, FTP_DATA_CONNECTION_TIME_OUT);
                if (bytesSentThisTime != bytesReadThisTime) { bytesSentTotal = -1; break; } // to detect error later
                bytesSentTotal += bytesSentThisTime;
              } while (true);
              f.close ();
            } else {
              bytesSentTotal = -1; // to detect error later
            }
  
            closeDataConnection ();
  
            #ifdef __PERFMON__
              __perfFSBytesRead__ += bytesReadTotal; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif
            
          if (bytesSentTotal == bytesReadTotal)       return F ("226 data transfer complete\r\n");
                                                      return F ("550 data transfer error\r\n");  
        }

        String __STOR__ (char *fileName) { 
          if (!*__homeDir__)                          { closeDataConnection (); return F ("530 not logged in\r\n"); }
          if (!__fileSystemMounted__)                 { closeDataConnection (); return F ("421 file system not mounted\r\n"); }
          String fp = fullFilePath (fileName, __workingDir__);
          if (fp == "" || isDirectory (fp))           { closeDataConnection (); return F ("501 invalid file name\r\n"); }
          if (!ftpUserHasRightToAccess (fp.c_str ())) { closeDataConnection (); return F ("550 access denyed\r\n"); }

          if (__dataConnectionSocket__ == -1)         return F ("425 can't open data connection\r\n");    

          if (sendAll (__controlConnectionSocket__, (char *) "150 starting data transfer\r\n", strlen ("150 starting data transfer\r\n"), FTP_CONTROL_CONNECTION_TIME_OUT) == -1) { closeDataConnection (); return ""; } // if control connection is closed

            int bytesRecvTotal = 0; int bytesWrittenTotal = 0;
            File f = fileSystem.open (fp, FILE_WRITE);
            if (f) {
              // read data from data connection and store it to the file
              #define BUFF_SIZE TCP_SND_BUF // TCP_SND_BUF = 5744, a maximum block size that ESP32 can send 
              char buff [BUFF_SIZE];
              do {
                int bytesRecvThisTime = recvAll (__dataConnectionSocket__, buff, BUFF_SIZE, NULL, FTP_DATA_CONNECTION_TIME_OUT);
                if (bytesRecvThisTime < 0)  { bytesRecvTotal = -1; break; } // to detect error later
                if (bytesRecvThisTime == 0) { break; } // finished, success
                bytesRecvTotal += bytesRecvThisTime;
                int bytesWrittenThisTime = f.write ((uint8_t *) buff, bytesRecvThisTime);
                bytesWrittenTotal += bytesWrittenThisTime;
                if (bytesWrittenThisTime != bytesRecvThisTime) { bytesRecvTotal = -1; break; } // to detect error later
              } while (true);
              f.close ();
            } else {
              bytesRecvTotal = -1; // to detect error later
            }

            closeDataConnection ();
  
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += bytesWrittenTotal; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif
                        
          if (bytesWrittenTotal == bytesRecvTotal)    return F ("226 data transfer complete\r\n");
                                                      return F ("550 data transfer error\r\n");    
        }

        String __XMKD__ (char *directoryName) { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                 return F ("421 file system not mounted\r\n"); 
          String fp = fullFilePath (directoryName, __workingDir__);
          if (fp == "")                               return F ("501 invalid directory name\r\n"); 
          if (!ftpUserHasRightToAccess (fp.c_str ())) return F ("550 access denyed\r\n"); 
  
          if (makeDir (fp))                           return F ("257 directory created\r\n");
                                                      return F ("550 could not create directory\r\n"); 
        }

        String __XRMD__ (String fileOrDirName) { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                 return F ("421 file system not mounted\r\n"); 
          String fp = fullFilePath (fileOrDirName, __workingDir__);
          if (fp == "")                               return F ("501 invalid file or directory name\r\n"); 
          if (!ftpUserHasRightToAccess (fp.c_str ())) return F ("550 access denyed\r\n"); 

          if (isFile (fp)) {
            if (deleteFile (fp))                      return F ("250 file deleted\r\n");
                                                      return F ("452 could not delete file\r\n");
          } else {
            if (fp == String (__homeDir__))           return F ("550 you can't remove your home directory\r\n");
            if (fp == String (__workingDir__))        return F ("550 you can't remove your working directory\r\n");
            if (removeDir (fp))                       return F ("250 directory removed\r\n");
                                                      return F ("452 could not remove directory\r\n");
          }
        }

        char __rnfrPath__ [FILE_PATH_MAX_LENGTH + 1];

        String __RNFR__ (char *fileOrDirName) { 
          if (!*__homeDir__)                          return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                 return F ("421 file system not mounted\r\n"); 
          String fp = fullFilePath (fileOrDirName, __workingDir__);
          if (fp == "")                               return F ("501 invalid file or directory name\r\n"); 
          if (!ftpUserHasRightToAccess (fp.c_str ())) return F ("550 access denyed\r\n"); 

          // save temporal result
          strncpy (__rnfrPath__, (char *) fp.c_str (), FILE_PATH_MAX_LENGTH);
          __rnfrPath__ [FILE_PATH_MAX_LENGTH] = 0;

                                                      return F("350 need more information\r\n"); // RNTO command will follow
        }
          
        String __RNTO__ (char *fileOrDirName) { 
          if (!*__homeDir__)                                    return F ("530 not logged in\r\n");
          if (!__fileSystemMounted__)                           return F ("421 file system not mounted\r\n"); 
          String fp = fullFilePath (fileOrDirName, __workingDir__);
          if (fp == "")                                         return F ("501 invalid file or directory name\r\n"); 
          if (!ftpUserHasRightToAccess (fp.c_str ()))           return F ("550 access denyed\r\n"); 

          // rename file from temporal result
          if (fileSystem.rename (__rnfrPath__, (char *) fp.c_str ())) return F ("250 renamed\r\n");
                                                                return F ("553 unable to rename\r\n");          
        }
          
    };


    // ----- ftpServer class -----

    class ftpServer {                                             
    
      public:

        // ftpServer state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          STARTING = 1, 
          RUNNING = 2        
        };
        
        STATE_TYPE state () { return __state__; }
    
        ftpServer (  // the following parameters will be handeled by ftpServer instance
                     char *serverIP,                                                                // FTP server IP address, 0.0.0.0 for all available IP addresses
                     int serverPort,                                                                // FTP server port
                     bool (*firewallCallback) (char *connectingIP)                                  // a reference to callback function that will be celled when new connection arrives 
                   )  { 
                        // create a local copy of parameters for later use
                        strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0;
                        __serverPort__ = serverPort;
                        __firewallCallback__ = firewallCallback;

                        // start listener in its own thread (task)
                        __state__ = STARTING;                        
                        #define tskNORMAL_PRIORITY 1
                        if (pdPASS != xTaskCreate (__listenerTask__, "ftpServer", FTP_SERVER_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL)) {
                          dmesg ("[ftpServer] xTaskCreate error");
                        } else {
                          // wait until listener starts accepting connections
                          while (__state__ == STARTING) delay (1); 
                          // when constructor returns __state__ could be eighter RUNNING (in case of success) or NOT_RUNNING (in case of error)
                        }
                      }
        
        ~ftpServer () {
                          // close listening socket
                          int listeningSocket;
                          xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                            listeningSocket = __listeningSocket__;
                            __listeningSocket__ = -1;
                          xSemaphoreGive (__ftpServerSemaphore__);
                          if (listeningSocket > -1) close (listeningSocket);

                          // wait until listener finishes before unloading so that memory variables are still there while the listener is running
                          while (__state__ != NOT_RUNNING) delay (1);
                      }
 
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        char __serverIP__ [46] = "0.0.0.0";
        int __serverPort__ = 21;
        bool (* __firewallCallback__) (char *connectingIP) = NULL;

        int __listeningSocket__ = -1;

        static void __listenerTask__ (void *pvParameters) {
          {
            // get "this" pointer
            ftpServer *ths = (ftpServer *) pvParameters;  
    
            // start listener
            ths->__listeningSocket__ = socket (PF_INET, SOCK_STREAM, 0);
            if (ths->__listeningSocket__ == -1) {
              dmesg ("[ftpServer] socket error: ", errno);
            } else {
              // make address reusable - so we won't have to wait a few minutes in case server will be restarted
              int flag = 1;
              if (setsockopt (ths->__listeningSocket__, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag)) == -1) {
                dmesg ("[ftpServer] setsockopt error: ", errno);
              } else {
                // bind listening socket to IP address and port     
                struct sockaddr_in serverAddress; 
                memset (&serverAddress, 0, sizeof (struct sockaddr_in));
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_addr.s_addr = inet_addr (ths->__serverIP__);
                serverAddress.sin_port = htons (ths->__serverPort__);
                if (bind (ths->__listeningSocket__, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
                  dmesg ("[ftpServer] bind error: ", errno);
               } else {
                 // mark socket as listening socket
                 #define BACKLOG 5
                 if (listen (ths->__listeningSocket__, TCP_LISTEN_BACKLOG) == -1) {
                  dmesg ("[ftpServer] listen error: ", errno);
                 } else {
          
                  // listener is ready for accepting connections
                  ths->__state__ = RUNNING;
                  dmesg ("[ftpServer] started");
                  while (ths->__listeningSocket__ > -1) { // while listening socket is opened
          
                      int connectingSocket;
                      struct sockaddr_in connectingAddress;
                      socklen_t connectingAddressSize = sizeof (connectingAddress);
                      connectingSocket = accept (ths->__listeningSocket__, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
                      if (connectingSocket == -1) {
                        if (ths->__listeningSocket__ > -1) dmesg ("[ftpServer] accept error: ", errno);
                      } else {
                        // get client's IP address
                        char clientIP [46]; inet_ntoa_r (connectingAddress.sin_addr, clientIP, sizeof (clientIP)); 
                        // get server's IP address
                        char serverIP [46]; struct sockaddr_in thisAddress = {}; socklen_t len = sizeof (thisAddress);
                        if (getsockname (connectingSocket, (struct sockaddr *) &thisAddress, &len) != -1) inet_ntoa_r (thisAddress.sin_addr, serverIP, sizeof (serverIP));
                        // port number could also be obtained if needed: ntohs (thisAddress.sin_port);
                        if (ths->__firewallCallback__ && !ths->__firewallCallback__ (clientIP)) {
                          dmesg ("[ftpServer] firewall rejected connection from ", clientIP);
                          close (connectingSocket);
                        } else {
                          // make the socket non-blocking so that we can detect time-out
                          if (fcntl (connectingSocket, F_SETFL, O_NONBLOCK) == -1) {
                            dmesg ("[ftpServer] fcntl error: ", errno);
                            close (connectingSocket);
                          } else {
                                // create ftpControlConnection instence that will handle the connection, then we can lose reference to it - ftpControlConnection will handle the rest
                                ftpControlConnection *fcn = new ftpControlConnection (connectingSocket, 
                                                                                      clientIP, serverIP);
                                if (!fcn) {
                                  // dmesg ("[ftpServer] new ftpControlConnection error");
                                  sendAll (connectingSocket, ftpServiceUnavailable, strlen (ftpServiceUnavailable), FTP_CONTROL_CONNECTION_TIME_OUT);
                                  close (connectingSocket); // normally ftpControlConnection would do this but if it is not created we have to do it here
                                } else {
                                  if (fcn->state () != ftpControlConnection::RUNNING) {
                                    sendAll (connectingSocket, ftpServiceUnavailable, strlen (ftpServiceUnavailable), FTP_CONTROL_CONNECTION_TIME_OUT);
                                    delete (fcn); // normally ftpControlConnection would do this but if it is not running we have to do it here
                                  }
                                }
                                                               
                          } // fcntl
                        } // firewall
                      } // accept
                      
                  } // while accepting connections
                  dmesg ("[ftpServer] stopped");
          
                 } // listen
               } // bind
              } // setsockopt
              int listeningSocket;
              xSemaphoreTake (__ftpServerSemaphore__, portMAX_DELAY);
                listeningSocket = ths->__listeningSocket__;
                ths->__listeningSocket__ = -1;
              xSemaphoreGive (__ftpServerSemaphore__);
              if (listeningSocket > -1) close (listeningSocket);
            } // socket
            ths->__state__ = NOT_RUNNING;
          }
          // stop the listening thread (task)
          vTaskDelete (NULL); // it is listener's responsibility to close itself          
        }
          
    };
    
#endif
