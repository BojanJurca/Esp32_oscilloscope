/*

    ftpClient.hpp 
 
    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
  
    FTP client establishes pasive data connection for a single file transfer.

    May 22, 2024, Bojan Jurca

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <lwip/netdb.h>
    // fixed size strings
    #include "std/Cstring.hpp"


#ifndef __FTP_CLIENT__
    #define __FTP_CLIENT__

    #ifndef __FILE_SYSTEM__
        #error "You can't use ftpClient.h without file_system.h. Either #include file_system.h prior to including ftpClient.h or exclude ftpClient.h"
    #endif


    // ----- functions and variables in this modul -----

    cstring ftpPut (const char *, const char *, const char *, const char *, int, const char *);
    cstring ftpGet (const char *, const char *, const char *, const char *, int, const char *);

    
    // TUNNING PARAMETERS

    #define FTP_CLIENT_TIME_OUT 3000              // 3000 ms = 3 sec 
    #define FTP_CMD_BUFFER_SIZE 300               // sending FTP commands and readinf replies from the server
    // ftpClientBuffer will be used first to read configuration file /etc/mail/sendmail.cf file in the memory and then for data transmission, fro both purposes 1 KB seems OK
    #define FTP_CLIENT_BUFFER_SIZE 1024           // MTU = 1500 (maximum transmission unit), TCP_SND_BUF = 5744 (a maximum block size that ESP32 can send), FAT cluster size = n * 512. 1024 seems a good trade-off


    // ----- CODE -----

       
    // sends or receives a file via FTP, ftpCommand is eighter "PUT" or "GET", returns error or success text from FTP server, uses ftpClientBuffer that should be allocated in advance
    cstring __ftpClient__ (const char *ftpCommand, const char *clientFile, const char *serverFile, const char *password, const char *userName, int ftpPort, const char *ftpServer, const char *ftpClientBuffer) {
      // get server address
      struct hostent *he = gethostbyname (ftpServer);
      if (!he) {
        return cstring ("gethostbyname error: ") + cstring (h_errno) + " " + hstrerror (h_errno);
      }
      // create socket
      int controlConnectionSocket = socket (PF_INET, SOCK_STREAM, 0);
      if (controlConnectionSocket == -1) {
          return cstring ("socket error: ") + cstring (errno) + " " + strerror (errno);
      }

      // remember some information that netstat telnet command would use
      additionalSocketInformation [controlConnectionSocket - LWIP_SOCKET_OFFSET] = { __FTP_CLIENT_SOCKET__, 0, 0, millis (), millis () };

      // make the socket not-blocking so that time-out can be detected
      if (fcntl (controlConnectionSocket, F_SETFL, O_NONBLOCK) == -1) {
          cstring e = cstring (errno) + " " + strerror (errno);
          close (controlConnectionSocket);
          return cstring ("fcntl error: ") + e;
      }
      // connect to server
      struct sockaddr_in serverAddress;
      serverAddress.sin_family = AF_INET;
      serverAddress.sin_port = htons (ftpPort);
      serverAddress.sin_addr.s_addr = *(in_addr_t *) he->h_addr; 
      if (connect (controlConnectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
          if (errno != EINPROGRESS) {
              cstring e = cstring (errno) + " " + strerror (errno); 
              close (controlConnectionSocket);
              return cstring ("connect error: ") + e;
          }
      } // it is likely that socket is not connected yet at this point

      char buffer [FTP_CMD_BUFFER_SIZE];
      int dataConnectionSocket = -1;
     
      // read and process FTP server replies in an endless loop
      int receivedTotal = 0;
      do {
        receivedTotal = recvAll (controlConnectionSocket, buffer + receivedTotal, sizeof (buffer) - 1 - receivedTotal, "\n", FTP_CLIENT_TIME_OUT);
        if (receivedTotal <= 0) {
          cstring e = cstring (errno) + " " + strerror (errno);
          close (controlConnectionSocket);
          return cstring ("recv error: ") + e;
        }
        char *endOfCommand = strstr (buffer, "\n");
        while (endOfCommand) {
          *endOfCommand = 0;
          // process command in the buffer
          {
            #define ftpReplyIs(X) (strstr (buffer, X) == buffer)

                  if (ftpReplyIs ("220 "))  { // server wants client to log in
                                              cstring s = cstring ("USER ") + userName + "\r\n";
                                              if (sendAll (controlConnectionSocket, s, FTP_CLIENT_TIME_OUT) == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (controlConnectionSocket);
                                                return cstring ("send error: ") + e;
                                              }
                                            }
            else if (ftpReplyIs ("331 "))   { // server wants client to send password
                                              cstring s = cstring ("PASS ") + password + "\r\n";
                                              if (sendAll (controlConnectionSocket, s, FTP_CLIENT_TIME_OUT) == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (controlConnectionSocket);
                                                return cstring ("send error: ") + e;
                                              }
                                            }
            else if (ftpReplyIs ("230 "))   { // server said that we have logged in, initiate pasive data connection
                                              if (sendAll (controlConnectionSocket, "PASV\r\n", FTP_CLIENT_TIME_OUT) == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (controlConnectionSocket);
                                                return cstring ("send error: ") + e;
                                              }
                                            }
            else if (ftpReplyIs ("227 "))   { // server send connection information like 227 entering passive mode (10,18,1,66,4,1)
                                              // open data connection
                                              int ip1, ip2, ip3, ip4, p1, p2; // get FTP server IP and port
                                              if (6 != sscanf (buffer, "%*[^(](%i,%i,%i,%i,%i,%i)", &ip1, &ip2, &ip3, &ip4, &p1, &p2)) { // should always succeed
                                                close (controlConnectionSocket);
                                                return buffer; 
                                              }
                                              char pasiveDataIP [46]; sprintf (pasiveDataIP, "%i.%i.%i.%i", ip1, ip2, ip3, ip4);
                                              int pasiveDataPort = p1 << 8 | p2; 
                                              // establish data connection now
                                              /* // we already heve client's IP so we don't have to reslove its name first
                                              struct hostent *he = gethostbyname (pasiveDataIP);
                                              if (!he) {
                                                dmesg ("[httpClient] gethostbyname() error: ", h_errno);
                                                return F ("425 can't open active data connection\r\n");
                                              }
                                              */
                                              // create socket
                                              dataConnectionSocket = socket (PF_INET, SOCK_STREAM, 0);
                                              if (dataConnectionSocket == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (controlConnectionSocket);
                                                return cstring ("socket error: ") + e; 
                                              }

                                              // remember some information that netstat telnet command would use
                                              additionalSocketInformation [dataConnectionSocket - LWIP_SOCKET_OFFSET] = { __FTP_DATA_SOCKET__, 0, 0, millis (), millis () };

                                              // make the socket not-blocking so that time-out can be detected
                                              if (fcntl (dataConnectionSocket, F_SETFL, O_NONBLOCK) == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (dataConnectionSocket);
                                                close (controlConnectionSocket);
                                                return cstring ("fcntl error: ") + e; 
                                              }
                                              // connect to client that acts as a data server 
                                              struct sockaddr_in serverAddress;
                                              serverAddress.sin_family = AF_INET;
                                              serverAddress.sin_port = htons (pasiveDataPort);
                                              serverAddress.sin_addr.s_addr = inet_addr (pasiveDataIP); // serverAddress.sin_addr.s_addr = *(in_addr_t *) he->h_addr; 
                                              /*
                                              int retVal;
                                              unsigned long startMillis = millis ();
                                              do {
                                                retVal = connect (dataConnectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress));
                                                if (retVal != -1) break; // success
                                                if (errno == EINPROGRESS && millis () - startMillis < FTP_CLIENT_TIME_OUT) delay (1); // continue waiting
                                                else {
                                                  cstring e (errno);
                                                  close (dataConnectionSocket);
                                                  close (controlConnectionSocket);
                                                  return ("connect() error: " + e);                                                 
                                                }
                                              } while (true);
                                              */
                                              if (connect (dataConnectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
                                                if (errno != EINPROGRESS) {
                                                  cstring e = cstring (errno) + " " + strerror (errno);
                                                  close (dataConnectionSocket);
                                                  close (controlConnectionSocket);
                                                  return cstring ("connect error: ") + e;
                                                } 
                                              }
                                              // it is likely that socket is not connected yet at this point (the socket is non-blocking)
                                              // are we GETting or PUTting the file?
                                              cstring s;
                                                      if (!strcmp (ftpCommand, "GET")) {
                                                        s = cstring ("RETR ") + serverFile + "\r\n";
                                              } else if (!strcmp (ftpCommand, "PUT")) {
                                                        s = cstring ("STOR ") + serverFile + "\r\n";
                                              } else  {
                                                close (dataConnectionSocket);
                                                close (controlConnectionSocket);
                                                return cstring ("Unknown FTP command ") + ftpCommand; 
                                              }
                                              if (sendAll (controlConnectionSocket, s, FTP_CLIENT_TIME_OUT) == -1) {
                                                cstring e = cstring (errno) + " " + strerror (errno);
                                                close (dataConnectionSocket);
                                                close (controlConnectionSocket);
                                                return cstring ("send error: ") + e;
                                              }
                                            }

            else if (ftpReplyIs ("150 "))   { // server wants to start data transfer
                                              // are we GETting or PUTting the file?
                                                      if (!strcmp (ftpCommand, "GET")) {
                                                        
                                                          int bytesRecvTotal = 0; int bytesWrittenTotal = 0;
                                                          File f = fileSystem.open (clientFile, "w");
                                                          if (f) {
                                                            // read data from data connection and store it to the file
                                                            do {
                                                              int bytesRecvThisTime = recvAll (dataConnectionSocket, (char *) ftpClientBuffer, FTP_CLIENT_BUFFER_SIZE, NULL, FTP_CLIENT_TIME_OUT);
                                                              if (bytesRecvThisTime < 0)  { bytesRecvTotal = -1; break; } // to detect error later
                                                              if (bytesRecvThisTime == 0) { break; } // finished, success
                                                              bytesRecvTotal += bytesRecvThisTime;
                                                              int bytesWrittenThisTime = f.write ((uint8_t *) ftpClientBuffer, bytesRecvThisTime);
                                                              bytesWrittenTotal += bytesWrittenThisTime;
                                                              if (bytesWrittenThisTime != bytesRecvThisTime) { bytesRecvTotal = -1; break; } // to detect error later
                                                            } while (true);
                                                            f.close ();
                                                          } else {
                                                            close (dataConnectionSocket);
                                                            close (controlConnectionSocket);
                                                            return cstring ("Can't open ") + clientFile + " for writting";
                                                          }
                                                          if (bytesWrittenTotal != bytesRecvTotal) {
                                                            close (dataConnectionSocket);
                                                            close (controlConnectionSocket);
                                                            return cstring ("Can't write ") + clientFile;
                                                          }
                                                          close (dataConnectionSocket);

                                                          diskTrafficInformation.bytesWritten += bytesWrittenTotal; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                                              } else if (!strcmp (ftpCommand, "PUT")) {
                                                          int bytesReadTotal = 0; int bytesSentTotal = 0;
                                                          File f = fileSystem.open (clientFile, "r");
                                                          if (f) {
                                                            // read data from file and transfer it through data connection
                                                            do {
                                                              int bytesReadThisTime = f.read ((uint8_t *) ftpClientBuffer, FTP_CLIENT_BUFFER_SIZE);
                                                              if (bytesReadThisTime == 0) { break; } // finished, success
                                                              bytesReadTotal += bytesReadThisTime;
                                                              int bytesSentThisTime = sendAll (dataConnectionSocket, ftpClientBuffer, bytesReadThisTime, FTP_CLIENT_TIME_OUT);
                                                              if (bytesSentThisTime != bytesReadThisTime) { bytesSentTotal = -1; break; } // to detect error later
                                                              bytesSentTotal += bytesSentThisTime;
                                                            } while (true);
                                                            f.close ();
                                                          } else {
                                                            close (dataConnectionSocket);
                                                            close (controlConnectionSocket);
                                                            return cstring ("Can't open ") + clientFile + " for reading";
                                                          }
                                                          if (bytesSentTotal != bytesReadTotal) {
                                                            close (dataConnectionSocket);
                                                            close (controlConnectionSocket);
                                                            return cstring ("Can't send ") + clientFile;                                                            
                                                          }
                                                          close (dataConnectionSocket);

                                                          diskTrafficInformation.bytesRead += bytesReadTotal; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                                       
                                              } else  {
                                                close (dataConnectionSocket);
                                                close (controlConnectionSocket);
                                                return cstring ("Unknown FTP command ") + ftpCommand; 
                                              }
                                            }
            else                            {
                                              if (buffer [3] == ' ')  {
                                                                        close (controlConnectionSocket);
                                                                        return buffer;
                                                                      }
                                            }
          }
          // is there more commands in the buffer?
          strcpy (buffer, endOfCommand + 1);
          endOfCommand = strstr (buffer, "\n");
        }
        receivedTotal = strlen (buffer);
      } while (controlConnectionSocket > -1); // while the connection is still opened
      return "";
  }

  // sends file, returns error or success text, fills empty parameters with the ones from configuration file /etc/ftp/ftpclient.cf
  cstring ftpPut (const char *localFileName = "", const char *remoteFileName = "", const char *password = "", const char *userName = "", int ftpPort = 0, const char *ftpServer = "") {
    char ftpClientBuffer [FTP_CLIENT_BUFFER_SIZE]; // ftpClientBuffer will be used first to read configuration file /etc/mail/sendmail.cf file in the memory and then for data transmission, fro both purposes 1 KB seems OK
    if (fileSystem.mounted ()) { 
      strcpy (ftpClientBuffer, "\n");
      if (fileSystem.readConfigurationFile (ftpClientBuffer + 1, sizeof (ftpClientBuffer) - 2, "/etc/ftp/ftpclient.cf")) {
        strcat (ftpClientBuffer, "\n");
        char *p = NULL;
        char *q;
        if (!*ftpServer) if ((ftpServer = strstr (ftpClientBuffer, "\nftpServer "))) ftpServer += 11;
        if (!ftpPort)    if ((p = strstr (ftpClientBuffer, "\nftpPort ")))           p += 9;
        if (!*userName)  if ((userName = strstr (ftpClientBuffer, "\nuserName ")))   userName += 10;
        if (!*password)  if ((password = strstr (ftpClientBuffer, "\npassword ")))   password += 10;

        for (q = ftpClientBuffer; *q; q++)
          if (*q < ' ') *q = 0;
        if (p) ftpPort = atoi (p);
      }
    } else {
        #ifdef __DMESG__
            dmesgQueue << "[ftpClient] file system not mounted, can't read /etc/ftp/ftpclient.cf";
        #endif
    }
    if (!*password || !*userName || !ftpPort || !*ftpServer) {
      #ifdef __DMESG__
          dmesgQueue << "[ftpClient] not all the arguments are set, if you want to use the default values, write them to /etc/ftp/ftpclient.cf";
      #endif
      return "Not all the arguments are set, if you want to use ftpPut default values, write them to /etc/ftp/ftpclient.cf";
    } else {
      return __ftpClient__ ("PUT", localFileName, remoteFileName, password, userName, ftpPort, ftpServer, ftpClientBuffer);
    }
  }

  // retrieves file, returns error or success text, fills empty parameters with the ones from configuration file /etc/ftp/ftpclient.cf
  cstring ftpGet (const char *localFileName = "", const char *remoteFileName = "", const char *password = "", const char *userName = "", int ftpPort = 0, const char *ftpServer = "") {
      char ftpClientBuffer [FTP_CLIENT_BUFFER_SIZE]; // ftpClientBuffer will be used first to read configuration file /etc/mail/sendmail.cf file in the memory and then for data transmission, fro both purposes 1 KB seems OK
      if (fileSystem.mounted ()) {
          strcpy (ftpClientBuffer, "\n");
          if (fileSystem.readConfigurationFile (ftpClientBuffer + 1, sizeof (ftpClientBuffer) - 2, "/etc/ftp/ftpclient.cf")) {
              strcat (ftpClientBuffer, "\n");

              char *p = NULL;
              char *q;
              if (!*ftpServer) if ((ftpServer = stristr (ftpClientBuffer, "\nftpServer "))) ftpServer += 11;
              if (!ftpPort)    if ((p = strstr (ftpClientBuffer, "\nftpPort ")))            p += 9;
              if (!*userName)  if ((userName = stristr (ftpClientBuffer, "\nuserName ")));  userName += 10;
              if (!*password)  if ((password = stristr (ftpClientBuffer, "\npassword ")));  password += 10;

              if ((q = strstr (ftpServer, "\n"))) *q = 0;
              if (p && (q = strstr (p, "\n"))) { *q = 0; ftpPort = atoi (p); }
              if ((q = strstr (userName, "\n"))) *q = 0;
              if ((q = strstr (password, "\n"))) *q = 0;
          }
      } else {
          #ifdef __DMESG__
              dmesgQueue << "[ftpClient] file system not mounted, can't read /etc/ftp/ftpclient.cf";
          #endif
      }
      if (!*password || !*userName || !ftpPort || !*ftpServer) {
          #ifdef __DMESG__
              dmesgQueue << "[ftpClient] not all the arguments are set, if you want to use the default values, write them to /etc/ftp/ftpclient.cf";
          #endif
          return "Not all the arguments are set, if you want to use ftpPut default values, write them to /etc/ftp/ftpclient.cf";
      } else {
          return __ftpClient__ ("GET", localFileName, remoteFileName, password, userName, ftpPort, ftpServer, ftpClientBuffer);
    }
  }

#endif
