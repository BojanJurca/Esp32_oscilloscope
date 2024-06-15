/*

    httpServer.hpp 
  
    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
  
    HTTP server can serve some HTTP requests itself (for example content of .html and other files) but the calling program
    can also provide its own httpRequestHandlerCallback function. Cookies and page redirection are supported. There is also
    a small "database" to keep valid web session tokens in order to support web login. Text and binary WebSocket straming is
    also supported.
  
    May 22, 2024, Bojan Jurca

    Nomenclature used here for easier understaning of the code:

     - "connection" normally applies to TCP connection from when it was established to when it is terminated

                  Looking back to HTTP 1.0 protocol one TCP connection was used to transmit oparamene HTTP request from browser to web server
                  and then HTTP response back to the browser. After this TCP connection was terminated. HTTP 1.1 introduced "keep-alive"
                  directive. When the browser requests several pages from server it can do it over the same TCP connection in short time period 
                  (one after another) reducing the need of establishing and terminating TCP connections several times.   

     - "session" applies to user interaction between login and logout

                  Taking account the web technology one web session tipically consists of many TCP connections. Cookies are involved
                  in order to preserve the state of the web session since HTTP requests and replies themselves are stateless. A list of 
                  valid "session tokens" is keept on the web server so web server can check the session validity.

      - "buffer size" is the number of bytes that can be placed in a buffer. 
      
                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".


      - "HTTP request":

                   ---------------  GET / HTTP/1.1
                  |                 Host: 10.18.1.200
                  |                 Connection: keep-alive    field value
                  |                 Cache-Control: max-age=0   |
         request  |    field name - Upgrade-Insecure-Requests: 1
                  |                 User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.45 Safari/537.36
                  |                 Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng;q=0.8,application/signed-exchange;v=b3;q=0.9
                  |                 Accept-Encoding: gzip, deflate
                  |                 Accept-Language: sl-SI,sl;q=0.9,en-GB;q=0.8,en;q=0.7
                  |                 Cookie: refreshCounter=1
                  |                             |          |
                  |                        cookie name   cookie value
                   ---------------     
       
      - "HTTP reply":
                                                    status
                                                       |         cookie name, value, path and expiration
                   --------  ---------------  HTTP/1.1 200 OK      |     |    |         |
                  |         |                 Set-Cookie: refreshCounter=2; Path=/; Expires=Thu, 09 Dec 2021 19:07:04 GMT
            reply |  header |    field name - Content-Type: text/html
                  |         |                 Content-Length: 96   |
                  |          ---------------                      field value
                   - content ---------------  <HTML>Web cookies<br><br>This page has been refreshed 2 times. Click refresh to see more.</HTML>
                  
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    // needed for web sockets
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL (4, 4, 0) // Checks if board manager version for esp32 is >= 2.0.0 - thank you Ziss (https://github.com/Ziss)
      #include <sha/sha_parallel_engine.h>    // needed for websockets support
    #else
      #include <hwcrypto/sha.h>               // needed for websockets support
    #endif
    #include <mbedtls/base64.h>
    #include <mbedtls/md.h>
    // fixed size strings    
    #include "std/Cstring.hpp"
                                                                

#ifndef __HTTP_SERVER__
  #define __HTTP_SERVER__

    #ifdef HTTP_SERVER_CORE
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "httpServer will run only on #defined core"
        #endif
    #endif

    #ifndef __FILE_SYSTEM__
      #ifdef SHOW_COMPILE_TIME_INFORMATION
          #pragma message "Compiling httpServer.h without file system (fileSystem.hpp), httpServer will not be able to serve files"
      #endif
    #endif
  

    // ----- TUNNING PARAMETERS -----

    #ifndef HTTP_CONNECTION_STACK_SIZE
        #define HTTP_CONNECTION_STACK_SIZE 8 * 1024             // TCP connections' stack size
    #endif
    // #define HTTP_SERVER_CORE 1 // 1 or 0                     // #define HTTP_SERVER_CORE if you want httpServer to run on specific core
    #define HTTP_BUFFER_SIZE 1500                               // reading and temporary keeping HTTP requests, buffer for constructing HTTP reply, MTU = 1500
    #define HTTP_CONNECTION_TIME_OUT 3000                       // 3000 ms for HTTP requests, 0 for infinite
    #define HTTP_REPLY_STATUS_MAX_LENGTH 32                     // only first 3 characters are important
    #define HTTP_WS_FRAME_MAX_SIZE 1500                         // WebSocket send frame size and also read frame size (there are 2 buffers), MTU = 1500
    #define HTTP_CONNECTION_WS_TIME_OUT 300000                  // 300000 ms = 5 min for WebSocket connections, 0 for infinite
    // #define WEB_SESSIONS                                     // comment this line out if you won't use web sessions
    #ifdef WEB_SESSIONS
        #define WEB_SESSION_TIME_OUT 300                        // 300 s = 5 min, 0 for infinite

        #include "keyValueDatabase.hpp"

        typedef Cstring<64> webSessionToken_t;
        struct webSessionTokenInformation_t {
            time_t expires; // web session toke expiration time in GMT
            Cstring<64> userName; // USER_PASSWORD_MAX_LENGTH = 64
        };

        keyValueDatabase<webSessionToken_t, webSessionTokenInformation_t> webSessionTokenDatabase; // a database containing valid web session tokens

    #endif

    // please note that the limit for getHttpRequestHeaderField and getHttpRequestCookie return values are defined by cstring #definition in Cstring.h

    #define reply400 "HTTP/1.0 400 Bad request\r\nConnection: close\r\nContent-Length:34\r\n\r\nFormat of HTTP request is invalid."
    #define reply404 "HTTP/1.0 404 Not found\r\nConnection: close\r\nContent-Length:10\r\n\r\nNot found."
    #define reply503 "HTTP/1.0 503 Service unavailable\r\nConnection: close\r\nContent-Length:39\r\n\r\nHTTP server is not available right now."
    #define reply507 "HTTP/1.0 507 Insuficient storage\r\nConnection: close\r\nContent-Length:41\r\n\r\nThe buffers of HTTP server are too small."


    // ----- CODE -----

    // #include "dmesg.hpp"
    #ifndef __TIME_FUNCTIONS__
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Implicitly including time_functions.h (needed to calculate expiration time of cookies)"
        #endif
      #include "time_functions.h"
    #endif
    
    
    #ifndef __SHA256__
      #define __SHA256__
        bool sha256 (char *buffer, size_t bufferSize, const char *clearText) { // converts clearText to 265 bit SHA, returns character representation in hexadecimal format of hash value
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


    // control httpServer critical sections
    static SemaphoreHandle_t __httpServerSemaphore__ = xSemaphoreCreateMutex (); 

    // log what is going on within httpServer
    byte httpServerConcurentTasks = 0;
    byte httpServerConcurentTasksHighWatermark = 0;

    // ----- WebSocket class -----
  
    class WebSocket {  
    
      public:

        // WebSocket state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          RUNNING = 2        
        };

        STATE_TYPE state () { return __state__; }
    
        WebSocket ( // the following parameters will be handeled by WebSocket instance
                    int connectionSocket,
                    char *wsRequest,
                    char *clientIP, char *serverIP
                  ) {
                      // create a local copy of parameters for later use
                      __connectionSocket__ = connectionSocket;
                      __wsRequest__ = wsRequest;
                      __clientIP__ = clientIP;
                      __serverIP__ = serverIP;

                      // do the handshake with the browser so it would consider webSocket connection established
                      char *i = stristr (wsRequest, (char *) "SEC-WEBSOCKET-KEY: ");
                      if (i) {
                        i += 19;
                        char *j = strstr (i, "\r\n");
                        if (j) {
                          if (j - i <= 24) { // Sec-WebSocket-Key is not supposed to exceed 24 characters
                            char key [25]; memcpy (key, i, j - i); key [j - i] = 0; 
                            // calculate Sec-WebSocket-Accept
                            char s1 [64]; strcpy (s1, key); strcat (s1, (char *) "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"); 
                            #define SHA1_RESULT_SIZE 20
                            unsigned char s2 [SHA1_RESULT_SIZE]; esp_sha (SHA1, (unsigned char *) s1, strlen (s1), s2);
                            #define WS_CLIENT_KEY_LENGTH 24
                            size_t olen = WS_CLIENT_KEY_LENGTH;
                            char s3 [32];
                            mbedtls_base64_encode ((unsigned char *) s3, 32, &olen, s2, SHA1_RESULT_SIZE);
                            // compose websocket accept response and send it back to the client
                            char buffer  [255]; // 255 will do
                            sprintf (buffer, "HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", s3);
                            if (sendAll (connectionSocket, buffer, HTTP_CONNECTION_WS_TIME_OUT) == strlen (buffer)) {
                              __state__ = RUNNING;
                            }
                          } else { // |key| > 24
                              #ifdef __DMESG__
                                  dmesgQueue << "[WebSocket] WsRequest key too long";
                              #endif
                          }
                        } else { // j == NULL
                            #ifdef __DMESG__
                                dmesgQueue << "[WebSocket] WsRequest without key";
                            #endif
                        }
                      } else { // i == NULL
                          #ifdef __DMESG__
                              dmesgQueue << "[WebSocket] WsRequest without key";
                          #endif
                      }

                    }
          
        ~WebSocket () { 
                        // send closing frame if possible
                        __sendFrame__ (NULL, 0, WebSocket::CLOSE);
                        closeWebSocket ();
                      } 

        int getSocket () { return __connectionSocket__; }

        char *getClientIP () { return __clientIP__; }

        char *getServerIP () { return __serverIP__; }
  
        void closeWebSocket ()  { 
                                  // close connection socket
                                  int connectionSocket;
                                  xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                                    connectionSocket = __connectionSocket__;
                                    __connectionSocket__ = -1;
                                  xSemaphoreGive (__httpServerSemaphore__);
                                  if (connectionSocket > -1) close (connectionSocket);                      
                                }

        char *getWsRequest () { return __wsRequest__; }
    
        enum WEBSOCKET_DATA_TYPE {
          NOT_AVAILABLE = 0,          // no data is available to be read 
          STRING = 1,                 // text data is available to be read
          BINARY = 2,                 // binary data is available to be read
          CLOSE = 8,                  // used internally for closing frame      
          ERROR = 10,                 // error in TCP communication
          TIME_OUT = 11               // time-out detected
        };
   
        WEBSOCKET_DATA_TYPE available ()  { // checks if data is ready to be read, returns the type of data that has arrived.
                                            // All the TCP connection reading is actually done within this function
                                            // that buffers the data that can later be read by using readString () or
                                            // readBinary () or binarySize () functions. Call it repeatedly until
                                            // some other data type is returned.

          switch (__readFrameState__) {
            case EMPTY:                   {
                                            // prepare data structure for the first read operation
                                            __bytesRead__ = 0;
                                            __readFrameState__ = READING_SHORT_HEADER;
                                            // continue reading immediately
                                          }
            case READING_SHORT_HEADER:    { 
                                            // check socket if data is pending to be read
                                            char c; int i = recv (__connectionSocket__, &c, sizeof (c), MSG_PEEK);
                                            if (i == -1) {
                                              if ((errno == EAGAIN || errno == ENAVAIL) && (millis () - __lastActive__ < HTTP_CONNECTION_WS_TIME_OUT || !HTTP_CONNECTION_WS_TIME_OUT)) {
                                                return WebSocket::NOT_AVAILABLE; // not time-out yet
                                              } else {
                                                closeWebSocket ();
                                                return WebSocket::TIME_OUT;
                                              }
                                            }
                                            // else - something is available to be read

                                            // read 6 bytes of short header
                                            i = recv (__connectionSocket__, __readFrame__ + __bytesRead__, 6 - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            }

                                            additionalNetworkInformation.bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
                                                                                           
                                            __lastActive__ = millis ();
                                            if (6 != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got 6 bytes continue reading short header the next time available () is called
                                            // check if this frame type is supported
                                            if (!(__readFrame__ [0] & 0b10000000)) { // check fin bit
                                                #ifdef __DMESG__
                                                    dmesgQueue << "[WebSocket] frame type not supported";
                                                #endif
                                                closeWebSocket ();
                                                return WebSocket::ERROR;
                                            }
                                            byte b  = __readFrame__ [0] & 0b00001111; // check opcode, 1 = text, 2 = binary, 8 = close
                                            if (b == WebSocket::CLOSE) {
                                              // dmesg ("[WebSocket] closed by peer");
                                              closeWebSocket ();
                                              return WebSocket::CLOSE;
                                            }
                                            if (b != WebSocket::STRING && b != WebSocket::BINARY) { 
                                                #ifdef __DMESG__
                                                    dmesgQueue << "[WebSocket] only STRING and BINARY frame types are supported";
                                                #endif
                                                closeWebSocket ();
                                                return WebSocket::ERROR;
                                            } // NOTE: after this point only TEXT and BINRY frames are processed!
                                            // check payload length that also determines frame type
                                            __readBufferSize__ = __readFrame__ [1] & 0b01111111; // byte 1: mask bit is always 1 for packets that come from browsers, cut it off
                                            if (__readBufferSize__ <= 125) { // short payload
                                                  __mask__ = __readFrame__ + 2; // bytes 2, 3, 4, 5
                                                  __readBuffer__  = __readFrame__ + 6; // skip 6 bytes of header, note that __readFrame__ is large enough
                                                  // continue with reading payload immediatelly
                                                  __readFrameState__ = READING_PAYLOAD;
                                                  __bytesRead__ = 0; // reset the counter, count only payload from now on
                                                  goto readingPayload;
                                            } else if (__readBufferSize__ == 126) { // 126 means medium payload, read additional 2 bytes of header
                                                  __readFrameState__ = READING_MEDIUM_HEADER;
                                                  // continue reading immediately
                                            } else { // 127 means large data block - not supported since ESP32 doesn't have enough memory
                                                  closeWebSocket ();
                                                  return WebSocket::ERROR;                         
                                            }
                                          }
            case READING_MEDIUM_HEADER:   { 
                                            // we don't have to repeat the checking already done in short header case, just read additiona 2 bytes and update the data structure
                                            // read additional 2 bytes (8 altogether) bytes of medium header
                                            int i = recv (__connectionSocket__, __readFrame__ + __bytesRead__, 8 - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            }

                                            additionalNetworkInformation.bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();

                                            __lastActive__ = millis ();
                                            if (8 != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got 8 bytes continue reading medium header the next time available () is called
                                            // correct internal structure for reading into extended buffer and continue at FILLING_EXTENDED_BUFFER immediately
                                            __readBufferSize__ = __readFrame__ [2] << 8 | __readFrame__ [3];
                                            __mask__ = __readFrame__ + 4; // bytes 4, 5, 6, 7
                                            __readBuffer__  = __readFrame__ + 8; // skip 8 bytes of header, check if __readFrame is large enough:
                                            if (__readBufferSize__ > HTTP_WS_FRAME_MAX_SIZE - 8) {
                                                #ifdef __DMESG__
                                                    dmesgQueue << "[WebSocket] can only receive frames of up to " << HTTP_WS_FRAME_MAX_SIZE - 8 << " payload bytes";
                                                #endif
                                                closeWebSocket ();
                                                return WebSocket::ERROR;
                                            }
                                            __readFrameState__ = READING_PAYLOAD;
                                            __bytesRead__ = 0; // reset the counter, count only payload from now on
                                            // continue with reading payload immediatelly
                                          }
            case READING_PAYLOAD:         
        readingPayload:                                                    
                                          {
                                            // read all payload bytes
                                            int i = recv (__connectionSocket__, __readBuffer__ + __bytesRead__, __readBufferSize__ - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              __readFrameState__ = EMPTY;
                                              return WebSocket::ERROR;
                                            }

                                            additionalNetworkInformation.bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].bytesReceived += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            additionalSocketInformation [__connectionSocket__ - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
                                        
                                            if (__readBufferSize__ != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got all payload bytes continue reading the next time available () is called
                                            __lastActive__ = millis ();
                                            // all is read, decode (unmask) the data
                                            for (int i = 0; i < __readBufferSize__; i++) __readBuffer__ [i] = (__readBuffer__ [i] ^ __mask__ [i % 4]);
                                            // conclude payload with 0 in case this is going to be interpreted as text - like C string
                                            __readBuffer__ [__readBufferSize__] = 0;
                                            __readFrameState__ = FULL;     // stop reading until buffer is read by the calling program
                                            return __readFrame__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary
                                          }

            case FULL:                    // return immediately, there is no space left to read additional incoming data
                                          return __readFrame__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary 
          }
          // for every case that has not been handeled earlier return not available
          return NOT_AVAILABLE;
        }

        cstring readString () { // reads string that arrived from browser (it is a calling program responsibility to check if data type is text)
                               // returns "" in case of communication error
          while (true) {
            switch (available ()) {
              case WebSocket::NOT_AVAILABLE:  delay (1);
                                              break;
              case WebSocket::STRING:         { 
                                                cstring s ((char *) __readBuffer__); 
                                                __readFrameState__ = EMPTY;
                                                return s;
                                              }
              default:                        return ""; // WebSocket::BINARY, WebSocket::ERROR, WebSocket::CLOSE, WebSocket::TIME_OUT
            }                                                    
          }
        }

        size_t binarySize () { return __readFrameState__ == FULL ? __readBufferSize__ : 0; } // returns how many bytes has arrived from browser, 0 if data is not ready (yet) to be read
                                                    
        size_t readBinary (byte *buffer, size_t bufferSize) { // returns number bytes copied into buffer
                                                              // returns 0 if there is not enough space in buffer or in case of communication error
          while (true) {
            switch (available ()) {
              case WebSocket::NOT_AVAILABLE:  delay (1);
                                              break;
              case WebSocket::BINARY:         { 
                                                size_t l = binarySize ();
                                                if (bufferSize >= l) 
                                                  memcpy (buffer, __readBuffer__, l);
                                                else
                                                  l = 0;
                                                __readFrameState__ = EMPTY;
                                                return l; 
                                              }
              default:                        return 0; // WebSocket::STRING WebSocket::ERROR, WebSocket::CLOSE, WebSocket::TIME_OUT
            }                                                    
          }
        }

        bool sendString (char *text) { return __sendFrame__ ((byte *) text, strlen (text), WebSocket::STRING); } // returns success

        bool sendString (const char *text) { return __sendFrame__ ((byte *) text, strlen (text), WebSocket::STRING); } // returns success
                                                  
        bool sendBinary (byte *buffer, size_t bufferSize) { return __sendFrame__ (buffer, bufferSize, WebSocket::BINARY); } // returns success
        
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        unsigned long __lastActive__ = millis (); // time-out detection

        int __connectionSocket__ = -1;
        char *__wsRequest__ = NULL;
        char *__clientIP__ = NULL;
        char *__serverIP__ = NULL;

        bool __sendFrame__ (byte *buffer, size_t bufferSize, WEBSOCKET_DATA_TYPE dataType) { // returns true if frame has been successfully sent 
          if (bufferSize > 0xFFFF) { // this size fits in large frame size - not supported
              #ifdef __DMESG__
                  dmesgQueue << "[WebSocket] large frame size is not supported";
              #endif
              closeWebSocket ();
              return false;                         
          } 
          // byte *frame = NULL;
          if (bufferSize > HTTP_WS_FRAME_MAX_SIZE - 4) { // 4 bytes are needed for header
              #ifdef __DMESG__
                  dmesgQueue << "[WebSocket] frame size > " << HTTP_WS_FRAME_MAX_SIZE - 4 << "is not supported";
              #endif
              closeWebSocket ();
              return false;                         
          }           
          byte sendFrame [HTTP_WS_FRAME_MAX_SIZE];
          int sendFrameSize;
          if (bufferSize > 125) { // medium frame size
            // if (!(frame = (byte *) malloc (frameSize = 4 + bufferSize))) { // 4 bytes for header (without mask) + payload
            //   dmesg ("[WebSocket] __sendFrame__ malloc error (out of memory)");
            //   closeWebSocket ();
            //   return false;
            // }
            // frame type
            sendFrame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
            sendFrame [1] = 126; // medium frame size, without masking (we won't do the masking, won't set the MASK bit)
            sendFrame [2] = bufferSize >> 8; // / 256;
            sendFrame [3] = bufferSize; // % 256;
            memcpy (sendFrame + 4, buffer, bufferSize);  
            sendFrameSize = bufferSize + 4;
          } else { // small frame size
            // if (!(frame = (byte *) malloc (frameSize = 2 + bufferSize))) { // 2 bytes for header (without mask) + payload
            //   dmesg ("[WebSocket] __sendFrame__ malloc error (out of memory)");
            //   closeWebSocket ();
            //   return false;
            // }
            sendFrame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
            sendFrame [1] = bufferSize; // small frame size, without masking (we won't do the masking, won't set the MASK bit)
            if (bufferSize) memcpy (sendFrame + 2, buffer, bufferSize);  
            sendFrameSize = bufferSize + 2;
          }
          if (sendAll (__connectionSocket__, (char *) sendFrame, sendFrameSize, HTTP_CONNECTION_WS_TIME_OUT) != sendFrameSize) {
            closeWebSocket ();
            return false;
          }
          __lastActive__ = millis (); 
          return true;
        }
    
        enum READ_FRAME_STATE {
          EMPTY = 0,                                  // frame is empty
          READING_SHORT_HEADER = 1,
          READING_MEDIUM_HEADER = 2,
          READING_PAYLOAD = 3,
          FULL = 4                                    // frame is full and waiting to be read by calling program
        }; 
        READ_FRAME_STATE __readFrameState__ = EMPTY;
        byte __readFrame__ [HTTP_WS_FRAME_MAX_SIZE];  // frame consists of a header (6, 8 or 12 bytes (12 only for large frames which are not supported here)) and data payload buffer
        byte *__readBuffer__;                         // pointer to buffer for frame payload - will be directed to __readFrame__ just after the header later
        unsigned int __readBufferSize__;              // size of payload in current frame
        unsigned int __bytesRead__;                   // how many bytes of current buffer have been read so far
        byte *__mask__;                               // pointer to 4 frame mask bytes
    };

    
    // ----- httpConnection class -----

    class httpConnection {

      public:

        // httpConnection state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          RUNNING = 2        
        };

        STATE_TYPE state () { return __state__; }

        httpConnection ( // the following parameters will be handeled by httpConnection instance
                         int connectionSocket,
                         String (*httpRequestHandlerCallback) (char *httpRequest, httpConnection *hcn), // httpRequestHandler callback function provided by calling program
                         void (*wsRequestHandlerCallback) (char *wsRequest, WebSocket *webSocket),      // wsRequestHandler callback function provided by calling program      
                         const char *clientIP, 
                         char *serverIP,
                         const char *httpServerHomeDirectory = (char *) "/var/www/html"
                       )  {
                              // create a local copy of parameters for later use
                              __connectionSocket__ = connectionSocket;
                              __httpRequestHandlerCallback__ = httpRequestHandlerCallback;
                              __wsRequestHandlerCallback__ = wsRequestHandlerCallback;
                              strncpy (__clientIP__, clientIP, sizeof (__clientIP__)); __clientIP__ [sizeof (__clientIP__) - 1] = 0; // copy client's IP since connection may live longer than the server that created it
                              strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0; // copy server's IP since connection may live longer than the server that created it
                              #ifdef __FILE_SYSTEM__
                                  // we have done the checking in httpServer constructor, there is no need to repeat the checking in httpCOnnection constructor agin
                                  // if (!httpServerHomeDirectory || !*httpServerHomeDirectory || strlen (httpServerHomeDirectory) >= sizeof (__httpServerHomeDirectory__) - 2) {
                                  //     dmesg ("[httpConnection] invalid httpServerHomeDirectory");
                                  //     return;
                                  // }
                                  // but we have to keep a local copy of httpServerHomeDirectory since the server can stop before the connection it created gets closed
                                  strcpy (__httpServerHomeDirectory__, httpServerHomeDirectory);
                                  if (__httpServerHomeDirectory__ [strlen (__httpServerHomeDirectory__) - 1] != '/') strcat (__httpServerHomeDirectory__, "/"); // __httpServerHomeDirectory__ always ends with /
                              #endif

                              // handle connection in its own thread (task)
                              // if running out of memory, creation of the new task would fail, so try multiple times before reporting an error
                              #define tskNORMAL_PRIORITY 1
                              for (int i = 0; i < 30; i++) { // try 3 s
                                  #ifdef HTTP_SERVER_CORE
                                      BaseType_t taskCreated = xTaskCreatePinnedToCore (__connectionTask__, "httpConnection", HTTP_CONNECTION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL, HTTP_SERVER_CORE);
                                  #else
                                      BaseType_t taskCreated = xTaskCreate (__connectionTask__, "httpConnection", HTTP_CONNECTION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL);
                                  #endif
                                  if (pdPASS != taskCreated) {
                                      delay (100);
                                  } else {
                                      __state__ = RUNNING;

                                      xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                                          httpServerConcurentTasks++;
                                          if (httpServerConcurentTasks > httpServerConcurentTasksHighWatermark)
                                              httpServerConcurentTasksHighWatermark = httpServerConcurentTasks;
                                      xSemaphoreGive (__httpServerSemaphore__);

                                      return; // success
                                  }
                              }
                              #ifdef __DMESG__
                                  dmesgQueue << "[httpConnection] xTaskCreate error";
                              #endif
                          }

        ~httpConnection ()  {
                              // close connection socket
                              int connectionSocket;
                              xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                                  connectionSocket = __connectionSocket__;
                                  __connectionSocket__ = -1;
                                
                                  httpServerConcurentTasks--;
                              xSemaphoreGive (__httpServerSemaphore__);
                              if (connectionSocket > -1) close (connectionSocket);
                            }

        int getSocket () { return __connectionSocket__; }

        char *getClientIP () { return __clientIP__; }

        char *getServerIP () { return __serverIP__; }

        char *getHttpRequest () { return __httpRequestAndReplyBuffer__; }

        cstring getHttpRequestHeaderField (const char *fieldName) { // HTTP header fields are in format \r\nfieldName: fieldValue\r\n
            char *p = stristr (__httpRequestAndReplyBuffer__, fieldName); 
            if (p && p != __httpRequestAndReplyBuffer__ && *(p - 1) == '\n' && *(p + strlen (fieldName)) == ':') { // p points to fieldName in HTTP request
                p += strlen (fieldName) + 1; while (*p == ' ') p++; // p points to field value in HTTP request
                cstring s; while (*p >= ' ') s += *(p ++);
                return s;
            }
            return "";
        }

        cstring getHttpRequestCookie (const char *cookieName) { // cookies are passed from browser to http server in "cookie" HTTP header field
            char *p = stristr (__httpRequestAndReplyBuffer__, (char *) "\nCookie:"); // find cookie field name in HTTP header          
            if (p) {
                p = strstr (p, cookieName); // find cookie name in HTTP header
                if (p && p != __httpRequestAndReplyBuffer__ && *(p - 1) == ' ' && *(p + strlen (cookieName)) == '=') {
                    p += strlen (cookieName) + 1; while (*p == ' ' || *p == '=' ) p++; // p points to cookie value in HTTP request
                    cstring s; while (*p > ' ' && *p != ';') s += *(p ++);
                    return s;
                }
            }
            return "";
        }

        void setHttpReplyStatus (const char *status) { strncpy (__httpReplyStatus__, status, HTTP_REPLY_STATUS_MAX_LENGTH); __httpReplyStatus__ [HTTP_REPLY_STATUS_MAX_LENGTH - 1] = 0; }
        
        void setHttpReplyHeaderField (cstring fieldName, cstring fieldValue) { 
            __httpReplyHeader__ += fieldName;
            __httpReplyHeader__ +=  + ": ";
            __httpReplyHeader__ += fieldValue;
            __httpReplyHeader__ += "\r\n"; 
        }

        void setHttpReplyCookie (cstring cookieName, cstring cookieValue, time_t expires = 0, cstring path = "/") { 
          char e [50] = "";
          if (expires) {
              if (!time ()) { // time not set
                  #ifdef __DMESG__
                      dmesgQueue << "[httpConnection] could not set cookie expiration time since the current time has not been set yet";
                  #endif
                  return;
              }
              struct tm st = gmTime (expires);
              strftime (e, sizeof (e), "; Expires=%a, %d %b %Y %H:%M:%S GMT", &st);
          }
          // save whole fieldValue into cookieName to save stack space
          cookieName += "=";
          cookieName += cookieValue;
          cookieName += "; Path=";
          cookieName += path;
          cookieName += e;
          setHttpReplyHeaderField ("Set-Cookie", cookieName);
        }

        // combination of clientIP and User-Agent HTTP header field - used for calculation of web session token
        String getClientSpecificInformation () { return String (__clientIP__) + getHttpRequestHeaderField ("User-Agent"); }


        // support for web sessions
        #ifdef WEB_SESSIONS

            // calculates new token from random number and client specific information
            webSessionToken_t newWebSessionToken (char *userName, time_t expires) { 
                // generate new token
                static int tokenCounter = 0;
                webSessionToken_t webSessionToken;
                sha256 (webSessionToken.c_str (), 64 + 1, (char *) (String (tokenCounter ++) + String (esp_random ()) + String (__clientIP__) + getHttpRequestHeaderField ("User-Agent")).c_str ());
                
                // insert the new token into token database together with information associated with it
                signed char e = webSessionTokenDatabase.Insert (webSessionToken, {expires, userName}); // if Insert fails, the token will just not work
                if (e) { // != OK
                    #ifdef __DMESG__
                        dmesgQueue << "[httpConnection] webSessionTokenDatabase.Insert error: " << e; 
                    #endif
                }             
                return webSessionToken;
            }

            // check validity of a token in webSessionTokenDatabase
            Cstring<64> getUserNameFromToken (webSessionToken_t& webSessionToken) {
                // find token in the webSessionTokenDatabase
                webSessionTokenInformation_t webSessionTokenInformation;
                signed char e = webSessionTokenDatabase.FindValue (webSessionToken, &webSessionTokenInformation);
                switch (e) {
                    case err_ok:        if ((time () && webSessionTokenInformation.expires <= time ()) || webSessionTokenInformation.expires == 0)  
                                            return "";
                                        else
                                            return webSessionTokenInformation.userName;
                    case err_not_found: return "";
                    default:                                
                                    #ifdef __DMESG__
                                        dmesgQueue << "[httpConnection] webSessionTokenDatabase.FindValue error: " << e; 
                                    #endif
                                    return "";
                }
            }

            // updates the token in webSessionTokenDatabase
            bool updateWebSessionToken (webSessionToken_t& webSessionToken, Cstring<64>& userName, time_t expires) {
                return (webSessionTokenDatabase.Update (webSessionToken, {expires, userName}) == OK);
            }

            // deletes the token from webSessionTokenDatabase
            bool deleteWebSessionToken (webSessionToken_t& webSessionToken) {
                return (webSessionTokenDatabase.Delete (webSessionToken) == OK);
            }

        #endif

                                                                                                  
      private:

        STATE_TYPE __state__ = NOT_RUNNING;
    
        int __connectionSocket__ = -1;
        String (* __httpRequestHandlerCallback__) (char *httpRequest, httpConnection *hcn) = NULL;
        void (* __wsRequestHandlerCallback__) (char *wsRequest, WebSocket *webSocket) = NULL;        
        char __clientIP__ [46] = "";
        char __serverIP__ [46] = "";

        #ifdef __FILE_SYSTEM__
            char __httpServerHomeDirectory__ [FILE_PATH_MAX_LENGTH] = "/var/www/html";
        #else
            char *__httpServerHomeDirectory__; // not used
        #endif

        Cstring<HTTP_BUFFER_SIZE> __httpRequestAndReplyBuffer__;

        char  __httpReplyStatus__ [HTTP_REPLY_STATUS_MAX_LENGTH] = "200 OK"; // by default
        cstring __httpReplyHeader__ = ""; // by default

        static void __connectionTask__ (void *pvParameters) {
          // get "this" pointer
          httpConnection *ths = (httpConnection *) pvParameters;           
          { // code block
            // READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST 
            
            int receivedTotal = 0;
            bool keepAlive;
            char *endOfHttpRequest = NULL;
            do {
                receivedTotal = recvAll (ths->__connectionSocket__, ths->__httpRequestAndReplyBuffer__.c_str () + receivedTotal, HTTP_BUFFER_SIZE - 1 - receivedTotal, (char *) "\r\n\r\n", HTTP_CONNECTION_TIME_OUT); // BJ remark: is - 1 necessary? recvAll already considers C string ending with 0 itself.
                if (receivedTotal <= 0) {
                    #ifdef __DMESG__
                        if (errno != 11 && errno != 128) dmesgQueue << "[httpConnection] recv error: " << errno << " " << strerror (errno); // don't record time-out, it often happens 
                    #endif
                  goto endOfConnection;
                }

                endOfHttpRequest = strstr (ths->__httpRequestAndReplyBuffer__, "\r\n\r\n"); 
                if (!endOfHttpRequest) {
                    #ifdef __DMESG__
                        dmesgQueue << "[httpConnection] __httpRequestAndReplyBuffer__ too small for HTTP request";
                    #endif
                    sendAll (ths->__connectionSocket__, reply507, HTTP_CONNECTION_TIME_OUT);
                    goto endOfConnection;
                }

                // connection: keep-alive?
                keepAlive = (stristr (ths->__httpRequestAndReplyBuffer__, "CONNECTION: KEEP-ALIVE") != NULL);

                // WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET 
  
                if (stristr (ths->__httpRequestAndReplyBuffer__, (char *) "UPGRADE: WEBSOCKET")) {
                  WebSocket webSocket (ths->__connectionSocket__, ths->__httpRequestAndReplyBuffer__, ths->__clientIP__, ths->__serverIP__); 
                  if (webSocket.state () == WebSocket::RUNNING) {
                    if (ths->__wsRequestHandlerCallback__) ths->__wsRequestHandlerCallback__ (ths->__httpRequestAndReplyBuffer__, &webSocket);
                    else {
                        #ifdef __DMESG__
                            dmesgQueue << "[httpConnection]", " wsRequestHandlerCallback was not provided to handle WebSocket";
                        #endif
                    }
                  }
                  goto endOfConnection;
                }
  
                // SEND HTTP REPLY FROM CALLBACK HANDLER SEND HTTP REPLY FROM CALLBACK HANDLER SEND HTTP REPLY FROM CALLBACK HANDLER 
            
                String httpReplyContent ("");
                if (ths->__httpRequestHandlerCallback__) {
                    httpReplyContent = ths->__httpRequestHandlerCallback__ (ths->__httpRequestAndReplyBuffer__, ths);
                    if (!httpReplyContent) { // out of memory
                        sendAll (ths->__connectionSocket__, reply503, HTTP_CONNECTION_TIME_OUT);
                        goto endOfConnection;
                    }
                }

                if (httpReplyContent != "") {
                  // if Content-type was not provided during __httpRequestHandlerCallback__ try guessing what it is
                  if (!stristr ((char *) ths->__httpReplyHeader__.c_str (), "CONTENT-TYPE")) { 
                      if (stristr ((char *) httpReplyContent.c_str (), "<HTML")) {
                          ths->setHttpReplyHeaderField ("Content-Type", "text/html");
                      } else if (strstr ((char *) httpReplyContent.c_str (), "{")) {
                          ths->setHttpReplyHeaderField ("Content-Type", "application/json");
                      // ... add more if needed
                      } else { 
                          ths->setHttpReplyHeaderField ("Content-Type", "text/plain");
                      }
                  }
                  if (ths->__httpReplyHeader__.errorFlags ()) {
                      #ifdef __DMESG__
                          dmesgQueue << "[httpConnection] __httpReplyHeader__ too small for HTTP reply header";
                      #endif
                      sendAll (ths->__connectionSocket__, reply507, HTTP_CONNECTION_TIME_OUT);
                      goto endOfConnection;
                  }
                  // construct the whole HTTP reply from differrent pieces and send it to client (browser)
                  // if the reply is short enough send it in one block, 
                  // if not send header first and then the content, so we won't have to move hughe blocks of data
                  unsigned long httpReplyContentLen = httpReplyContent.length ();

                  ths->__httpRequestAndReplyBuffer__ = "";
                  ths->__httpRequestAndReplyBuffer__ += "HTTP/1.1 ";
                  ths->__httpRequestAndReplyBuffer__ += ths->__httpReplyStatus__;
                  ths->__httpRequestAndReplyBuffer__ += "\r\n";
                  ths->__httpRequestAndReplyBuffer__ += ths->__httpReplyHeader__;
                  ths->__httpRequestAndReplyBuffer__ += "Content-Length: ";
                  ths->__httpRequestAndReplyBuffer__ += httpReplyContentLen; 
                  ths->__httpRequestAndReplyBuffer__ += "\r\n\r\n";

                  unsigned long httpReplyHeaderLen = ths->__httpRequestAndReplyBuffer__.length ();

                  // can not happen due to the difference in lengths, we can skip checking:
                  // if (ths->__httpRequestAndReplyBuffer__.error ()) {
                  //    dmesg ("[httpConnection] __httpRequestAndReplyBuffer__ too small");
                  //    sendAll (ths->__connectionSocket__, reply507, HTTP_CONNECTION_TIME_OUT);
                  //    goto endOfConnection;
                  // }

                  int bytesToCopyThisTime = min (httpReplyContentLen, (unsigned long) HTTP_BUFFER_SIZE - httpReplyHeaderLen); 
                  strncpy (&ths->__httpRequestAndReplyBuffer__ [httpReplyHeaderLen], (char *) httpReplyContent.c_str (), bytesToCopyThisTime);
                  ths->__httpRequestAndReplyBuffer__ [HTTP_BUFFER_SIZE] = 0;
                  if (sendAll (ths->__connectionSocket__, ths->__httpRequestAndReplyBuffer__, HTTP_CONNECTION_TIME_OUT) <= 0) {
                      #ifdef __DMESG__
                          dmesgQueue << "[httpConnection] send error: " << errno << " " << strerror (errno);
                      #endif
                      goto endOfConnection;
                  }
                  int bytesSent = bytesToCopyThisTime; // already sent
                  while (bytesSent < httpReplyContentLen) {
                    bytesToCopyThisTime = min (httpReplyContentLen - bytesSent, (unsigned long) 1500); // MTU = 1500, TCP_SND_BUF = 5744 (a maximum block size that ESP32 can send)
                    if (sendAll (ths->__connectionSocket__, httpReplyContent.c_str () + bytesSent, bytesToCopyThisTime, HTTP_CONNECTION_TIME_OUT) <= 0) {
                        #ifdef __DMESG__
                            dmesgQueue << "[httpConnection] send error: " << errno << " " << strerror (errno);
                        #endif
                        goto endOfConnection;
                    }                    
                    bytesSent += bytesToCopyThisTime; // already sent
                  }
                  // HTTP reply sent
                  goto nextHttpRequest;
                }

                // SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE 
  
                #ifdef __FILE_SYSTEM__
                  if (fileSystem.mounted ()) {
                    if (strstr (ths->__httpRequestAndReplyBuffer__.c_str (), "GET ") == ths->__httpRequestAndReplyBuffer__.c_str ()) {
                      char *p = strstr (ths->__httpRequestAndReplyBuffer__.c_str () + 4, " ");
                      if (p) {
                        *p = 0;
                        // get file name from HTTP request
                        cstring fileName (ths->__httpRequestAndReplyBuffer__.c_str () + 4);
                        if (fileName == "" || fileName == "/") fileName = "/index.html";
                        fileName = cstring (ths->__httpServerHomeDirectory__) + (fileName.c_str () + 1); // __httpServerHomeDirectory__ always ends with /

                        // if Content-type was not provided during __httpRequestHandlerCallback__ try guessing what it is
                        if (!stristr ((char *) ths->__httpReplyHeader__.c_str (), (char *) "CONTENT-TYPE")) { 
                               if (fileName.endsWith ((char *) ".bmp"))                                         ths->setHttpReplyHeaderField ("Content-Type", "image/bmp");
                          else if (fileName.endsWith ((char *) ".css"))                                         ths->setHttpReplyHeaderField ("Content-Type", "text/css");
                          else if (fileName.endsWith ((char *) ".csv"))                                         ths->setHttpReplyHeaderField ("Content-Type", "text/csv");
                          else if (fileName.endsWith ((char *) ".gif"))                                         ths->setHttpReplyHeaderField ("Content-Type", "image/gif");
                          else if (fileName.endsWith ((char *) ".htm") || fileName.endsWith ((char *) ".html")) ths->setHttpReplyHeaderField ("Content-Type", "text/html");
                          else if (fileName.endsWith ((char *) ".jpg") || fileName.endsWith ((char *) ".jpeg")) ths->setHttpReplyHeaderField ("Content-Type", "image/jpeg");
                          else if (fileName.endsWith ((char *) ".js"))                                          ths->setHttpReplyHeaderField ("Content-Type", "text/javascript");
                          else if (fileName.endsWith ((char *) ".json"))                                        ths->setHttpReplyHeaderField ("Content-Type", "application/json");
                          else if (fileName.endsWith ((char *) ".mpeg"))                                        ths->setHttpReplyHeaderField ("Content-Type", "video/mpeg");
                          else if (fileName.endsWith ((char *) ".pdf"))                                         ths->setHttpReplyHeaderField ("Content-Type", "application/pdf");
                          else if (fileName.endsWith ((char *) ".png"))                                         ths->setHttpReplyHeaderField ("Content-Type", "image/png");
                          else if (fileName.endsWith ((char *) ".tif") || fileName.endsWith ((char *) ".tiff")) ths->setHttpReplyHeaderField ("Content-Type", "image/tiff");
                          else if (fileName.endsWith ((char *) ".txt"))                                         ths->setHttpReplyHeaderField ("Content-Type", "text/plain");
                          // ... add more if needed but Contet-Type can often be omitted without problems ...
                        }

                        // seving files works better if served one at a time
                        if (HTTP_CONNECTION_TIME_OUT == 0) {
                            xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                        } else {
                            if (xSemaphoreTake (__httpServerSemaphore__, pdMS_TO_TICKS (HTTP_CONNECTION_TIME_OUT)) != pdTRUE) {
                                sendAll (ths->__connectionSocket__, reply503, HTTP_CONNECTION_TIME_OUT);
                                goto endOfConnection;
                            }
                        }
                        File f = fileSystem.open (fileName, "r");           
                        if (f) {
                          if (!f.isDirectory ()) {

                            diskTrafficInformation.bytesRead += f.size (); // asume the whole file will be read - update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                            if (ths->__httpReplyHeader__.errorFlags ()) {
                                #ifdef __DMESG__
                                    dmesgQueue << "[httpConnection] __httpReplyHeader__ too small for HTTP reply header";
                                #endif
                                sendAll (ths->__connectionSocket__, reply507, HTTP_CONNECTION_TIME_OUT);
                                xSemaphoreGive (__httpServerSemaphore__);
                                goto endOfConnection;
                            }
                            // construct the whole HTTP reply from differrent pieces and send it to client (browser)
                            // if the reply is short enough send it in one block,   
                            // if not send header first and then the content, so we won't have to move hughe blocks of data
                            unsigned long httpReplyContentLen = f.size ();

                            ths->__httpRequestAndReplyBuffer__ = "";
                            ths->__httpRequestAndReplyBuffer__ += "HTTP/1.1 ";
                            ths->__httpRequestAndReplyBuffer__ += ths->__httpReplyStatus__;
                            ths->__httpRequestAndReplyBuffer__ += "\r\n";
                            ths->__httpRequestAndReplyBuffer__ += ths->__httpReplyHeader__;
                            ths->__httpRequestAndReplyBuffer__ += "Content-Length: ";
                            ths->__httpRequestAndReplyBuffer__ += httpReplyContentLen; 
                            ths->__httpRequestAndReplyBuffer__ += "\r\n\r\n";

                            unsigned long httpReplyHeaderLen = ths->__httpRequestAndReplyBuffer__.length ();

                            // can not happen due to the difference in lengths, we can skip checking:
                            // if (ths->__httpRequestAndReplyBuffer__.error ()) {
                            //    dmesg ("[httpConnection] __httpRequestAndReplyBuffer__ too small");
                            //    sendAll (ths->__connectionSocket__, reply507, HTTP_CONNECTION_TIME_OUT);
                            //    goto endOfConnection;
                            // }

                            int bytesToReadThisTime = min (httpReplyContentLen, (unsigned long) HTTP_BUFFER_SIZE - httpReplyHeaderLen); 
                            int bytesReadThisTime = f.read ((uint8_t *) &ths->__httpRequestAndReplyBuffer__ [httpReplyHeaderLen], (unsigned long) bytesToReadThisTime);
                            // delay (1); // yield ();
                            ths->__httpRequestAndReplyBuffer__ [HTTP_BUFFER_SIZE] = 0;
                            if (!bytesToReadThisTime || sendAll (ths->__connectionSocket__, ths->__httpRequestAndReplyBuffer__, httpReplyHeaderLen + bytesReadThisTime, HTTP_CONNECTION_TIME_OUT) <= 0) {
                                #ifdef __DMESG__
                                    dmesgQueue << "[httpConnection] read-send error (1): " << errno << " " << strerror (errno);
                                #endif
                                f.close ();
                                xSemaphoreGive (__httpServerSemaphore__);
                                goto endOfConnection;
                            }
                            int bytesSent = bytesReadThisTime; // already sent
                            while (bytesSent < httpReplyContentLen) {
                              bytesToReadThisTime = min (httpReplyContentLen - bytesSent, (unsigned long) HTTP_BUFFER_SIZE);
                              bytesReadThisTime = f.read ((uint8_t *) &ths->__httpRequestAndReplyBuffer__ [0], (unsigned long) bytesToReadThisTime);
                              delay (2); // yield (); // WiFi STAtion sometimes disconnects at heavy load - maybe giving it some time would make things better? 
                              if (!bytesToReadThisTime || sendAll (ths->__connectionSocket__, ths->__httpRequestAndReplyBuffer__, bytesReadThisTime, HTTP_CONNECTION_TIME_OUT) <= 0) {
                                  #ifdef __DMESG__
                                      dmesgQueue << "[httpConnection] read-send error (2): " << errno << " " << strerror (errno);
                                  #endif
                                  f.close ();
                                  xSemaphoreGive (__httpServerSemaphore__);
                                  goto endOfConnection;
                              }                    
                              bytesSent += bytesReadThisTime; // already sent
                            }
                            // HTTP reply sent
                            f.close ();
                            xSemaphoreGive (__httpServerSemaphore__);
                            goto nextHttpRequest;
                          } // if file is a file, not a directory
                          f.close ();
                        } // if file is open
                        xSemaphoreGive (__httpServerSemaphore__);
                      }   
                    }          
                  }
                #endif
  
                // SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY

                #ifdef __DMESG__
                    dmesgQueue << "[httpServer] 404: " << (char *) ths->__httpRequestAndReplyBuffer__;
                #endif
  
                if (sendAll (ths->__connectionSocket__, reply404, HTTP_CONNECTION_TIME_OUT) <= 0) {
                    #ifdef __DMESG__
                        dmesgQueue << "[httpConnection] send error: " << errno << " " << strerror (errno);
                    #endif
                    // cout << "[httpServer] send error, closing connection\n";
                    goto endOfConnection;
                }
  
        nextHttpRequest:
              // if we are running out of ESP32's resources we won't try to keep the connection alive, this would slow down the server a bit but it would let still it handle requests from different clients
              if (ths->__connectionSocket__ >= LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN - 2) { // running out of sockets
                  // cout << "[httpServer] running out of sockets, closing connection\n";
                  goto endOfConnection; // running out of sockets
              }
              if (heap_caps_get_largest_free_block (MALLOC_CAP_DEFAULT) < HTTP_CONNECTION_STACK_SIZE) { // there is not a memory block large enough evailable to start a new task that would handle the connection
                  // cout << "[httpServer] running out of memory, closing connection\n";
                  goto endOfConnection; 
              }

              // connection: keep-alive?
              if (keepAlive) {
                  receivedTotal = 0;
                  ths->__httpRequestAndReplyBuffer__ [0] = 0; 
                  // restore default values of member variables for the next HTTP request on the same TCP connection
                  strcpy (ths->__httpReplyStatus__, "200 OK"); 
                  ths->__httpReplyHeader__ = "";                  
              } else {
                  goto endOfConnection;
              }

            } while (keepAlive); // do while keep-alive   
          } // code block

        endOfConnection:  

          // all variables are freed now, unload the instance and stop the task (in this order)
          delete ths;
          vTaskDelete (NULL); // it is connection's responsibility to close itself
        }

    };


    // ----- httpServer class -----

    class httpServer {                                             
    
      public:

        // httpServer state
        enum STATE_TYPE {
          NOT_RUNNING = 0, 
          STARTING = 1, 
          RUNNING = 2        
        };
        
        STATE_TYPE state () { return __state__; }
    
        httpServer ( // the following parameters will be pased to each httpConnection instance
                     String (*httpRequestHandlerCallback) (char *httpRequest, httpConnection *hcn) = NULL,  // httpRequestHandler callback function provided by calling program
                     void (*wsRequestHandlerCallback) (char *wsRequest, WebSocket *webSocket) = NULL,       // wsRequestHandler callback function provided by calling program      
                     // the following parameters will be handeled by httpServer instance
                     const char *serverIP = "0.0.0.0",                                                      // HTTP server IP address, 0.0.0.0 for all available IP addresses
                     int serverPort = 80,                                                                   // HTTP server port
                     bool (*firewallCallback) (char *connectingIP) = NULL,                                  // a reference to callback function that will be celled when new connection arrives 
                     const char *httpServerHomeDirectory = "/var/www/html"
                   )  { 
                        // create directory structure
                        #ifdef __FILE_SYSTEM__
                          if (fileSystem.mounted ()) {
                            if (!fileSystem.isDirectory ((char *) "/var/www/html")) { fileSystem.makeDirectory ((char *) "/var"); fileSystem.makeDirectory ((char *) "/var/www"); fileSystem.makeDirectory ((char *) "/var/www/html"); }
                          }
                        #endif
                        // create a local copy of parameters for later use
                        __httpRequestHandlerCallback__ = httpRequestHandlerCallback;
                        __wsRequestHandlerCallback__ = wsRequestHandlerCallback;
                        strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0;
                        __serverPort__ = serverPort;
                        __firewallCallback__ = firewallCallback;
                        #ifdef __FILE_SYSTEM__
                            if (!httpServerHomeDirectory || !*httpServerHomeDirectory || strlen (httpServerHomeDirectory) >= sizeof (__httpServerHomeDirectory__) - 2) {
                                #ifdef __DMESG__
                                    dmesgQueue << "[httpServer] invalid httpServerHomeDirectory";
                                #endif
                                return;
                            }
                            strcpy (__httpServerHomeDirectory__, httpServerHomeDirectory);
                            if (__httpServerHomeDirectory__ [strlen (__httpServerHomeDirectory__) - 1] != '/') strcat (__httpServerHomeDirectory__, "/"); // __httpServerHomeDirectory__ always ends with /
                        #endif                        

                        // start listener in its own thread (task)
                        __state__ = STARTING;                        
                        #define tskNORMAL_PRIORITY 1
                        #ifdef HTTP_SERVER_CORE
                            BaseType_t taskCreated = xTaskCreatePinnedToCore (__listenerTask__, "httpServer", 2 * 1024, this, tskNORMAL_PRIORITY, NULL, HTTP_SERVER_CORE);
                        #else
                            BaseType_t taskCreated = xTaskCreate (__listenerTask__, "httpServer", 2 * 1024, this, tskNORMAL_PRIORITY, NULL);
                        #endif
                        if (pdPASS != taskCreated) {
                            #ifdef __DMESG__
                                dmesgQueue << "[httpServer] xTaskCreate error";
                            #endif
                        } else {
                          // wait until listener starts accepting connections
                          while (__state__ == STARTING) delay (1); 
                          // when constructor returns __state__ could be eighter RUNNING (in case of success) or NOT_RUNNING (in case of error)
                        }
                      }
        
        ~httpServer ()  {
                          // close listening socket
                          int listeningSocket;
                          xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                            listeningSocket = __listeningSocket__;
                            __listeningSocket__ = -1;
                          xSemaphoreGive (__httpServerSemaphore__);
                          if (listeningSocket > -1) close (listeningSocket);

                          // wait until listener finishes before unloading so that memory variables are still there while the listener is running
                          while (__state__ != NOT_RUNNING) delay (1);
                        }
 
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        String (* __httpRequestHandlerCallback__) (char *httpRequest, httpConnection *hcn) = NULL;
        void (* __wsRequestHandlerCallback__) (char *wsRequest, WebSocket *webSocket) = NULL;
        char __serverIP__ [46] = "0.0.0.0";
        int __serverPort__ = 80;
        bool (* __firewallCallback__) (char *connectingIP) = NULL;

        #ifdef __FILE_SYSTEM__
            char __httpServerHomeDirectory__ [FILE_PATH_MAX_LENGTH] = "/var/www/html";
        #else
            char *__httpServerHomeDirectory__; // not used
        #endif        

        int __listeningSocket__ = -1;

        static void __listenerTask__ (void *pvParameters) {
          {
            // get "this" pointer
            httpServer *ths = (httpServer *) pvParameters;  

            #ifdef WEB_SESSIONS
                // load database and start database cleaning task
                xTaskCreate ([] (void *param) { 
                                                  // load existing tokens
                                                  signed char e = webSessionTokenDatabase.loadData ("/var/www/webSessionTokens.db");
                                                  if (e != OK) {
                                                      #ifdef __DMESG__
                                                          dmesgQueue << "[httpServer] webSessionCleaner: webSessionTokenDatabase.loadData error: " << e << ", truncating webSessionTokenDatabase";
                                                      #endif
                                                      webSessionTokenDatabase.Truncate (); // forget all stored data and try to make it work from the start
                                                  }

                                                  // periodically delete expired tokens
                                                  while (true) {
                                                      webSessionToken_t expiredToken;
                                                      webSessionTokenInformation_t webSessionTokenInformation;
                                                      for (auto p: webSessionTokenDatabase) {
                                                          signed char e = webSessionTokenDatabase.FindValue (p->key, &webSessionTokenInformation, p->blockOffset);
                                                          if (e == OK && time () && webSessionTokenInformation.expires && webSessionTokenInformation.expires <= time ()) {
                                                              expiredToken = p->key;
                                                              break;
                                                          } 
                                                      }
                                                      if (expiredToken > "") {
                                                          webSessionTokenDatabase.Delete (expiredToken);
                                                      }

                                                      delay (6000); // repeat every 6 sec
                                                  }
                                              }, 
                                              "webSessionCleaner", 4 * 1024, NULL, 1, NULL);                
            #endif            
    
            // start listener
            ths->__listeningSocket__ = socket (PF_INET, SOCK_STREAM, 0);
            if (ths->__listeningSocket__ == -1) {
                #ifdef __DMESG__
                    dmesgQueue << "[httpServer] socket error: " << errno << " " << strerror (errno);
                #endif
            } else {
              // make address reusable - so we won't have to wait a few minutes in case server will be restarted
              int flag = 1;
              if (setsockopt (ths->__listeningSocket__, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag)) == -1) {
                  #ifdef __DMESG__
                      dmesgQueue << "[httpServer] setsockopt error: " << errno << " " << strerror (errno);
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
                        dmesgQueue << "[httpServer] bind error: " << errno << " " << strerror (errno);
                    #endif
               } else {
                 // mark socket as listening socket
                 if (listen (ths->__listeningSocket__, 12) == -1) {
                    #ifdef __DMESG__
                        dmesgQueue << "[httpServer] listen error: " << errno << " " << strerror (errno);
                    #endif
                 } else {

                  // remember some information that netstat telnet command would use
                  additionalSocketInformation [ths->__listeningSocket__ - LWIP_SOCKET_OFFSET] = { __LISTENING_SOCKET__, 0, 0, millis (), millis () };
          
                  // listener is ready for accepting connections
                  ths->__state__ = RUNNING;
                  #ifdef __DMESG__
                      dmesgQueue << "[httpServer] listener is running on core " << xPortGetCoreID ();
                      dmesgQueue << "[httpServer] home (root) directory is " << ths->__httpServerHomeDirectory__; 
                  #endif
                  while (ths->__listeningSocket__ > -1) { // while listening socket is opened
          
                      int connectingSocket;
                      struct sockaddr_in connectingAddress;
                      socklen_t connectingAddressSize = sizeof (connectingAddress);
                      // while (heap_caps_get_largest_free_block (MALLOC_CAP_DEFAULT) < HTTP_CONNECTION_STACK_SIZE) delay (10); // there is no memory block large enough evailable to start a new task that would handle the new connection
                      connectingSocket = accept (ths->__listeningSocket__, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
                      if (connectingSocket == -1) {
                        if (ths->__listeningSocket__ > -1) {
                            #ifdef __DMESG__
                                dmesgQueue << "[httpServer] accept error: " << errno << " " << strerror (errno);
                            #endif
                        }
                      } else {

                        // remember some information that netstat telnet command would use
                        additionalSocketInformation [ths->__listeningSocket__ - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
                        additionalSocketInformation [connectingSocket - LWIP_SOCKET_OFFSET] = { __HTTP_SERVER_SOCKET__, 0, 0, millis (), millis () };

                        // get client's IP address
                        char clientIP [46]; inet_ntoa_r (connectingAddress.sin_addr, clientIP, sizeof (clientIP)); 
                        // get server's IP address
                        char serverIP [46]; struct sockaddr_in thisAddress = {}; socklen_t len = sizeof (thisAddress);
                        if (getsockname (connectingSocket, (struct sockaddr *) &thisAddress, &len) != -1) inet_ntoa_r (thisAddress.sin_addr, serverIP, sizeof (serverIP));
                        // port number could also be obtained if needed: ntohs (thisAddress.sin_port);
                        if (ths->__firewallCallback__ && !ths->__firewallCallback__ (clientIP)) {
                            #ifdef __DMESG__
                                dmesgQueue <<"[httpServer] firewall rejected connection from " << clientIP;
                            #endif
                          close (connectingSocket);
                        } else {
                          // make the socket non-blocking so that we can detect time-out
                          if (fcntl (connectingSocket, F_SETFL, O_NONBLOCK) == -1) {
                              #ifdef __DMESG__
                                  dmesgQueue <<"[httpServer] fcntl error: " << errno << " " << strerror (errno);
                              #endif
                              close (connectingSocket);
                          } else {
                                // create httpConnection instence that will handle the connection, then we can lose reference to it - httpConnection will handle the rest
                                httpConnection *hcp = new (std::nothrow) httpConnection (connectingSocket, ths->__httpRequestHandlerCallback__, ths->__wsRequestHandlerCallback__, clientIP, serverIP, ths->__httpServerHomeDirectory__);
                                if (!hcp) {
                                  // dmesg ("[httpServer] new httpConnection error");
                                  sendAll (connectingSocket, reply503, HTTP_CONNECTION_TIME_OUT);
                                  close (connectingSocket); // normally httpConnection would do this but if it is not created we have to do it here
                                } else {
                                  if (hcp->state () != httpConnection::RUNNING) {
                                    sendAll (connectingSocket, reply503, HTTP_CONNECTION_TIME_OUT);
                                    delete (hcp); // normally httpConnection would do this but if it is not running we have to do it here
                                  } else {
                                    ; // httpConnection is running in its own task and will stop by itself 
                                  }
                                }
                                                               
                          } // fcntl
                        } // firewall
                      } // accept

                  } // while accepting connections
                  #ifdef __DMESG__
                      dmesgQueue << "[httpServer] stopped";
                  #endif
          
                 } // listen
               } // bind
              } // setsockopt
              int listeningSocket;
              xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                listeningSocket = ths->__listeningSocket__;
                ths->__listeningSocket__ = -1;
              xSemaphoreGive (__httpServerSemaphore__);
              if (listeningSocket > -1) close (listeningSocket);
            } // socket
            ths->__state__ = NOT_RUNNING;
          }
          // stop the listening thread (task)
          vTaskDelete (NULL); // it is listener's responsibility to close itself          
        }
          
    };
    
#endif
