/*
 * 
 * FtpServer.hpp 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 *  FtpServer is built upon TcpServer with connectionHandler that handles TcpConnection according to FTP protocol.
 *  This goes for control connection at least. FTP is a little more complicated since it uses another TCP connection
 *  for data transfer. Beside that, data connection can be initialized by FTP server or FTP client and it is FTP client's
 *  responsibility to decide which way it is going to be.
 * 
 * History:
 *          - first release, 
 *            November 18, 2018, Bojan Jurca
 *          - added SPIFFSsemaphore and SPIFFSsafeDelay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca
 *          - introduction of FTP_FILE_TIME definition
 *            August, 25, 2019, Bojan Jurca
 *          - minor structural changes,
 *            the use of dmesg
 *            September 14, 2019, Bojan Jurca
 *          - replaced gmtime () function that returns ponter to static structure with reentrant solution
 *            October 29, 2019, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca 
 *  
 */


#ifndef __FTP_SERVER__
  #define __FTP_SERVER__

  #ifndef HOSTNAME
    #define HOSTNAME "MyESP32Server" // WiFi.getHostname() // use default if not defined
  #endif

  #include <WiFi.h>

  int __pasiveDataPort__ () {
    static portMUX_TYPE csFtpPasiveDataPort = portMUX_INITIALIZER_UNLOCKED;
    static int lastPasiveDataPort = 1024;                                               // change pasive data port range if needed
    portENTER_CRITICAL (&csFtpPasiveDataPort);
    int pasiveDataPort = lastPasiveDataPort = (((lastPasiveDataPort + 1) % 16) + 1024); // change pasive data port range if needed
    portEXIT_CRITICAL (&csFtpPasiveDataPort);
    return pasiveDataPort;
  }

  void __ftpDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__ // use dmesg from telnet server if possible
      dmesg (message);
    #else
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
    #endif
  }
  void (* ftpDmesg) (String) = __ftpDmesg__; // use this pointer to display / record system messages

  #include "TcpServer.hpp"        // FTP server.hpp is built upon TcpServer.hpp  
  #include "real_time_clock.hpp"  // FTP server needs time to report file time (in ls), but it is always a fiction since SPIFFS doesn't record file creation time at all
    #ifndef FTP_RTC                 // if not defined earlier define it now but it will only make code to compile, not to work properly
      real_time_clock __FTP_RTC__ ((char *) "", (char *) "", (char *) "");
      #define FTP_RTC __FTP_RTC__
    #endif
  #include "file_system.h"        // ftpServer.hpp needs file_system.h
  #include "user_management.h"    // ftpServer.hpp needs user_management.h


  class ftpServer: public TcpServer {                                             
  
    public:
  
      ftpServer (char *serverIP,                                       // FTP server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                 int serverPort,                                       // FTP server port
                 bool (* firewallCallback) (char *)                    // a reference to callback function that will be celled when new connection arrives 
                ): TcpServer (__ftpConnectionHandler__, NULL, 8192, 300000, serverIP, serverPort, firewallCallback)
                                                {
                                                  if (started ()) ftpDmesg ("[ftpServer] started on " + String (serverIP) + ":" + String (serverPort) + (firewallCallback ? " with firewall." : "."));
                                                  else            ftpDmesg ("[ftpServer] couldn't start.");
                                                }
      
      ~ftpServer ()                             { if (started ()) ftpDmesg ("[ftpServer] stopped."); }
                                                      
    private:

      static void __ftpConnectionHandler__ (TcpConnection *connection, void *notUsed) {  // connectionHandler callback function
        // log_i ("[Thread:%i][Core:%i] connection has started\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID ()); 
        char buffer [160];                  // make sure there is enough space for each type of use but be modest - this buffer uses thread stack
        TcpClient *activeDataClient = NULL; // non-threaded TCP client will be used for handling active data connections
        TcpServer *pasiveDataServer = NULL; // non-threaded TCP server will be used for handling pasive data connections
        char user [33]; *user = 0;          // store the name of the user that has logged in here 
        bool loggedIn = false;              // "logged in" flag
        char homeDir [33]; *homeDir = 0;    // store home directory of the user that has logged in here
        char fileName [33];                 // define once here, will be used in several places of this function latter
        File file;                          // define once here, will be used in several places of this function latter

        if (!__fileSystemMounted__) {
          connection->sendData ("220-" + String (HOSTNAME) + " SPIFFS file system not mounted. You may have to use telnet and mkfs.spiffs to format flash disk first.");
          goto closeFtpConnection;
          return;
        }
  
        #if USER_MANAGEMENT == NO_USER_MANAGEMENT
          if (!connection->sendData ("220-" + String (HOSTNAME) + " FTP server - everyone is allowed to login\r\n220 \r\n")) goto closeFtpConnection;
        #else
          if (!connection->sendData ("220-" + String (HOSTNAME) + " FTP server - please login\r\n220 \r\n")) goto closeFtpConnection;
        #endif  
        
        while (true) { // read and process incomming commands in an endless loop
          int receivedTotal = 0;
          #define ftpCmd buffer
          char *endOfCmd;
          char *ftpParam;
          do {
            int received;
            buffer [receivedTotal += (received = connection->recvData (buffer + receivedTotal, sizeof (buffer) - receivedTotal - 1))] = 0; // read incomming characters and mark the end with 0
            if (!received) goto closeFtpConnection;
          } while (!(endOfCmd = strstr (buffer, "\r\n")));
          *endOfCmd = 0; // mark the end of received command
          // log_v ("[Thread:%lu][Core:%i] new command = %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), buffer);
          if ((ftpParam = strstr (buffer, " "))) *ftpParam++ = 0; else ftpParam = endOfCmd; // mark the end of command and the beginning of parameter
          
          if (!strcmp        (ftpCmd, "USER")) {  // ---------- USER ----------
            
                if (strlen (ftpParam) < sizeof (user)) strcpy (user, ftpParam); else *user = *homeDir = 0;
                loggedIn = false;
                connection->sendData ((char *) "331 enter password\r\n");
          
          } else if (!strcmp (ftpCmd, "PASS")) {  // ---------- PASS ----------
            
                if (checkUserNameAndPassword (user, ftpParam)) getUserHomeDirectory (homeDir, user);
                if (*homeDir) { 
                  loggedIn = true; 
                  ftpDmesg ("[ftpServer] " + String (user) + " logged in.");
                  sprintf (buffer, "230 logged on, use \"/\" to refer to your home directory \"%s\", cd command is not supported\r\n", homeDir);
                  connection->sendData (buffer);
                } else { 
                  ftpDmesg ("[ftpServer] " + String (user) + " login attempt failed.");
                  connection->sendData ((char *) "530 user name or password incorrect\r\n"); 
                }
          
          } else if (!strcmp (ftpCmd, "CWD")) {   // ---------- CWD ----------          // "cd directory" command
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                if (!strcmp (ftpParam, "/")) {
                  sprintf (buffer, "250 current directory is \"/\" which referes to your home directory \"%s\"\r\n", homeDir);
                  connection->sendData (buffer);
                } else {
                  connection->sendData ((char *) "550 path not found\r\n");
                }
          
          } else if (!strcmp (ftpCmd, "PWD")) {      // ---------- PWD ----------          // we have just one directory
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                sprintf (buffer, "257 use \"/\" to refer to your home directory \"%s\", cd command is not supported\r\n", homeDir);
                connection->sendData (buffer);
          
          } else if (!strcmp (ftpCmd, "TYPE")) {     // ---------- TYPE ----------         // just pretend we have done it
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                connection->sendData ((char *) "200 ok\r\n");      
          
          } else if (!strcmp (ftpCmd, "SYST")) {     // ---------- SYST ----------         // pretend this is a UNIX system
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                connection->sendData ((char *) "215 UNIX Type: L8\r\n");
          
          } else if (!strcmp (ftpCmd, "SIZE")) {     // ---------- SIZE ----------      // just report 0, it will do
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                connection->sendData ((char *) "213 0\r\n");
          
          } else if (!strcmp (ftpCmd, "PASV")) {     // ---------- PASV ----------      // switch to pasive mode, next command (NLST, LIST, RETR STOR) will follow shortly
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                int ip1, ip2, ip3, ip4, p1, p2; // get (this) server IP and (hopefully) next free port
                if (4 == sscanf (connection->getThisSideIP (), "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
                  char reply [50];
                  int pasiveDataPort = __pasiveDataPort__ ();
                  p2 = pasiveDataPort % 256;
                  p1 = pasiveDataPort / 256;
                          sprintf (reply, "227 entering passive mode (%i,%i,%i,%i,%i,%i)\r\n", ip1, ip2, ip3, ip4, p1, p2);
                  pasiveDataServer = new TcpServer (5000, connection->getThisSideIP (), pasiveDataPort, NULL); // open a new TCP server to accept pasive data connection
                  connection->sendData (reply); // send reply
                } else {
                    connection->sendData ((char *) "425 can't open data connection\r\n");
                }
          
          } else if (!strcmp (ftpCmd, "PORT")) {     // ---------- PORT ----------     // switch to active mode, next command (NLST, LIST, RETR STOR) will follow shortly
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                int ip1, ip2, ip3, ip4, p1, p2; // get client IP and port
                if (6 == sscanf (ftpParam, "%i,%i,%i,%i,%i,%i", &ip1, &ip2, &ip3, &ip4, &p1, &p2)) {
                  char activeDataIP [16];
                  int activeDataPort;
                  sprintf (activeDataIP, "%i.%i.%i.%i", ip1, ip2, ip3, ip4); 
                  activeDataPort = 256 * p1 + p2;
                  activeDataClient = new TcpClient (activeDataIP, activeDataPort, 5000); // open a new TCP client for active data connection
                } 
                connection->sendData ((char *) "200 port ok\r\n"); 
          
          } else if (!strcmp (ftpCmd, "NLST") || !strcmp (ftpCmd, "LIST")) {  // ---------- LIST ---------- // "ls" or "dir" command requires ASCII mode data transfer to the client - the content is a list of file names
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                if (!connection->sendData ((char *) "150 starting transfer\r\n")) goto closeFtpConnection; 
                  // list file as UNIX does
                  String s = "";
                  char d [33]; strcpy (d, homeDir); if (strlen (d) > 1 && *(d + strlen (d) - 1) == '/') *(d + strlen (d) - 1) = 0;
                  xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
                  File dir = SPIFFS.open (d);
                  if (!dir) {
                    // log_e ("failed to open directory %s\n", homeDir);
                    s = "\r\n";
                  } else {
                    if (!dir.isDirectory ()) {
                      // log_e ("%s is not a directory\n", homeDir);
                      s = "\r\n";
                    } else {
                      File file = dir.openNextFile ();
                      while (file) {
                        if(!file.isDirectory ()) {
                          char c [60];
                          sprintf (c, "  %6i", file.size ());
                          time_t rawNow;
                          #ifdef FTP_FILE_TIME // backward compatibility
                            rawNow = FTP_FILE_TIME; // SPIFFS does't record file time so this time is is a fiction
                          #else
                            if (FTP_RTC.isGmtTimeSet ())
                              rawNow = FTP_RTC.getLocalTime (); // SPIFFS does't record file time so this time is is a fiction
                            else
                              rawNow = 1572387378; // right the time I'm writting this code, it is a fiction anyway
                          #endif
                          struct tm structNow = timeToStructTime (&rawNow);
                          // char *month [12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dec"};
                          // sprintf (c + strlen (c), " %s %02i %02i:%02i      ", month [structNow.tm_mon], structNow.tm_mday, structNow.tm_hour, structNow.tm_min);
                          strftime (c + strlen (c), 25, " %b %lu %H:%M      ", &structNow);
                          s += String ("-r-xr-xrwx   1 owner    group        ") + String (c) + String (file.name ()) + "\r\n";
                        }
                        file = dir.openNextFile ();
                      }
                      if (s == "") s = "\r\n"; // we don't want to return empty string - it would make it harder to detect errors
                    }
                  }
                  xSemaphoreGive (SPIFFSsemaphore);
                  // s contains now UNIX like lsit of files in homeDir

                  int written = 0;
                  TcpConnection *dataConnection = NULL;
                  if (pasiveDataServer) while (!(dataConnection = pasiveDataServer->connection ()) && !pasiveDataServer->timeOut ()) SPIFFSsafeDelay (1); // wait until a connection arrives to non-threaded server or time-out occurs
                  if (activeDataClient) dataConnection = activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all
                  if (dataConnection) written = dataConnection->sendData (s);
                  if (activeDataClient) { delete (activeDataClient); activeDataClient = NULL; }
                  if (pasiveDataServer) { delete (pasiveDataServer); pasiveDataServer = NULL; }
                if (written) connection->sendData ((char *) "226 transfer complete\r\n");
                else         connection->sendData ((char *) "425 can't open data connection\r\n");
          
          } else if (!strcmp (ftpCmd, "XRMD")) {     // ---------- XRMD ----------     // "rm filename" command
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                if (*ftpParam == '/') ftpParam++; // trim possible prefix
                
                xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
                
                if (strlen (homeDir) + strlen (ftpParam) < sizeof (fileName) && sprintf (fileName, "%s%s", homeDir, ftpParam) && SPIFFS.remove (fileName)) sprintf (buffer, "250 %s deleted\r\n", fileName);
                else                                                                                                                                       sprintf (buffer, "452 file could not be deleted\r\n");
                
                xSemaphoreGive (SPIFFSsemaphore);                
                
                connection->sendData (buffer);
          
          } else if (!strcmp (ftpCmd, "RETR")) {     // ---------- RETR ----------     // "get filename" command
            
                if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                if (*ftpParam == '/') ftpParam++; // trim possible prefix
                int bytesWritten = -1; int bytesRead = 0;
                if (!connection->sendData ((char *) "150 starting transfer\r\n")) goto closeFtpConnection; 
                  TcpConnection *dataConnection = NULL;
                  if (pasiveDataServer) while (!(dataConnection = pasiveDataServer->connection ()) && !pasiveDataServer->timeOut ()) SPIFFSsafeDelay (1); // wait until a connection arrives to non-threaded server or time-out occurs
                  if (activeDataClient) dataConnection = activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all
                  if (dataConnection) {
                    if (strlen (homeDir) + strlen (ftpParam) < sizeof (fileName) && sprintf (fileName, "%s%s", homeDir, ftpParam)) {
                      
                      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
                      
                      if ((bool) (file = SPIFFS.open (fileName, FILE_READ))) {
                        if (!file.isDirectory ()) {
                          byte *buff = (byte *) malloc (2048); // get 2048 B of memory from heap (not from the stack)
                          if (buff) {
                            int i = bytesWritten = 0;
                            while (file.available ()) {
                              *(buff + i++) = file.read ();
                              if (i == 2048) { bytesRead += 2048; bytesWritten += dataConnection->sendData ((char *) buff, 2048); i = 0; }
                            }
                            if (i) { bytesRead += i; bytesWritten += dataConnection->sendData ((char *) buff, i); }
                            free (buff);
                          }
                        }
                        file.close ();
                      }
                      
                      xSemaphoreGive (SPIFFSsemaphore);
                      
                    }
                  }
                  if (activeDataClient) { delete (activeDataClient); activeDataClient = NULL; }
                  if (pasiveDataServer) { delete (pasiveDataServer); pasiveDataServer = NULL; }
                if (bytesWritten == bytesRead) sprintf (buffer, "226 %s transfer complete\r\n", fileName);
                else                           sprintf (buffer, "550 file could not be transfered\r\n");
                connection->sendData (buffer, strlen (buffer));
          
          } else if (!strcmp (ftpCmd, "STOR")) {     // ---------- STOR ----------     // "put filename" command
            
              if (!loggedIn) goto closeFtpConnection; // someone is not playing by the rules
                if (*ftpParam == '/') ftpParam++; // trim possible prefix
                int bytesRead = -1; int bytesWritten = 0;
                if (!connection->sendData ((char *) "150 starting transfer\r\n")) goto closeFtpConnection; 
                  TcpConnection *dataConnection = NULL;
                  if (pasiveDataServer) while (!(dataConnection = pasiveDataServer->connection ()) && !pasiveDataServer->timeOut ()) SPIFFSsafeDelay (1); // wait until a connection arrives to non-threaded server or time-out occurs
                  if (activeDataClient) dataConnection = activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all
                  if (dataConnection) {
                    if (strlen (homeDir) + strlen (ftpParam) < sizeof (fileName) && sprintf (fileName, "%s%s", homeDir, ftpParam)) {
                      
                      xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
                      
                      if ((bool) (file = SPIFFS.open (fileName, FILE_WRITE))) {
                        byte *buff = (byte *) malloc (2048); // get 2048 B of memory from heap (not from the stack)
                        if (buff) {
                          bytesRead = 0;
                          int received;
                          do {
                            bytesRead += received = dataConnection->recvData ((char *) buff, 2048);
                            if (received && file.write (buff, received) == received) bytesWritten += received;
                          } while (received);
                          free (buff);
                        }
                        file.close ();
                      }
                      
                      xSemaphoreGive (SPIFFSsemaphore);
                      
                    }
                  }
                  if (activeDataClient) { delete (activeDataClient); activeDataClient = NULL; }
                  if (pasiveDataServer) { delete (pasiveDataServer); pasiveDataServer = NULL; }
                if (bytesRead == bytesWritten) sprintf (buffer, "226 %s transfer complete\r\n", fileName);
                else                           sprintf (buffer, "452 not enough space or other error\r\n");
                connection->sendData (buffer);
          
          } else if (!strcmp (ftpCmd, "QUIT")) {     // ---------- QUIT ----------     // do not respond ...
            
                if (activeDataClient) delete (activeDataClient);                        // ... close active data connection it it is opened ...
                if (pasiveDataServer) delete (pasiveDataServer);                        // ... close pasive data connection it it is opened ...
                goto closeFtpConnection;                                                // ... close control connection by returning
          
          } else {
                connection->sendData ((char *) "220 unsupported command\r\n");
          }
        }
      closeFtpConnection:
        // log_i ("[Thread:%i][Core:%i] connection has ended\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID ()); 
        if (loggedIn) ftpDmesg ("[ftpServer] " + String (user) + " logged out.");
      }
      
  };

#endif
