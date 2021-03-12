/*
 * 
 * webServer.hpp 
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 *  WebServer includes httpServer with connectionHandler that handles TcpConnection according to HTTP protocol.
 * 
 *  A connectionHandler handles some HTTP requests by itself but the calling program can provide its own callback
 *  function. In this case connectionHandler will first ask callback function weather is it going to handle the HTTP 
 *  request. If not the connectionHandler will try to resolve it by checking file system for a proper .html file
 *  or it will respond with reply 404 - page not found.
 * 
 * History:
 *          - first release, 
 *            November 19, 2018, Bojan Jurca
 *          - added fileSystemSemaphore and delay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            April 13, 2019, Bojan Jurca 
 *          - added webClient function,
 *            added basic support for web sockets
 *            May, 19, 2019, Bojan Jurca
 *          - the use of dmesg
 *            September 14, 2019, Bojan Jurca
 *          - added webClientCallMAC function
 *            September, 27, Bojan Jurca
 *          - separation of httpHandler and wsHandler
 *            October 30, 2019, Bojan Jurca
 *          - webServer is now inherited from TcpServer and renamed to httpServer
 *            February 27, 2020, Bojan Jurca 
 *          - elimination of compiler warnings and some bugs
 *            Jun 11, 2020, Bojan Jurca            
 *          - port from SPIFFS to FAT file system, adjustment for Arduino 1.8.13,
 *            support for keep-alive directive
 *            October 10, 2020, Bojan Jurca
 *          - support for HTTP request and response header fields and cookies
 *            February 3, 2021, Bojan Jurca
 *          - MIME types
 *            March 2, Bojan Jurca
 *
 */

#ifndef __WEB_SERVER__
  #define __WEB_SERVER__

  #ifndef HOSTNAME
    #define HOSTNAME "MyESP32Server" // WiFi.getHostname() // use default if not defined
  #endif

  // ----- includes, definitions and supporting functions -----

  #include <WiFi.h>

        // dmesg functionality
        void __webDmesg__ (String message) { 
          #ifdef __TELNET_SERVER__ // use dmesg from telnet server if possible
            dmesg (message);
          #else
                Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
          #endif
        }
        void (* webDmesg) (String) = __webDmesg__; // use this pointer to display / record system messages, if Telnet server is also included it will redirect it to its dmesg command

  #include "common_functions.h"   // stristr, between
  #include "TcpServer.hpp"        // webServer.hpp is built upon TcpServer.hpp  
  #include "user_management.h"    // webServer.hpp needs user_management.h to get www home directory
  #include "file_system.h"        // webServer.hpp needs file_system.h to read files  from home directory
  #include "network.h"            // webServer.hpp needs network.h


