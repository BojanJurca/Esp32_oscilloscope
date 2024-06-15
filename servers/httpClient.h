/*

    httpClient.hpp 
  
    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
  
    HTTP client combines a HTTP request from server, page and method and returns a HTTP reply or "" if there is none.
  
    May 22, 2024, Bojan Jurca

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <lwip/netdb.h>
    // fixed size strings
    #include "std/Cstring.hpp"
    

#ifndef __HTTP_CLIENT__
  #define __HTTP_CLIENT__


    // ----- functions and variables in this modul -----

    String httpClient (const char *serverName, int serverPort, const char *httpRequest, const char *httpMethod, unsigned long timeOut);


    // ----- TUNNING PARAMETERS -----

    #define HTTP_REPLY_TIME_OUT 3000              // 1500 ms = 1,5 sec for HTTP requests
    #define HTTP_REPLY_BUFFER_SIZE 1500           // reading HTTP reply, MTU = 1500 (maximum transmission unit), TCP_SND_BUF = 5744 (a maximum block size that ESP32 can send), FAT cluster size = n * 512. 1500 seems the best option


    // ----- CODE -----

    String httpClient (const char *serverName, int serverPort, const char *httpAddress, const char *httpMethod = (char *) "GET", unsigned long timeOut = HTTP_REPLY_TIME_OUT) {
      // get server address
      struct hostent *he = gethostbyname (serverName);
      if (!he) return "[httpClient] gethostbyname() error: " + String (h_errno) + " " + hstrerror (h_errno);
      // create socket
      int connectionSocket = socket (PF_INET, SOCK_STREAM, 0);
      if (connectionSocket == -1) return "[httpClient] socket() error: " + String (errno) + " " + strerror (errno);

      // remember some information that netstat telnet command would use
      additionalSocketInformation [connectionSocket - LWIP_SOCKET_OFFSET] = { __HTTP_CLIENT_SOCKET__, 0, 0, millis (), millis () };

      // make the socket not-blocking so that time-out can be detected
      if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
        int e = errno;
        close (connectionSocket);
        return "[httpClient] fcnt() error: " + String (e) + " " + strerror (e);
      }
      // connect to server
      struct sockaddr_in serverAddress;
      serverAddress.sin_family = AF_INET;
      serverAddress.sin_port = htons (serverPort);
      serverAddress.sin_addr.s_addr = *(in_addr_t *) he->h_addr; 
      if (connect (connectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
        if (errno != EINPROGRESS) {
          int e = errno;
          close (connectionSocket);
          return "[httpClient] connect() error: " + String (e) + " " + strerror (e);
        } // it is likely that socket is not connected yet at this point
      }

      // construct and send minimal HTTP request (or almost minimal, IIS for example, requires Host field)
      char serverIP [46];
      inet_ntoa_r (serverAddress.sin_addr, serverIP, sizeof (serverIP));
      cstring httpRequest;
      httpRequest += httpMethod;
      httpRequest += " ";
      httpRequest += httpAddress;
      httpRequest += " HTTP/1.0\r\nHost: ";
      httpRequest += serverIP;
      httpRequest += "\r\n\r\n"; // 1.0 HTTP does not know keep-alive directive - we want the server to close the connection immediatelly after sending the reply
      if (httpRequest.errorFlags ()) {
          close (connectionSocket);
          return "Out of memory";
      }
      if (sendAll (connectionSocket, httpRequest, timeOut) == -1) {
          int e = errno;
          close (connectionSocket);
          return "[httpClient] send() error: " + String (e) + " " + strerror (e);
      }
      // read HTTP reply  
      char __httpReply__ [HTTP_REPLY_BUFFER_SIZE];
      int receivedThisTime;    
      unsigned long lastActive = millis ();
      String httpReply ("");
      while (true) { // read blocks of incoming data
        switch (receivedThisTime = recv (connectionSocket, __httpReply__, HTTP_REPLY_BUFFER_SIZE - 1, 0)) {
          case -1: {    // error
                        if ((errno == EAGAIN || errno == ENAVAIL) && HTTP_REPLY_TIME_OUT && millis () - lastActive < HTTP_REPLY_TIME_OUT) break; // not time-out yet
                        int e = errno;
                        close (connectionSocket);
                        return "[httpClient] recv() error: " + String (e) + " " + strerror (e);
                    }
          case 0:       // connection closed by peer
                        close (connectionSocket);  
                        if (stristr ((char *) httpReply.c_str (), (char *) "\nCONTENT-LENGTH:")) return ""; // Content-lenght does not match
                        else return httpReply; // return what we have recived without checking if HTTP reply is OK
          default:

                        additionalNetworkInformation.bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                        additionalSocketInformation [connectionSocket - LWIP_SOCKET_OFFSET].bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                        __httpReply__ [receivedThisTime] = 0;
                          int bl = httpReply.length ();
                        httpReply += __httpReply__;
                          if (httpReply.length () != bl + strlen (__httpReply__)) {
                            close (connectionSocket);
                            return "Out of memory";
                          }
                        lastActive = millis ();
                        // check if HTTP reply is complete
                        char *p = stristr ((char *) httpReply.c_str (), (char *) "\nCONTENT-LENGTH:");
                        if (p) {
                          p += 16;
                          unsigned int contentLength;
                          if (sscanf (p, "%u", &contentLength) == 1) {
                            p = strstr (p, "\r\n\r\n"); // the content comes afterwards
                            if (p && contentLength == strlen (p + 4)) {
                              close (connectionSocket);
                              return httpReply;
                            }
                          }
                        }
                        // else continue reading
        }
      }
      // never executes
      close (connectionSocket);
      return "";
    }
 
#endif
