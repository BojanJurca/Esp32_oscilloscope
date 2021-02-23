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
 *          - added fileSystemSemaphore and delay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca
 *          - introduction of FTP_FILE_TIME definition
 *            August, 25, 2019, Bojan Jurca
 *          - replaced gmtime () function that returns pointer to static structure with reentrant solution
 *            October 29, 2019, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca 
 *          - port from SPIFFS to FAT file system, adjustment for Arduino 1.8.13
 *            added FTP commands (pwd, cd, mkdir, rmdir, ...)
 *            October 31, 2020, Bojan Jurca
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

  #include "TcpServer.hpp"        // ftpServer.hpp is built upon TcpServer.hpp  
  #include "file_system.h"        // ftpServer.hpp needs file_system.h
  #include "user_management.h"    // ftpServer.hpp needs user_management.h


  class ftpServer: public TcpServer {                                             
  
    public:

      // keep ftp session parameters in a structure
      typedef struct {
        String userName;
        String homeDir;
        String workingDir;
        TcpConnection *controlConnection; // ftp control connection
        TcpServer *pasiveDataServer;      // for ftp pasive mode data connection
        TcpClient *activeDataClient;      // for ftp active mode data connection
        // feel free to add more
      } ftpSessionParameters;        
  
      ftpServer (char *serverIP,                                       // FTP server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                 int serverPort,                                       // FTP server port
                 bool (* firewallCallback) (char *)                    // a reference to callback function that will be celled when new connection arrives 
                ): TcpServer (__ftpConnectionHandler__, (void *) this, 8 * 1024, 300000, serverIP, serverPort, firewallCallback)
                                                {
                                                  if (started ()) ftpDmesg ("[ftpServer] started on " + String (serverIP) + ":" + String (serverPort) + (firewallCallback ? " with firewall." : "."));
                                                  else            ftpDmesg ("[ftpServer] couldn't start.");
                                                }
      
      ~ftpServer ()                             { if (started ()) ftpDmesg ("[ftpServer] stopped."); }
                                                      
    private:

      static void __ftpConnectionHandler__ (TcpConnection *connection, void *thisFtpServer) { // connectionHandler callback function

        // this is where ftp session begins
        
        ftpServer *ths = (ftpServer *) thisFtpServer; // we've got this pointer into static member function
        ftpSessionParameters fsp = {"", "", "", connection, NULL, NULL};
        
        String cmdLine;

        if (!__fileSystemMounted__) {
          connection->sendData ("220-" + String (HOSTNAME) + " file system file system not mounted. You may have to use telnet and mkfs.fat to format flash disk first.");
          goto closeFtpConnection;
        }
  
        #if USER_MANAGEMENT == NO_USER_MANAGEMENT
          if (!connection->sendData ("220-" + String (HOSTNAME) + " FTP server - everyone is allowed to login\r\n220 \r\n")) goto closeFtpConnection;
        #else
          if (!connection->sendData ("220-" + String (HOSTNAME) + " FTP server - please login\r\n220 \r\n")) goto closeFtpConnection;
        #endif  

        while (13 == __readLineFromClient__ (&cmdLine, connection)) { // read and process comands in a loop        

          if (cmdLine.length ()) {

            // ----- parse command line into arguments (max 32) -----
            
            int argc = 0; String argv [8] = {"", "", "", "", "", "", "", ""}; 
            argv [0] = String (cmdLine); argv [0].trim ();
            String param = ""; // everything behind argv [0]
            if (argv [0] != "") {
              argc = 1;
              while (argc < 8) {
                int l = argv [argc - 1].indexOf (" ");
                if (l > 0) {
                  argv [argc] = argv [argc - 1].substring (l + 1);
                  argv [argc].trim ();
                  if (argc == 1) param = argv [argc]; // remember the whole parameter line in its original form
                  argv [argc - 1] = argv [argc - 1].substring (0, l); // no need to trim
                  argc ++;
                } else {
                  break;
                }
              }
            }

              //debug FTP protocol: Serial.print ("<--"); for (int i = 0; i < argc; i++) Serial.print (argv [i] + " "); Serial.println ();

            // ----- try to handle ftp command -----
            String s = ths->__internalFtpCommandHandler__ (argc, argv, param, &fsp);
            connection->sendData (s); // send reply to telnet client
              
              //debug FTP protocol: Serial.println ("-->" + s);

          } // if cmdLine is not empty
        } // read and process comands in a loop

closeFtpConnection:      
        if (fsp.userName != "") ftpDmesg ("[ftpServer] " + fsp.userName + " logged out.");
      }
    
      // returns last chracter pressed (Enter or 0 in case of error
      static char __readLineFromClient__ (String *line, TcpConnection *connection) {
        *line = "";
        
        char c;
        while (connection->recvData (&c, 1)) { // read and process incomming data in a loop 
          switch (c) {
              case 10:  // ignore
                        break;
              case 13:  // end of command line
                        return 13;
              default:  // fill the buffer 
                        *line += c;
                        break;              
          } // switch
        } // while
        return false;
      }

      // ----- built-in commands -----

      String __internalFtpCommandHandler__ (int argc, String argv [], String param, ftpServer::ftpSessionParameters *fsp) {
        argv [0].toUpperCase ();

        if (argv [0] == "OPTS") { //------------------------------------------- OPTS (before login process)
          
          if (argc == 3) return this->__OPTS__ (argv [1], argv [2]);

        } else if (argv [0] == "USER") { //------------------------------------ USER (login process)
          
          if (argc <= 2) return this->__USER__ (argv [1], fsp); // not entering user name may be ok for anonymous login

        } else if (argv [0] == "PASS") { //------------------------------------ PASS (login process)
          
          if (argc <= 2) return this->__PASS__ (argv [1], fsp); // not entering password may be ok for anonymous login

        } else if (argv [0] == "CWD") { //------------------------------------ CWD (cd directory)
          
          if (argc >= 2) return this->__CWD__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "PWD" || argv [0] == "XPWD") { //-------------- PWD, XPWD
          
          if (argc == 1) return this->__PWD__ (fsp);

        } else if (argv [0] == "TYPE") { //----------------------------------- TYPE
          
          return this->__TYPE__ (fsp);

        } else if (argv [0] == "NOOP") { //----------------------------------- NOOP
          
          return this->__NOOP__ ();

        } else if (argv [0] == "SYST") { //----------------------------------- SYST
          
          return this->__SYST__ (fsp);

        } else if (argv [0] == "SIZE") { //----------------------------------- SIZE 
          
          if (argc == 2) return this->__SIZE__ (argv [1], fsp);

        } else if (argv [0] == "PASV") { //----------------------------------- PASV - start pasive data transfer (always followed by one of data transfer commands)
          
          if (argc == 1) return this->__PASV__ (fsp);

        } else if (argv [0] == "PORT") { //----------------------------------- PORT - start active data transfer (always followed by one of data transfer commands)
          
          if (argc == 2) return this->__PORT__ (argv [1], fsp);

        } else if (argv [0] == "NLST" || argv [0] == "LIST") { //------------- NLST, LIST (ls directory) - also ends pasive or active data transfer

          if (argc == 1) return this->__NLST__ (fsp->workingDir, fsp);
          if (argc == 2) return this->__NLST__ (argv [1], fsp);

        } else if (argv [0] == "CWD") { //------------------------------------ CWD (cd directory)

          if (argc >= 2) return this->__CWD__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "XMKD" || argv [0] == "MKD") { //-------------- XMKD, MKD (mkdir directory)

          if (argc >= 2) return this->__XMKD__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "RNFR" || argv [0] == ".....") { //-------------- RNFR, MKD (... file or directory)

          if (argc >= 2) return this->__RNFR__ (param, fsp); // use param instead of argv [1] to enable the use of long file names
          
        } else if (argv [0] == "XRMD" || argv [0] == "DELE") { //------------- XRMD, DELE (rm file or directory)

          if (argc >= 2) return this->__XRMD__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "RETR") { //----------------------------------- RETR (get fileName) - also ends pasive or active data transfer

          if (argc >= 2) return this->__RETR__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "STOR") { //----------------------------------- STOR (put fileName) - also ends pasive or active data transfer

          if (argc >= 2) return this->__STOR__ (param, fsp); // use param instead of argv [1] to enable the use of long file names

        } else if (argv [0] == "QUIT") { //----------------------------------- QUIT
          
          return this->__QUIT__ (fsp);
                    
        }

        // -------------------------------------------------------------------- INVALID COMMAND
                      
        return "502 command not implemented\r\n"; // "220 unsupported command\r\n";                               
      }

      inline String __OPTS__ (String argv1, String argv2) { // enable UTF8
        argv1.toUpperCase (); argv2.toUpperCase ();
        if (argv1 == "UTF8" && argv2 == "ON")         return "200 UTF8 enabled\r\n";
                                                      return "502 command not implemented\r\n";
      }

      inline String __USER__ (String userName, ftpSessionParameters *fsp) { // save user name and require password
        fsp->userName = userName;                     return "331 enter password\r\n";
      }

      inline String __PASS__ (String password, ftpSessionParameters *fsp) { // login
        if (checkUserNameAndPassword (fsp->userName, password)) fsp->workingDir = fsp->homeDir = getUserHomeDirectory (fsp->userName);
        if (fsp->homeDir > "") { // if logged in
          ftpDmesg ("[ftpServer] " + fsp->userName + " logged in.");
                                                      return "230 logged on, your home directory is \"" + fsp->homeDir + "\"\r\n";
        } else { 
          ftpDmesg ("[ftpServer] " + fsp->userName + " login attempt failed.");
                                                      return "530 user name or password incorrect\r\n"; 
        }
      }

      inline String __PWD__ (ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                      return "530 not logged in\r\n";
                                                     return "257 \"" + fsp->workingDir + "\"\r\n";
      }

      inline String __TYPE__ (ftpSessionParameters *fsp) { // command needs to be implemented but we are not going to repond to it
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
                                                      return "200 ok\r\n";
      }

      inline String __NOOP__ () { 
                                                      return "200 ok\r\n";
      }
      
      inline String __SYST__ (ftpSessionParameters *fsp) { // command needs to be implemented but we are going to pretend this is a UNIX system
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
                                                      return "215 UNIX Type: L8\r\n";
      }

      inline String __SIZE__ (String fileName, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        String fp = fullFilePath (fileName, fsp->workingDir);
        if (fp == "")                                 return "550 invalid file name\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        unsigned long fSize = 0;
        File f = FFat.open (fp, FILE_READ);
        if (f) {
          fSize = f.size ();                        
          f.close ();
        }
                                                      return "213 " + String (fSize) + "\r\n";
      }

      inline String __PASV__ (ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        int ip1, ip2, ip3, ip4, p1, p2; // get (this) server IP and next free port
        if (4 == sscanf (fsp->controlConnection->getThisSideIP (), "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
          // get next free port
          int pasiveDataPort = __pasiveDataPort__ ();
           // open a new TCP server to accept pasive data connection
          fsp->pasiveDataServer = new TcpServer (5000, fsp->controlConnection->getThisSideIP (), pasiveDataPort, NULL);
          // report to ftp client through control connection how to connect for data exchange
          p2 = pasiveDataPort % 256;
          p1 = pasiveDataPort / 256;
          if (fsp->pasiveDataServer)                  return "227 entering passive mode (" + String (ip1) + "," + String (ip2) + "," + String (ip3) + "," + String (ip4) + "," + String (p1) + "," + String (p2) + ")\r\n";
        } 
                                                      return "425 can't open data connection\r\n";
      }

      inline String __PORT__ (String dataConnectionInfo, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
        
        int ip1, ip2, ip3, ip4, p1, p2; // get client IP and port
        if (6 == sscanf (dataConnectionInfo.c_str (), "%i,%i,%i,%i,%i,%i", &ip1, &ip2, &ip3, &ip4, &p1, &p2)) {
          char activeDataIP [16];
          int activeDataPort;
          sprintf (activeDataIP, "%i.%i.%i.%i", ip1, ip2, ip3, ip4); 
          activeDataPort = 256 * p1 + p2;
          fsp->activeDataClient = new TcpClient (activeDataIP, activeDataPort, 5000); // open a new TCP client for active data connection
          if (fsp->activeDataClient) {
            if (fsp->activeDataClient->connection ()) return "200 port ok\r\n"; 
            delete (fsp->activeDataClient);
            fsp->activeDataClient = NULL;
          }
        } 
                                                      return "425 can't open data connection\r\n";
      }

      inline String __NLST__ (String& directory, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        String fp = fullDirectoryPath (directory, fsp->workingDir);

        if (fp == "" || !isDirectory (fp))            return "550 invalid directory\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        if (fp.substring (fp.length () - 1) != "/") fp += '/'; // full directoy path always ends with /

        if (!fsp->controlConnection->sendData ((char *) "150 starting transfer\r\n")) return ""; // control connection closed
        
        int bytesWritten = 0;
        TcpConnection *dataConnection = NULL;
        if (fsp->pasiveDataServer) while (!(dataConnection = fsp->pasiveDataServer->connection ()) && !fsp->pasiveDataServer->timeOut ()) delay (1); // wait until a connection arrives to non-threaded server or time-out occurs
        if (fsp->activeDataClient) dataConnection = fsp->activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all

        if (dataConnection) bytesWritten = dataConnection->sendData (listDirectory (fp) + "\r\n");
        
        if (fsp->activeDataClient) { delete (fsp->activeDataClient); fsp->activeDataClient = NULL; }
        if (fsp->pasiveDataServer) { delete (fsp->pasiveDataServer); fsp->pasiveDataServer = NULL; }
        if (bytesWritten)                             return "226 transfer complete\r\n";
                                                      return "425 can't open data connection\r\n";
      }

      inline String __CWD__ (String directory, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
        String fp = fullDirectoryPath (directory, fsp->workingDir);
        if (fp == "" || !isDirectory (fp))            return "501 invalid directory " + fp +"\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        fsp->workingDir = fp;                         return "250 your working directory is now " + fsp->workingDir + "\r\n";
      }

      inline String __XMKD__ (String directory, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
        String fp = fullDirectoryPath (directory, fsp->workingDir);
        if (fp == "")                                 return "501 invalid directory\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        if (makeDir (fp))                             return "257 \"" + fp + "\" created\r\n";
                                                      return "550 could not create \"" + fp + "\"\r\n";
      }

      inline String __RNFR__ (String fileOrDirName, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        fsp->controlConnection->sendData ((char *) "350 need more information\r\n");
        String s;
        if (13 != __readLineFromClient__ (&s, fsp->controlConnection)) return "503 wrong command syntax\r\n";

        if (s.substring (0, 4) != "RNTO")             return "503 wrong command syntax\r\n";
        s = s.substring (4); s.trim ();
        String fp1 = fullFilePath (fileOrDirName, fsp->workingDir);
        if (fp1 == "")                                return "501 invalid directory\r\n";
        if (!userMayAccess (fp1, fsp->homeDir))       return "553 access denyed\r\n";

        String fp2 = fullFilePath (s, fsp->workingDir);
        if (fp2 == "")                                return "501 invalid directory\r\n";
        if (!userMayAccess (fp2, fsp->homeDir))       return "553 access denyed\r\n";

        if (FFat.rename (fp1, fp2))                   return "250 renamed to " + s + "\r\n";
                                                      return "553 unable to rename " + fileOrDirName + "\r\n";
      }

      inline String __XRMD__ (String fileOrDirName, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";
        String fp = fullFilePath (fileOrDirName, fsp->workingDir);
        if (fp == "")                                 return "501 invalid file or directory name\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        if (isFile (fp)) {
          if (deleteFile (fp))                        return "250 " + fp + " deleted\r\n";
                                                      return "452 could not delete " + fp + "\r\n";
        } else {
          if (fp == fsp->homeDir)                     return "550 you can't remove your home directory\r\n";
          if (fp == fsp->workingDir)                  return "550 you can't remove your working directory\r\n";
          if (removeDir (fp))                         return "250 " + fp + " removed\r\n";
                                                      return "452 could not remove " + fp + "\r\n";
        }
                             
      }

      inline String __RETR__ (String fileName, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        String fp = fullFilePath (fileName, fsp->workingDir);

        if (fp == "" || !isFile (fp))                 return "501 invalid file name\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        if (!fsp->controlConnection->sendData ((char *) "150 starting transfer\r\n")) return ""; // control connection closed 

        int bytesWritten = 0; int bytesRead = 0;
        TcpConnection *dataConnection = NULL;
        if (fsp->pasiveDataServer) while (!(dataConnection = fsp->pasiveDataServer->connection ()) && !fsp->pasiveDataServer->timeOut ()) delay (1); // wait until a connection arrives to non-threaded server or time-out occurs
        if (fsp->activeDataClient) dataConnection = fsp->activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all

        if (dataConnection) {
          File f = FFat.open (fp, FILE_READ);
          if (f) {
            if (!f.isDirectory ()) {
              // read data from file and transfer it through data connection
              byte *buff = (byte *) malloc (2048); // get 2048 B of memory from heap (not from the stack)
              if (buff) {
                int i = bytesWritten = 0;
                while (f.available ()) {
                  *(buff + i++) = f.read ();
                  if (i == 2048) { bytesRead += 2048; bytesWritten += dataConnection->sendData ((char *) buff, 2048); i = 0; }
                }
                if (i) { bytesRead += i; bytesWritten += dataConnection->sendData ((char *) buff, i); }
                free (buff);
              }
            }
            f.close ();
          }
        }

        if (fsp->activeDataClient) { delete (fsp->activeDataClient); fsp->activeDataClient = NULL; }
        if (fsp->pasiveDataServer) { delete (fsp->pasiveDataServer); fsp->pasiveDataServer = NULL; }
        if (bytesRead && bytesWritten == bytesRead)   return "226 transfer complete\r\n";
                                                      return "550 file could not be transfered\r\n";      
      }

      inline String __STOR__ (String fileName, ftpSessionParameters *fsp) { 
        if (fsp->homeDir == "")                       return "530 not logged in\r\n";

        String fp = fullFilePath (fileName, fsp->workingDir);

        if (fp == "" || isDirectory (fp))             return "501 invalid file name\r\n";
        if (!userMayAccess (fp, fsp->homeDir))        return "550 access denyed\r\n";

        if (!fsp->controlConnection->sendData ((char *) "150 starting transfer\r\n")) return ""; // control connection closed 

        int bytesWritten = 0; int bytesRead = 0;
        TcpConnection *dataConnection = NULL;
        if (fsp->pasiveDataServer) while (!(dataConnection = fsp->pasiveDataServer->connection ()) && !fsp->pasiveDataServer->timeOut ()) delay (1); // wait until a connection arrives to non-threaded server or time-out occurs
        if (fsp->activeDataClient) dataConnection = fsp->activeDataClient->connection (); // non-threaded client differs from non-threaded server - connection is established before constructor returns or not at all

        if (dataConnection) {
          File f = FFat.open (fp, FILE_WRITE);
          if (f) {
            byte *buff = (byte *) malloc (2048); // get 2048 B of memory from heap (not from the stack)
            if (buff) {
              int received;
              do {
                bytesRead += (received = dataConnection->recvData ((char *) buff, 2048));
                int written = f.write (buff, received);                   
                if (received && (written == received)) bytesWritten += written;
              } while (received);
              free (buff);
            }
            f.close ();
          } else {
            ftpDmesg ("[ftpServer] could not open " + fp + " for writing.");
          }
        }

        if (fsp->activeDataClient) { delete (fsp->activeDataClient); fsp->activeDataClient = NULL; }
        if (fsp->pasiveDataServer) { delete (fsp->pasiveDataServer); fsp->pasiveDataServer = NULL; }
        if (bytesRead && bytesWritten == bytesRead)   return "226 transfer complete\r\n";
                                                      return "550 file could not be transfered\r\n";      
      }

      inline String __QUIT__ (ftpSessionParameters *fsp) { 
        // close data connection if opened
        if (fsp->activeDataClient) delete (fsp->activeDataClient);
        if (fsp->pasiveDataServer) delete (fsp->pasiveDataServer); 
        // report client we are closing contro connection
        fsp->controlConnection->sendData ((char *) "221 closing control connection\r\n");
        fsp->controlConnection->closeConnection ();
        return "";
      }
      
  };

#endif