/*
 * Basic support for webSockets. A lot of webSocket things are missing. Out of 3 frame sizes only first 2 are 
 * supported, the third is too large for ESP32 anyway. Continuation frames are not supported hence all the exchange
 * information should fit into one frame. Beside that only non-control frames are supported. But all this is just 
 * information for programmers who want to further develop this modul. For those who are just going to use it I'll 
 * try to hide this complexity making the use of webSockets as easy and usefull as possible under given limitations. 
 * See the examples.
 * 
 * WebSocket is a TcpCOnnection with some additional reading and sending functions but we won't inherit it here from
 * TcpConnection since TcpCOnnection already exists at the time WebSocket is beeing created
 * 
 */

  #include "hwcrypto/sha.h"       // needed for websockets support 
  #include "mbedtls/base64.h"     // needed for websockets support

  class WebSocket {  
  
    public:
  
      WebSocket (TcpConnection *connection,     // TCP connection over which webSocket connection communicate with browser
                 String wsRequest               // ws request for later reference if needed
                )                               {
                                                  // make a copy of constructor parameters
                                                  __connection__ = connection;
                                                  __wsRequest__ = wsRequest;

                                                  // debug: Serial.printf ("[websocket debug] %s\n", wsRequest.c_str ());

                                                  // do the handshake with the browser so it would consider webSocket connection established
                                                  int i = wsRequest.indexOf ("Sec-WebSocket-Key: ");
                                                  if (i > -1) {
                                                    int j = wsRequest.indexOf ("\r\n", i + 19);
                                                    if (j > -1) {
                                                      String key = wsRequest.substring (i + 19, j);
                                                      if (key.length () <= 24) { // Sec-WebSocket-Key is not supposed to exceed 24 characters
                                                         
                                                        // calculate Sec-WebSocket-Accept
                                                        char s1 [64]; strcpy (s1, key.c_str ()); strcat (s1, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"); 
                                                        #define SHA1_RESULT_SIZE 20
                                                        unsigned char s2 [SHA1_RESULT_SIZE]; esp_sha (SHA1,(unsigned char*) s1, strlen (s1), s2);
                                                        #define WS_CLIENT_KEY_LENGTH  24
                                                        size_t olen = WS_CLIENT_KEY_LENGTH;
                                                        char s3 [32];
                                                        mbedtls_base64_encode ((unsigned char *) s3, 32, &olen, s2, SHA1_RESULT_SIZE);
                                                        // compose websocket accept reply and send it back to the client
                                                        char buffer  [255]; // this will do
                                                        sprintf (buffer, "HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", s3);
                                                        if (connection->sendData (buffer)) {
                                                          // Serial.printf ("[webSocket] connection confirmed\n");
                                                        } else {
                                                          // log_e ("[webSocket] couldn't send accept key back to browser\n");
                                                        }
                                                      } else { // |key| > 24
                                                        // log_e ("[webSocket] key in wsRequest too long\n");
                                                      }
                                                    } else { // j == -1
                                                      // log_e ("[webSocket] key not found in webRequest\n");
                                                    }
                                                  } else { // i == -1
                                                    // log_e ("[webSocket] key not found in webRequest\n");
                                                  }
                                                  
                                                  // we won't do the checking if everything was sucessfull in constructor,
                                                  // subsequent calls would run into an error enyway in this case so they
                                                  // can handle the errors themselves 

                                                  // TO DO: make constructor return NULL in case of any error
                                                }
      
      ~WebSocket ()                             { // destructor
                                                  if (__payload__) {
                                                    free (__payload__);
                                                    // __payload__ = NULL;
                                                  }
                                                  // send closing frame if possible
                                                  // debug: Serial.printf ("[webSocket] sending closing frame\n");
                                                  __sendFrame__ (NULL, 0, WebSocket::CLOSE);
                                                  closeWebSocket ();
                                                } 

      String getWsRequest ()                    { return __wsRequest__; }

      void closeWebSocket ()                    { __connection__->closeConnection (); }

      bool isClosed ()                          { return __connection__->isClosed (); }

      bool isOpened ()                          { return __connection__->isOpened (); }

      enum WEBSOCKET_DATA_TYPE {
        NOT_AVAILABLE = 0,          // no data is available to be read 
        STRING = 1,                 // text data is available to be read
        BINARY = 2,                 // binary data is available to be read
        CLOSE = 8,        
        ERROR = 3
      };

      WEBSOCKET_DATA_TYPE available ()          { // checks if data is ready to be read, returns the type of data that arrived.
                                                  // All the TCP connection reading is actually done within this function
                                                  // that buffers data that can later be read by using readString () or
                                                  // readBinary () or binarySize () functions. Call it repeatedly until
                                                  // data type is returned.

                                                  switch (__bufferState__) {
                                                    case EMPTY:                   {
                                                                                    // Serial.printf ("[webSocket] EMPTY, preparing data structure\n");
                                                                                    // prepare data structure for the first read operation
                                                                                    __bytesRead__ = 0;
                                                                                    __bufferState__ = READING_SHORT_HEADER;
                                                                                    // continue reading immediately
                                                                                  }
                                                    case READING_SHORT_HEADER:    { 
                                                                                    // Serial.printf ("[webSocket] READING_SHORT_HEADER, reading 6 bytes of header\n");                
                                                                                    switch (__connection__->available ()) {
                                                                                      case TcpConnection::NOT_AVAILABLE:  return WebSocket::NOT_AVAILABLE;
                                                                                      case TcpConnection::ERROR:          return WebSocket::ERROR;
                                                                                      default:                            break;
                                                                                    }
                                                                    
                                                                                    // read 6 bytes of short header
                                                                                    if (6 != (__bytesRead__ += __connection__->recvData ((char *) __header__ + __bytesRead__, 6 - __bytesRead__))) return WebSocket::NOT_AVAILABLE; // if we haven't got 6 bytes continue reading short header the next time available () is called

                                                                                    // check if this frame type is supported
                                                                                    if (!(__header__ [0] & 0b10000000)) { // check fin bit
                                                                                      Serial.printf ("[webSocket] browser send a frame that is not supported: fin bit is not set\n");
                                                                                      __connection__->closeConnection ();
                                                                                      return WebSocket::ERROR;
                                                                                    }
                                                                                    byte b  = __header__ [0] & 0b00001111; // check opcode, 1 = text, 2 = binary, 8 = close
                                                                                    if (b == WebSocket::CLOSE) {
                                                                                      __connection__->closeConnection ();
                                                                                      Serial.printf ("[webSocket] browser requested to close webSocket\n");
                                                                                      return WebSocket::NOT_AVAILABLE;
                                                                                    }
                                                                                    if (b != WebSocket::STRING && b != WebSocket::BINARY) { 
                                                                                      Serial.printf ("[webSocket] browser send a frame that is not supported: opcode is not text or binary\n");
                                                                                      __connection__->closeConnection ();
                                                                                      return WebSocket::ERROR;
                                                                                    } // NOTE: after this point only TEXT and BINRY frames are processed!
                                                                                    // check payload length that also determines frame type
                                                                                    __payloadLength__ = __header__ [1] & 0b01111111; // byte 1: mask bit is always 1 for packets thet came from browsers, cut it off
                                                                                    if (__payloadLength__ <= 125) { // short payload
                                                                                          __mask__ = __header__ + 2; // bytes 2, 3, 4, 5
                                                                                          if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                                                                            __connection__->closeConnection ();
                                                                                            Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                                                            return WebSocket::ERROR;                                                                                            
                                                                                          }
                                                                                          // continue with reading payload immediatelly
                                                                                          __bufferState__ = READING_PAYLOAD;
                                                                                          __bytesRead__ = 0; // reset the counter, count only payload from now on
                                                                                          goto readingPayload;
                                                                                    } else if (__payloadLength__ == 126) { // 126 means medium payload, read additional 2 bytes of header
                                                                                          __bufferState__ = READING_MEDIUM_HEADER;
                                                                                          // continue reading immediately
                                                                                    } else { // 127 means large data block - not supported since ESP32 doesn't have enough memory
                                                                                          Serial.printf ("[webSocket] browser send a frame that is not supported: payload is larger then 65535 bytes\n");
                                                                                          __connection__->closeConnection ();
                                                                                          return WebSocket::ERROR;                         
                                                                                    }
                                                                                  }
                                                    case READING_MEDIUM_HEADER:   { 
                                                                                    // Serial.printf ("[webSocket] READING_MEDIUM_HEADER, reading additional 2 bytes of header\n");
                                                                                    // we don't have to repeat the checking already done in short header case, just read additiona 2 bytes and correct data structure

                                                                                    // read additional 2 bytes (8 altogether) bytes of medium header
                                                                                    if (8 != (__bytesRead__ += __connection__->recvData ((char *) __header__ + __bytesRead__, 8 - __bytesRead__))) return WebSocket::NOT_AVAILABLE; // if we haven't got 8 bytes continue reading medium header the next time available () is called
                                                                                    // correct internal structure for reading into extended buffer and continue at FILLING_EXTENDED_BUFFER immediately
                                                                                    __payloadLength__ = __header__ [2] << 8 | __header__ [3];
                                                                                    __mask__ = __header__ + 4; // bytes 4, 5, 6, 7
                                                                                    if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                                                                      __connection__->closeConnection ();
                                                                                      Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                                                      return WebSocket::ERROR;                                                                                            
                                                                                    }
                                                                                    __bufferState__ = READING_PAYLOAD;
                                                                                    __bytesRead__ = 0; // reset the counter, count only payload from now on
                                                                                    // continue with reading payload immediatelly
                                                                                  }
                                                    case READING_PAYLOAD:         
readingPayload:                                                    
                                                                                  // Serial.printf ("[webSocket] READING_PAYLOAD, reading %i bytes of payload\n", __payloadLength__);
                                                                                  {
                                                                                    // read all payload bytes
                                                                                    if (__payloadLength__ != (__bytesRead__ += __connection__->recvData ((char *) __payload__ + __bytesRead__, __payloadLength__ - __bytesRead__))) {
                                                                                      return WebSocket::NOT_AVAILABLE; // if we haven't got all payload bytes continue reading the next time available () is called
                                                                                    }
                                                                                    // all is read, decode (unmask) the data
                                                                                    for (int i = 0; i < __payloadLength__; i++) __payload__ [i] = (__payload__ [i] ^ __mask__ [i % 4]);
                                                                                    // conclude payload with 0 in case this is going to be interpreted as text - like C string
                                                                                    __payload__ [__payloadLength__] = 0;
                                                                                    __bufferState__ = FULL;     // stop reading until buffer is read by the calling program
                                                                                    return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary
                                                                                  }

                                                    case FULL:                    // return immediately, there is no space left to read new incoming data
                                                                                  // Serial.printf ("[webSocket] FULL, waiting for calling program to fetch the data\n");
                                                                                  return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary 
                                                  }
                                                  // for every case that has not been handeled earlier return not available
                                                  return NOT_AVAILABLE;
                                                }

      String readString ()                      { // reads String that arrived from browser (it is a calling program responsibility to check if data type is text)
                                                  // returns "" in case of communication error
                                                  while (true) {
                                                    switch (available ()) {
                                                      case WebSocket::NOT_AVAILABLE:  delay (1);
                                                                                      break;
                                                      case WebSocket::STRING:         { // Serial.printf ("readString: binary size = %i, buffer state = %i, available = %i\n", binarySize (), __bufferState__, available ());
                                                                                        String s;
                                                                                        // if (__bufferState__ == FULL && __payload__) { // double check ...
                                                                                          s = String ((char *) __payload__); 
                                                                                          free (__payload__);
                                                                                          __payload__ = NULL;
                                                                                          __bufferState__ = EMPTY;
                                                                                        // } else {
                                                                                        //   s = "";
                                                                                        // }
                                                                                        return s;
                                                                                      }
                                                      default:                        return ""; // WebSocket::BINARY or WebSocket::ERROR
                                                    }                                                    
                                                  }
                                                }

      size_t binarySize ()                      { // returns how many bytes has arrived from browser, 0 if data is not ready (yet) to be read
                                                  return __bufferState__ == FULL ? __payloadLength__ : 0;                                                    return 0;
                                                }

      size_t readBinary (byte *buffer, size_t bufferSize) { // returns number bytes copied into buffer
                                                            // returns 0 if there is not enough space in buffer or in case of communication error
                                                  while (true) {
                                                    switch (available ()) {
                                                      case WebSocket::NOT_AVAILABLE:  delay (1);
                                                                                      break;
                                                      case WebSocket::BINARY:         { // Serial.printf ("readBinary: binary size = %i, buffer state = %i, available = %i\n", binarySize (), __bufferState__, available ());
                                                                                        size_t l = binarySize ();
                                                                                        // if (__bufferState__ == FULL && __payload__) { // double check ...
                                                                                          if (bufferSize >= l) 
                                                                                            memcpy (buffer, __payload__, l);
                                                                                          else
                                                                                            l = 0;
                                                                                          free (__payload__);
                                                                                          __payload__ = NULL;
                                                                                          __bufferState__ = EMPTY;
                                                                                        // } else {
                                                                                        //   l = 0;
                                                                                        // }
                                                                                        return l; 
                                                                                      }
                                                      default:                        return 0; // WebSocket::STRING or WebSocket::ERROR
                                                    }                                                    
                                                  }
                                                }
                                                
      bool sendString (String text)             { // returns success
                                                  return __sendFrame__ ((byte *) text.c_str (), text.length (), WebSocket::STRING);
                                                }

      bool sendBinary (byte *buffer, size_t bufferSize) { // returns success
                                                  return __sendFrame__ (buffer, bufferSize, WebSocket::BINARY);
                                                }
                                                
    private:

      bool __sendFrame__ (byte *buffer, size_t bufferSize, WEBSOCKET_DATA_TYPE dataType) { // returns true if frame have been sent successfully
                                                if (bufferSize > 0xFFFF) { // this size fits in large frame size - not supported
                                                  Serial.printf ("[webSocket] trying to send a frame that is not supported: payload is larger then 65535 bytes\n");
                                                  __connection__->closeConnection ();
                                                  return false;                         
                                                } 
                                                byte *frame = NULL;
                                                int frameSize;
                                                if (bufferSize > 125) { // medium frame size
                                                  if (!(frame = (byte *) malloc (frameSize = 4 + bufferSize))) { // 4 bytes for header (without mask) + payload
                                                    Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                    __connection__->closeConnection ();
                                                    return false;
                                                  }
                                                  // frame type
                                                  frame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
                                                  frame [1] = 126; // medium frame size, without masking (we won't do the masking, won't set the MASK bit)
                                                  frame [2] = bufferSize >> 8; // / 256;
                                                  frame [3] = bufferSize; // % 256;
                                                  memcpy (frame + 4, buffer, bufferSize);  
                                                } else { // small frame size
                                                  if (!(frame = (byte *) malloc (frameSize = 2 + bufferSize))) { // 2 bytes for header (without mask) + payload
                                                    Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                    __connection__->closeConnection ();
                                                    return false;
                                                  }
                                                  frame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
                                                  frame [1] = bufferSize; // small frame size, without masking (we won't do the masking, won't set the MASK bit)
                                                  if (bufferSize) memcpy (frame + 2, buffer, bufferSize);  
                                                }
                                                if (__connection__->sendData ((char *) frame, frameSize) != frameSize) {
                                                  free (frame);
                                                  __connection__->closeConnection ();
                                                  Serial.printf ("[webSocket] failed to send frame\n");
                                                  return false;
                                                }
                                                free (frame);
                                                return true;
                                              }

      TcpConnection *__connection__;
      String __wsRequest__;

      enum BUFFER_STATE {
        EMPTY = 0,                                // buffer is empty
        READING_SHORT_HEADER = 1,
        READING_MEDIUM_HEADER = 2,
        READING_PAYLOAD = 3,
        FULL = 4                                  // buffer is full and waiting to be read by calling program
      }; 
      BUFFER_STATE __bufferState__ = EMPTY;
      unsigned int __bytesRead__;                 // how many bytes of current frame have been read so far
      byte __header__ [8];                        // frame header: 6 bytes for short frames, 8 for medium (and 12 for large frames - not supported)
      unsigned int __payloadLength__;             // size of payload in current frame
      byte *__mask__;                             // pointer to 4 frame mask bytes
      byte *__payload__ = NULL;                   // pointer to buffer for frame payload
  };


/*
 * httpServer is inherited from TcpServer with connection handler that handles connections according
 * to HTTP protocol.
 * 
 * Connection handler tries to resolve HTTP request in three ways:
 *  1. checks if the request is a WS request and starts WebSocket in this case
 *  2. asks httpRequestHandler provided by the calling program if it is going to provide the reply
 *  3. checks /var/www/html directry for .html file that suits the request
 *  4. replyes with 404 - not found 
 */
  
  class httpServer: public TcpServer {                                             
  
    public:

      // keep www session parameters in a structure - try to keep compatibility with previous version
            
      class wwwSessionParameters {
        public:
          wwwSessionParameters (String *httpRequest, String *homeDirectory, TcpConnection *cnn)  {
                                                                                                    __httpRequest__ = httpRequest;
                                                                                                    homeDir = *homeDirectory;
                                                                                                    connection = cnn;
                                                                                                  }
          // parameters
          String homeDir; // web server home directory
          TcpConnection *connection;
          // feel free to add more
          // reading HTTP request
          String getHttpRequestHeaderField (String fieldName) { // HTTP header fields are in format \r\nfieldName: fieldValue\r\n
                                                                char *httpRequest = (char *) (*__httpRequest__).c_str ();
                                                                char *p = stristr (httpRequest, (char *) ("\n" + fieldName + ":").c_str ()); // find field name in HTTP header
                                                                if (p) {
                                                                  p += 1 + fieldName.length () + 1; // pass \n fieldname and :
                                                                  char *q = strstr (p, "\r"); // find end of line - should always succeed
                                                                  if (q) {
                                                                    String fieldValue = __httpRequest__->substring (p - httpRequest, q - httpRequest);
                                                                    fieldValue.trim ();
                                                                    return fieldValue;
                                                                  }
                                                                }
                                                                return "";
                                                              }
          String getHttpRequestCookie (String cookieName) { // cookies are passed from browser to http server in "cookie" HTTP header field
                                                            String cookies = " " + getHttpRequestHeaderField ("Cookie") + ";";
                                                            return between (cookies, " " + cookieName + "=", ";");
                                                          }
          // setting HTTP response
          String httpResponseStatus = "200 OK"; // by default
          void setHttpResponseHeaderField (String fieldName, String fieldValue) { httpResponseHeaderFields += fieldName + ":" + fieldValue + "\r\n"; }
          void setHttpResponseCookie (String cookieName, String cookieValue, time_t expires = 0, String path = "/") { 
                                                                                                                      char e [50] = "";
                                                                                                                      if (expires) {
                                                                                                                        struct tm st = timeToStructTime (expires);
                                                                                                                        strftime (e, sizeof (e), "; Expires=%a, %d %b %Y %H:%M:%S GMT", &st);
                                                                                                                      }
                                                                                                                      setHttpResponseHeaderField ("Set-Cookie", cookieName + "=" + cookieValue + "; Path=" + path + String (e));
                                                                                                                    }
        private:
          friend class httpServer;
          // remember HTTP request
          String *__httpRequest__;
          // construct HTTP reply
          String httpResponseHeaderFields = "";
      };

      httpServer (String (*httpRequestHandler) (String& httpRequest, httpServer::wwwSessionParameters *wsp),  // httpRequestHandler callback function provided by calling program
                  void (*wsRequestHandler) (String& wsRequest, WebSocket *webSocket),                         // httpRequestHandler callback function provided by calling program      
                  unsigned int stackSize,                                                                     // stack size of httpRequestHandler thread, usually 4 KB will do 
                  char *serverIP,                                                                             // web server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                  int serverPort,                                                                             // web server port
                  bool (*firewallCallback) (char *)                                                           // a reference to callback function that will be celled when new connection arrives 
                 ): TcpServer (__webConnectionHandler__, this, stackSize, 1500, serverIP, serverPort, firewallCallback)
                                {
                                  __externalHttpRequestHandler__ = httpRequestHandler;
                                  __externalWsRequestHandler__ = wsRequestHandler; 
                                  __webServerHomeDirectory__ = getUserHomeDirectory ("webserver"); 
                                  if (__webServerHomeDirectory__ == "") { 
                                    webDmesg ("[httpServer] home directory for webserver system account is not set.");
                                    return;
                                  }
                                  __started__ = true; // we have initialized everything needed for TCP connection
                                  if (started ()) webDmesg ("[httpServer] started on " + String (serverIP) + ":" + String (serverPort) + (firewallCallback ? " with firewall." : "."));
                                }
      
      ~httpServer ()            { if (started ()) webDmesg ("[httpServer] stopped."); }
      
      bool started ()           { return TcpServer::started () && __started__; } 

      String getHomeDirectory () { return __webServerHomeDirectory__; }

    private:

      String (*__externalHttpRequestHandler__) (String& httpRequest, httpServer::wwwSessionParameters *wsp);  // httpRequestHandler callback function provided by calling program
      void (*__externalWsRequestHandler__) (String& wsRequest, WebSocket *webSocket);                         // wsRequestHandler callback function provided by calling program
      String __webServerHomeDirectory__ = "";                                                                 // webServer system account home directory
      bool __started__ = false;

      static void __webConnectionHandler__ (TcpConnection *connection, void *thisWebServer) {  // connectionHandler callback function
        httpServer *ths = (httpServer *) thisWebServer; // this is how you pass "this" pointer to static memeber function

        char buffer [2048 + 1]; *buffer = 0;
        String incomingRequests = "";
        int received;
        while ((received = connection->recvData (buffer, sizeof (buffer) - 1))) { // read block of incoming HTTP request(s)
          // Serial.printf ("   DEBUG {socket %i} received %i bytes\n", connection->getSocket (), received );
          buffer [received] = 0;
          // append it to already existing part of HTTP request
          incomingRequests += String (buffer);
          // check if the end of HTTP request has already arrived
          int i = incomingRequests.indexOf ("\r\n\r\n");
          if (i >= 0) {
            // Serial.printf ("   DEBUG {socket %i} received HTTP request\n", connection->getSocket () );
            String httpRequest = incomingRequests.substring (0, i + 4);
            wwwSessionParameters wsp (&httpRequest, &ths->__webServerHomeDirectory__, connection);

            // we have got HTTP request
            // 1st check if it is ws request - then cal WS handler
            // 2nd check if exterrnal HTTP handler (provided by calling program) is going to handle it
            // 3rd handle HTTP request internaly:
            //    - if it is a HTML file name then reply with file content
            //    - reply with error 404 if it is not

            // debug: Serial.printf ("[httpServer debug] %s\n", buffer);
            if (stristr (buffer, (char *) "UPGRADE: WEBSOCKET")) {
              connection->setTimeOut (300000); // set time-out to 5 minutes for WebSockets (by default if is 1.5 s for HTTP requests)
              WebSocket *webSocket = new WebSocket (connection, httpRequest); 
              if (webSocket && webSocket->isOpened ()) { // check success
                if (ths->__externalWsRequestHandler__) ths->__externalWsRequestHandler__ (httpRequest, webSocket);
                delete (webSocket);
              } else {
                webDmesg ("[httpServer] can't open WebSocket.");
              }
              return; // close this connection
            }

            String httpResponseContent;
            if (ths->__externalHttpRequestHandler__ && (httpResponseContent = ths->__externalHttpRequestHandler__ (httpRequest, &wsp)) != "") {
              if (!stristr ((char *) wsp.httpResponseHeaderFields.c_str (), (char *) "CONTENT-TYPE")) { // add Content-Type if it is not set yet
                     if (stristr ((char *) httpResponseContent.c_str (), (char *) "<HTML>")) wsp.setHttpResponseHeaderField ("Content-Type", "text/html"); // try guessing if Content-Type is HTML ...
                else if (strstr (httpResponseContent.c_str (), "{"))                         wsp.setHttpResponseHeaderField ("Content-Type", "application/json");
                // ... add more if needed but Contet-Type can often be omitted without problems ...                
                else                                                                         wsp.setHttpResponseHeaderField ("Content-Type", "text/plain"); // ... or just say it is a plain text
              }
              // debug: Serial.println ("[httpServer debug] HTTP/1.1 " + wsp.httpResponseStatus + "\r\n" + wsp.httpResponseHeaderFields + "Content-Length:" + String (httpResponseContent.length ()) + "\r\n\r\n" + httpResponseContent);
              connection->sendData ("HTTP/1.1 " + wsp.httpResponseStatus + "\r\n" + wsp.httpResponseHeaderFields + "Content-Length:" + String (httpResponseContent.length ()) + "\r\n\r\n" + httpResponseContent);
            } else {
              connection->sendData (ths->__internalHttpRequestHandler__ (httpRequest, &wsp)); // send reply to browser
            }

            // if the client wants to keep connection alive for the following requests then let it be so
            if (stristr ((char *) httpRequest.c_str (), (char *) "CONNECTION: KEEP-ALIVE")) {
              incomingRequests = incomingRequests.substring (i + 4); // read another request on this connection
            } else {
              break; // close this connection
            }
          }
        } // while
      }

      String __internalHttpRequestHandler__ (String& httpRequest, httpServer::wwwSessionParameters *wsp) { 
        // check if HTTP request is file name or report error 404

        #ifdef __FILE_SYSTEM__
          if (__fileSystemMounted__) {

            int i = httpRequest.indexOf (' ', 4);
            if (i >= 0) {
              String fileName = httpRequest.substring (5, i);
              if (fileName == "") fileName = "index.html";
              fileName = __webServerHomeDirectory__ + fileName;
              if (!stristr ((char *) wsp->httpResponseHeaderFields.c_str (), (char *) "CONTENT-TYPE")) { // add Content-Type if it is not set yet
                     if (fileName.endsWith (".bmp"))                                wsp->setHttpResponseHeaderField ("Content-Type", "image/bmp");
                else if (fileName.endsWith (".css"))                                wsp->setHttpResponseHeaderField ("Content-Type", "text/css");
                else if (fileName.endsWith (".csv"))                                wsp->setHttpResponseHeaderField ("Content-Type", "text/csv");
                else if (fileName.endsWith (".gif"))                                wsp->setHttpResponseHeaderField ("Content-Type", "image/gif");
                else if (fileName.endsWith (".htm") || fileName.endsWith (".html")) wsp->setHttpResponseHeaderField ("Content-Type", "text/html");
                else if (fileName.endsWith (".jpg") || fileName.endsWith (".jpeg")) wsp->setHttpResponseHeaderField ("Content-Type", "image/jpeg");
                else if (fileName.endsWith (".js"))                                 wsp->setHttpResponseHeaderField ("Content-Type", "text/javascript");
                else if (fileName.endsWith (".json"))                               wsp->setHttpResponseHeaderField ("Content-Type", "application/json");
                else if (fileName.endsWith (".mpeg"))                               wsp->setHttpResponseHeaderField ("Content-Type", "video/mpeg");
                else if (fileName.endsWith (".pdf"))                                wsp->setHttpResponseHeaderField ("Content-Type", "application/pdf");
                else if (fileName.endsWith (".png"))                                wsp->setHttpResponseHeaderField ("Content-Type", "image/png");
                else if (fileName.endsWith (".tif") || fileName.endsWith (".tiff")) wsp->setHttpResponseHeaderField ("Content-Type", "image/tiff");
                else if (fileName.endsWith (".txt"))                                wsp->setHttpResponseHeaderField ("Content-Type", "text/plain");
                // ... add more if needed but Contet-Type can often be omitted without problems ...
              }
              File f = FFat.open (fileName.c_str (), FILE_READ);           
              if (f) {
                if (!f.isDirectory ()) {
                  char *buff = (char *) malloc (2048); // get 2 KB of memory from heap
                  if (buff) {
                    String httpHeader = "HTTP/1.1 " + wsp->httpResponseStatus + "\r\n" + wsp->httpResponseHeaderFields + "Content-Length:" + String (f.size ()) + "\r\n\r\n";
                    // debug: Serial.printf ("[httpServer debug] %s", httpHeader.c_str ());
                    if (httpHeader.length () > 2046) { // there should normally be enough space, but check anyway
                      f.close ();
                      return "HTTP/1.1 500 Internal server error\r\nContent-Length:29\r\n\r\nError: HTTP header too large."; 
                    }
                    strcpy (buff, httpHeader.c_str ());
                    int i = strlen (buff);
                    while (f.available ()) {
                      *(buff + i++) = f.read ();
                      if (i == 2048) { wsp->connection->sendData ((char *) buff, 2048); i = 0; }
                    }
                    if (i) { wsp->connection->sendData (buff, i); }
                    free (buff);
                  } 
                  f.close ();
                  return ""; // success
                } // if file is a file, not a directory
                f.close ();
              } // if file is opened
            }
          }
        #endif

        return "HTTP/1.1 404 Not found\r\nContent-Length:20\r\n\r\nPage does not exist."; // HTTP header and content
      }     
        
  };

/*  
 * webClient function doesn't really belong to webServer, but it may came handy every now and then  
 */

  String webClient (char *serverIP, int serverPort, unsigned int timeOutMillis, String httpRequest) {
    if (getWiFiMode () == WIFI_OFF) {
      webDmesg ("[webClient] can't start, there is no network.");
      return "";
    }
  
    char *buffer = (char *) malloc (2048); // reserve some space from heap to hold blocks of response
    if (!buffer) {
      webDmesg ("[webClient] can't get heap memory.");
      return "";      
    }
    *buffer = 0;
    
    String retVal = ""; // place for response
    // create non-threaded TCP client instance
    TcpClient myNonThreadedClient (serverIP, serverPort, timeOutMillis); 
    // get reference to TCP connection. Before non-threaded constructor of TcpClient returns the connection is established if this is possible
    TcpConnection *myConnection = myNonThreadedClient.connection ();
    // test if connection is established
    if (myConnection) {
      httpRequest += " \r\n\r\n"; // make sure HTTP request ends properly
      /* int sentTotal = */ myConnection->sendData (httpRequest); // send HTTP request
      // Serial.printf ("[%10lu] [webClient] sent %i bytes.\n", millis (), sentTotal);
      // read response in a loop untill 0 bytes arrive - this is a sign that connection has ended 
      // if the response is short enough it will normally arrive in one data block although
      // TCP does not guarantee that it would
      int receivedTotal = 0;
      while (int received = myConnection->recvData (buffer, 2048 - 1)) {
        receivedTotal += received;
        buffer [received] = 0; // mark the end of the string we have just read
        retVal += String (buffer);
        // Serial.printf ("[%10lu] [webClient] received %i bytes.\n", millis (), received);
        // search buffer for content-lenght: to check if the whole reply has arrived against receivedTotal
        char *s = (char *) retVal.c_str ();
        char *c = stristr (s, (char *) "CONTENT-LENGTH:");
        if (c) {
          unsigned long ul;
          if (sscanf (c + 15, "%lu", &ul) == 1) {
            // Serial.printf ("[%10lu] [webClient] content-length %lu, receivedTotal %i.\n", millis (), ul, receivedTotal);
            if ((c = strstr (c + 15, "\r\n\r\n"))) 
              if (receivedTotal == ul + c - s + 4) {
                // Serial.printf ("[%10lu] [webClient] whole HTTP response of content-length %lu bytes and total length of %u bytes has arrived.\n", millis (), ul, retVal.length ());
                free (buffer);
                return retVal; // the response is complete
              }
          }
        }
      }
      if (receivedTotal) webDmesg ("[webClient] error in HTTP response regarding content-length, httpRequest = " + httpRequest);
      else               webDmesg ("[webClient] time-out, httpRequest = " + httpRequest);
    } else {
      webDmesg ("[webClient] unable to connect to " + String (serverIP) + " on port " + String (serverPort) + ", httpRequest = " + httpRequest);
    }     
    free (buffer);
    return ""; // response arrived, it may even be OK but it doesn't match content-length field
  }
 
#endif
