/*

    httpServer.hpp 
  
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
    HTTP server can serve some HTTP requests itself (for example content of .html and other files) but the calling program
    can also provide its own httpRequestHandlerCallback function. Cookies and page redirection are supported. There is also
    a small "database" to keep valid web session tokens in order to support web login. Text and binary WebSocket straming is
    also supported.
  
    October, 23, 2022, Bojan Jurca

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
      // #include <esp32/sha.h>               // depreciated
      #include <sha/sha_parallel_engine.h>    // needed for websockets support
    #else
      #include <hwcrypto/sha.h>               // needed for websockets support
    #endif
    #include <mbedtls/base64.h>
    #include <mbedtls/md.h>


#ifndef __HTTP_SERVER__
  #define __HTTP_SERVER__

  #ifndef __FILE_SYSTEM__
    #pragma message "Compiling httpServer.h without file system (file_system.h), httpServer will not be able to serve files"
  #endif
  #ifndef __PERFMON__
    #pragma message "Compiling httpServer.h without performance monitors (perfMon.h)"
  #endif

    // ----- TUNNING PARAMETERS -----

    #define HTTP_SERVER_STACK_SIZE 2 * 1024                     // TCP listener
    #define HTTP_CONNECTION_STACK_SIZE 8 * 1024                 // TCP connection
    #define HTTP_REQUEST_BUFFER_SIZE 2 * 1024                   // reading and temporary keeping HTTP requests
    #define HTTP_CONNECTION_TIME_OUT 1500                       // 1500 ms = 1,5 sec for HTTP requests
    #define HTTP_REPLY_STATUS_MAX_LENGTH 32                     // only first 3 characters are important
    #define HTTP_CONNECTION_WS_TIME_OUT 300000                  // 300000 ms = 5 min for WebSocket connections
    #define WEB_SESSION_TIME_OUT 300000                         // 300000 ms = 5 min for web sessions
    #define MAX_WEB_SESSION_TOKENS 5                            // maximum number of simultaneously valid web sessin tokens
    #define WEB_SESSION_TOKENS_ADDITIONAL_INFORMATION_SIZE 128  // maximum number of simultaneously valid web sessin tokens

    #define reply404 (char *) "HTTP/1.0 404 Not found\r\nContent-Length:10\r\n\r\nNot found."
    #define reply503 (char *) "HTTP/1.0 503 Service unavailable\r\nContent-Length:39\r\n\r\nHTTP server is not available right now."


    // ----- CODE -----

    #include "dmesg_functions.h"
    #ifndef __TIME_FUNCTIONS__
      #pragma message "Implicitly including time_functions.h (needed to calculate expiration time of cookies)"
      #include "time_functions.h"
    #endif
    
    #ifndef __STRISTR__
      #define __STRISTR__
      // missing C function in Arduino
      char *stristr (char *haystack, char *needle) { 
        if (!haystack || !needle) return NULL; // nonsense
        int nCheckLimit = strlen (needle);                     
        int hCheckLimit = strlen (haystack) - nCheckLimit + 1;
        for (int i = 0; i < hCheckLimit; i++) {
          int j = i;
          int k = 0;
          while (*(needle + k)) {
              char nChar = *(needle + k ++); if (nChar >= 'a' && nChar <= 'z') nChar -= 32; // convert to upper case
              char hChar = *(haystack + j ++); if (hChar >= 'a' && hChar <= 'z') hChar -= 32; // convert to upper case
              if (nChar != hChar) break;
          }
          if (!*(needle + k)) return haystack + i; // match!
        }
        return NULL; // no match
      }
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
                            if (sendAll (connectionSocket, buffer, strlen (buffer), HTTP_CONNECTION_WS_TIME_OUT) == strlen (buffer)) {
                              __state__ = RUNNING;
                            }
                          } else { // |key| > 24
                            dmesg ("[WebSocket] WsRequest key too long");
                          }
                        } else { // j == NULL
                          dmesg ("[WebSocket] WsRequest without key");
                        }
                      } else { // i == NULL
                        dmesg ("[WebSocket] WsRequest without key");
                      }

                      #ifdef __PERFMON__
                        xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                          __perOpenedfWebSockets__ ++;
                          __perfCurrentWebSockets__ ++;
                        xSemaphoreGive (__httpServerSemaphore__);
                      #endif
                    }
          
        ~WebSocket () { 
                        // send closing frame if possible
                        __sendFrame__ (NULL, 0, WebSocket::CLOSE);
                        closeWebSocket ();
                        if (__payload__) { free (__payload__); __payload__ = NULL; __bufferState__ = EMPTY; }

                        #ifdef __PERFMON__
                          xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                            __perfCurrentWebSockets__ --;
                          xSemaphoreGive (__httpServerSemaphore__);
                        #endif
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

          switch (__bufferState__) {
            case EMPTY:                   {
                                            // prepare data structure for the first read operation
                                            __bytesRead__ = 0;
                                            __bufferState__ = READING_SHORT_HEADER;
                                            // continue reading immediately
                                          }
            case READING_SHORT_HEADER:    { 
                                            // check socket if data is pending to be read
                                            char c; int i = recv (__connectionSocket__, &c, sizeof (c), MSG_PEEK);
                                            if (i == -1) {
                                              if ((errno == EAGAIN || errno == ENAVAIL) && HTTP_CONNECTION_WS_TIME_OUT && millis () - __lastActive__ >= HTTP_CONNECTION_WS_TIME_OUT) return WebSocket::TIME_OUT;
                                              else return WebSocket::NOT_AVAILABLE;
                                            }
                                            // else - something is available to be read

                                            // read 6 bytes of short header
                                            i = recv (__connectionSocket__, __header__ + __bytesRead__, 6 - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            }
                                            #ifdef __PERFMON__ 
                                              __perfWiFiBytesReceived__ += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            #endif                                                                                              
                                            __lastActive__ = millis ();
                                            if (6 != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got 6 bytes continue reading short header the next time available () is called
                                            // check if this frame type is supported
                                            if (!(__header__ [0] & 0b10000000)) { // check fin bit
                                              dmesg ("[WebSocket] frame type not supported");
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            }
                                            byte b  = __header__ [0] & 0b00001111; // check opcode, 1 = text, 2 = binary, 8 = close
                                            if (b == WebSocket::CLOSE) {
                                              // dmesg ("[WebSocket] closed by peer");
                                              closeWebSocket ();
                                              return WebSocket::CLOSE;
                                            }
                                            if (b != WebSocket::STRING && b != WebSocket::BINARY) { 
                                              dmesg ("[WebSocket] only STRING and BINARY frame types are supported");
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            } // NOTE: after this point only TEXT and BINRY frames are processed!
                                            // check payload length that also determines frame type
                                            __payloadLength__ = __header__ [1] & 0b01111111; // byte 1: mask bit is always 1 for packets thet came from browsers, cut it off
                                            if (__payloadLength__ <= 125) { // short payload
                                                  __mask__ = __header__ + 2; // bytes 2, 3, 4, 5
                                                  if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                                    dmesg ("[WebSocket] available malloc error (out of memory)");
                                                    closeWebSocket ();
                                                    __bufferState__ = EMPTY;
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
                                                  closeWebSocket ();
                                                  return WebSocket::ERROR;                         
                                            }
                                          }
            case READING_MEDIUM_HEADER:   { 
                                            // we don't have to repeat the checking already done in short header case, just read additiona 2 bytes and correct data structure
                                            // read additional 2 bytes (8 altogether) bytes of medium header
                                            int i = recv (__connectionSocket__, __header__ + __bytesRead__, 8 - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              return WebSocket::ERROR;
                                            }
                                            #ifdef __PERFMON__ 
                                              __perfWiFiBytesReceived__ += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            #endif                                                                                              
                                            __lastActive__ = millis ();
                                            if (8 != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got 8 bytes continue reading medium header the next time available () is called
                                            // correct internal structure for reading into extended buffer and continue at FILLING_EXTENDED_BUFFER immediately
                                            __payloadLength__ = __header__ [2] << 8 | __header__ [3];
                                            __mask__ = __header__ + 4; // bytes 4, 5, 6, 7
                                            if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                              dmesg ("[WebSocket] available malloc error (out of memory)");
                                              closeWebSocket ();
                                              __bufferState__ = EMPTY;
                                              return WebSocket::ERROR;                                                                                            
                                            }
                                            __bufferState__ = READING_PAYLOAD;
                                            __bytesRead__ = 0; // reset the counter, count only payload from now on
                                            // continue with reading payload immediatelly
                                          }
            case READING_PAYLOAD:         
        readingPayload:                                                    
                                          {
                                            // read all payload bytes
                                            int i = recv (__connectionSocket__, __payload__ + __bytesRead__, __payloadLength__ - __bytesRead__, 0); 
                                            if (i <= 0) {
                                              closeWebSocket ();
                                              if (__payload__) { free (__payload__); __payload__ = NULL; __bufferState__ = EMPTY; }
                                              return WebSocket::ERROR;
                                            }
                                            #ifdef __PERFMON__ 
                                              __perfWiFiBytesReceived__ += i; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                                            #endif                                                  
                                            if (__payloadLength__ != (__bytesRead__ += i)) return WebSocket::NOT_AVAILABLE; // if we haven't got all payload bytes continue reading the next time available () is called
                                            __lastActive__ = millis ();
                                            // all is read, decode (unmask) the data
                                            for (int i = 0; i < __payloadLength__; i++) __payload__ [i] = (__payload__ [i] ^ __mask__ [i % 4]);
                                            // conclude payload with 0 in case this is going to be interpreted as text - like C string
                                            __payload__ [__payloadLength__] = 0;
                                            __bufferState__ = FULL;     // stop reading until buffer is read by the calling program
                                            return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary
                                          }

            case FULL:                    // return immediately, there is no space left to read new incoming data
                                          return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary 
          }
          // for every case that has not been handeled earlier return not available
          return NOT_AVAILABLE;
        }
  
        String readString () { // reads String that arrived from browser (it is a calling program responsibility to check if data type is text)
                               // returns "" in case of communication error
          while (true) {
            switch (available ()) {
              case WebSocket::NOT_AVAILABLE:  delay (1);
                                              break;
              case WebSocket::STRING:         { 
                                                String s ((char *) __payload__); 
                                                free (__payload__); __payload__ = NULL; __bufferState__ = EMPTY;
                                                return s;
                                              }
              default:                        return ""; // WebSocket::BINARY, WebSocket::ERROR, WebSocket::CLOSE, WebSocket::TIME_OUT
            }                                                    
          }
        }
  
        size_t binarySize () { return __bufferState__ == FULL ? __payloadLength__ : 0; } // returns how many bytes has arrived from browser, 0 if data is not ready (yet) to be read
                                                    
        size_t readBinary (byte *buffer, size_t bufferSize) { // returns number bytes copied into buffer
                                                              // returns 0 if there is not enough space in buffer or in case of communication error
          while (true) {
            switch (available ()) {
              case WebSocket::NOT_AVAILABLE:  delay (1);
                                              break;
              case WebSocket::BINARY:         { 
                                                size_t l = binarySize ();
                                                if (bufferSize >= l) 
                                                  memcpy (buffer, __payload__, l);
                                                else
                                                  l = 0;
                                                free (__payload__); __payload__ = NULL; __bufferState__ = EMPTY;
                                                return l; 
                                              }
              default:                        return 0; // WebSocket::STRING WebSocket::ERROR, WebSocket::CLOSE, WebSocket::TIME_OUT
            }                                                    
          }
        }

        bool sendString (char *text) { return __sendFrame__ ((byte *) text, strlen (text), WebSocket::STRING); } // returns success

        bool sendString (const char *text) { return __sendFrame__ ((byte *) text, strlen (text), WebSocket::STRING); } // returns success
                                                  
        bool sendString (String text) { return __sendFrame__ ((byte *) text.c_str (), text.length (), WebSocket::STRING); } // returns success
  
        bool sendBinary (byte *buffer, size_t bufferSize) { return __sendFrame__ (buffer, bufferSize, WebSocket::BINARY); } // returns success
        
      private:

        STATE_TYPE __state__ = NOT_RUNNING;

        unsigned long __lastActive__ = millis (); // time-out detection

        int __connectionSocket__ = -1;
        char *__wsRequest__ = NULL;
        char *__clientIP__ = NULL;
        char *__serverIP__ = NULL;

        bool __sendFrame__ (byte *buffer, size_t bufferSize, WEBSOCKET_DATA_TYPE dataType) { // returns true if frame have been sent successfully
          if (bufferSize > 0xFFFF) { // this size fits in large frame size - not supported
            dmesg ("[WebSocket] large frame size is not supported");
            closeWebSocket ();
            return false;                         
          } 
          byte *frame = NULL;
          int frameSize;
          if (bufferSize > 125) { // medium frame size
            if (!(frame = (byte *) malloc (frameSize = 4 + bufferSize))) { // 4 bytes for header (without mask) + payload
              dmesg ("[WebSocket] __sendFrame__ malloc error (out of memory)");
              closeWebSocket ();
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
              dmesg ("[WebSocket] __sendFrame__ malloc error (out of memory)");
              closeWebSocket ();
              return false;
            }
            frame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
            frame [1] = bufferSize; // small frame size, without masking (we won't do the masking, won't set the MASK bit)
            if (bufferSize) memcpy (frame + 2, buffer, bufferSize);  
          }
          if (sendAll (__connectionSocket__, (char *) frame, frameSize, HTTP_CONNECTION_WS_TIME_OUT) != frameSize) {
            free (frame);
            closeWebSocket ();
            return false;
          }
          __lastActive__ = millis ();
          free (frame);
          return true;
        }
    
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
                         void (*wsRequestHandler) (char *wsRequest, WebSocket *webSocket),              // httpRequestHandler callback function provided by calling program      
                         char *clientIP, char *serverIP
                       )  {
                            // create a local copy of parameters for later use
                            __connectionSocket__ = connectionSocket;
                            __httpRequestHandlerCallback__ = httpRequestHandlerCallback;
                            __wsRequestHandler__ = wsRequestHandler;
                            strncpy (__clientIP__, clientIP, sizeof (__clientIP__)); __clientIP__ [sizeof (__clientIP__) - 1] = 0; // copy client's IP since connection may live longer than the server that created it
                            strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0; // copy server's IP since connection may live longer than the server that created it
                            // handle connection in its own thread (task)       
                            #define tskNORMAL_PRIORITY 1
                            if (pdPASS != xTaskCreate (__connectionTask__, "httpConnection", HTTP_CONNECTION_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL)) {
                              dmesg ("[httpConnection] xTaskCreate error");
                            } else {
                              __state__ = RUNNING;                            
                            }

                            #ifdef __PERFMON__
                              xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                                __perfOpenedHttpConnections__ ++;
                                __perfCurrentHttpConnections__ ++;
                              xSemaphoreGive (__httpServerSemaphore__);
                            #endif
                          }

        ~httpConnection ()  {
                              // close connection socket
                              int connectionSocket;
                              xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
                                connectionSocket = __connectionSocket__;
                                __connectionSocket__ = -1;
                                #ifdef __PERFMON__
                                  __perfCurrentHttpConnections__ --;
                                #endif
                              xSemaphoreGive (__httpServerSemaphore__);
                              if (connectionSocket > -1) close (connectionSocket);
                            }

        int getSocket () { return __connectionSocket__; }

        char *getClientIP () { return __clientIP__; }

        char *getServerIP () { return __serverIP__; }

        char *getHttpRequest () { return __httpRequest__; }

        String getHttpRequestHeaderField (char *fieldName) { // HTTP header fields are in format \r\nfieldName: fieldValue\r\n
          char *p = stristr (__httpRequest__, fieldName); 
          if (p && p != __httpRequest__ && *(p - 1) == '\n' && *(p + strlen (fieldName)) == ':') { // p points to fieldName in HTTP request
            p += strlen (fieldName) + 1; while (*p == ' ') p++; // p points to field value in HTTP request
            String s (""); while (*p > ' ') s += *(p ++);
            return s;
          }
          return "";
        }

        String getHttpRequestCookie (char *cookieName) { // cookies are passed from browser to http server in "cookie" HTTP header field
          char *p = stristr (__httpRequest__, (char *) "\nCookie:"); // find cookie field name in HTTP header          
          if (p) {
            p = strstr (p, cookieName); // find cookie name in HTTP header
            if (p && p != __httpRequest__ && *(p - 1) == ' ' && *(p + strlen (cookieName)) == '=') {
              p += strlen (cookieName) + 1; while (*p == ' ' || *p == '=' ) p++; // p points to cookie value in HTTP request
              String s (""); while (*p > ' ' && *p != ';') s += *(p ++);
              return s;
            }
          }
          return "";
        }

        void setHttpReplyStatus (char *status) { strncpy (__httpReplyStatus__, status, HTTP_REPLY_STATUS_MAX_LENGTH); __httpReplyStatus__ [HTTP_REPLY_STATUS_MAX_LENGTH - 1] = 0; }
        
        void setHttpReplyHeaderField (String fieldName, String fieldValue) { __httpReplyHeader__ += fieldName + ": " + fieldValue + "\r\n"; }

        void setHttpReplyCookie (String cookieName, String cookieValue, time_t expires = 0, String path = "/") { 
          char e [50] = "";
          if (expires) {
            struct tm st = timeToStructTime (expires);
            strftime (e, sizeof (e), "; Expires=%a, %d %b %Y %H:%M:%S GMT", &st);
          }
          setHttpReplyHeaderField ("Set-Cookie", cookieName + "=" + cookieValue + "; Path=" + path + String (e));
        }

        // combination of clientIP and User-Agent HTTP header field - used for calculation of web session token
        String getClientSpecificInformation () { return String (__clientIP__) + getHttpRequestHeaderField ((char *) "User-Agent"); }
                                                                                                                
      private:

        STATE_TYPE __state__ = NOT_RUNNING;
    
        int __connectionSocket__ = -1;
        String (* __httpRequestHandlerCallback__) (char *httpRequest, httpConnection *hcn) = NULL;
        void (* __wsRequestHandler__) (char *wsRequest, WebSocket *webSocket) = NULL;        
        char __clientIP__ [46] = "";
        char __serverIP__ [46] = "";

        char __httpRequest__ [HTTP_REQUEST_BUFFER_SIZE];

        char  __httpReplyStatus__ [HTTP_REPLY_STATUS_MAX_LENGTH] = "200 OK"; // by default
        String __httpReplyHeader__ = ""; // by default

        static void __connectionTask__ (void *pvParameters) {
          // get "this" pointer
          httpConnection *ths = (httpConnection *) pvParameters;           
          { // code block
                        
            // READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST READ HTTP REQUEST 
            
            int receivedTotal = 0;
            bool keepAlive = false;
            char *endOfHttpRequest = NULL;
            do {
              receivedTotal = recvAll (ths->__connectionSocket__, ths->__httpRequest__ + receivedTotal, HTTP_REQUEST_BUFFER_SIZE - 1 - receivedTotal, (char *) "\r\n\r\n", HTTP_CONNECTION_TIME_OUT);
              if (receivedTotal <= 0) {
                if (errno != 11) dmesg ("[httpConnection] recv error: ", errno, strerror (errno)); // don't record time-out, it often happens 
                goto endOfConnection;
              }

              #ifdef __PERFMON__
                __perfHttpRequests__ ++; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              #endif

                // WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET WEBSOCKET 
  
                if (stristr (ths->__httpRequest__, (char *) "UPGRADE: WEBSOCKET")) {
                  WebSocket webSocket (ths->__connectionSocket__, ths->__httpRequest__, ths->__clientIP__, ths->__serverIP__); 
                  if (webSocket.state () == WebSocket::RUNNING) {
                    if (ths->__wsRequestHandler__) ths->__wsRequestHandler__ (ths->__httpRequest__, &webSocket);
                    else dmesg ("[httpConnection]", " wsRequestHandler was not provided to handle WebSocket");
                  }
                  goto endOfConnection;
                }
  
                // SEND HTTP REPLY FROM CALLBACK HANDLER SEND HTTP REPLY FROM CALLBACK HANDLER SEND HTTP REPLY FROM CALLBACK HANDLER 
            
                String httpReplyContent ("");
                if (ths->__httpRequestHandlerCallback__) httpReplyContent = ths->__httpRequestHandlerCallback__ (ths->__httpRequest__, ths);
                if (httpReplyContent != "") {
                  // if Content-type was not provided during __httpRequestHandlerCallback__ try guessing what it is
                  if (!stristr ((char *) ths->__httpReplyHeader__.c_str (), (char *) "CONTENT-TYPE")) { 
                    if (stristr ((char *) httpReplyContent.c_str (), (char *) "<HTML>")) ths->setHttpReplyHeaderField ("Content-Type", "text/html");
                    else if (strstr ((char *) httpReplyContent.c_str (), "{"))           ths->setHttpReplyHeaderField ("Content-Type", "application/json");
                    // ... add more if needed
                    else                                                                 ths->setHttpReplyHeaderField ("Content-Type", "text/plain");
                  }
                  // construct the whole HTTP reply from differrent pieces and send it to client (browser)
                  // if the reply is short enough send it in one block, 
                  // if not send header first and then the content, so we won't have to move hughe blocks of data
                  String httpReplyHeader = "HTTP/1.1 " + String (ths->__httpReplyStatus__) + "\r\n" + ths->__httpReplyHeader__ + "Content-Length: " + httpReplyContent.length () + "\r\n\r\n";
                  size_t httpReplyHeaderLen = httpReplyHeader.length ();
                  size_t httpReplyContentLen = httpReplyContent.length ();
                  if (httpReplyHeaderLen + httpReplyContentLen <= TCP_SND_BUF) {
                    if (sendAll (ths->__connectionSocket__, (char *) (httpReplyHeader + httpReplyContent).c_str (), httpReplyHeaderLen + httpReplyContentLen, HTTP_CONNECTION_TIME_OUT) <= 0) {
                      dmesg ("[httpConnection] send error: ", errno, strerror (errno));
                      goto endOfConnection;
                    }
                    // HTTP reply sent
                    goto nextHttpRequest;
                  } else {
                    if (sendAll (ths->__connectionSocket__, (char *) httpReplyHeader.c_str (), httpReplyHeaderLen, HTTP_CONNECTION_TIME_OUT) <= 0) {
                      dmesg ("[httpConnection] send error: ", errno, strerror (errno));
                      goto endOfConnection;
                    }
                    if (sendAll (ths->__connectionSocket__, (char *) httpReplyContent.c_str (), httpReplyContentLen, HTTP_CONNECTION_TIME_OUT) <= 0) {
                      dmesg ("[httpConnection] send error: ", errno, strerror (errno));
                      goto endOfConnection;
                    }
                    // HTTP reply sent
                    goto nextHttpRequest;
                  }
                  // DEBUG: Serial.printf ("[httpConnection] reply length: %i\n", httpReplyHeaderLen + httpReplyContentLen);
                }
  
                // SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE SEND HTTP REPLY FROM FILE 
  
                #ifdef __FILE_SYSTEM__
                  if (__fileSystemMounted__) {
                    if (strstr (ths->__httpRequest__, "GET ") == ths->__httpRequest__) {
                      char *p = strstr (ths->__httpRequest__ + 4, " ");
                      if (p) {
                        *p = 0;
                        // get file name from HTTP request
                        String fileName (ths->__httpRequest__ + 4); if (fileName == "" || fileName == "/") fileName = "/index.html";
                        fileName = "/var/www/html" + fileName;
                        // if Content-type was not provided during __httpRequestHandlerCallback__ try guessing what it is
                        if (!stristr ((char *) ths->__httpReplyHeader__.c_str (), (char *) "CONTENT-TYPE")) { 
                               if (fileName.endsWith (".bmp"))                                ths->setHttpReplyHeaderField ("Content-Type", "image/bmp");
                          else if (fileName.endsWith (".css"))                                ths->setHttpReplyHeaderField ("Content-Type", "text/css");
                          else if (fileName.endsWith (".csv"))                                ths->setHttpReplyHeaderField ("Content-Type", "text/csv");
                          else if (fileName.endsWith (".gif"))                                ths->setHttpReplyHeaderField ("Content-Type", "image/gif");
                          else if (fileName.endsWith (".htm") || fileName.endsWith (".html")) ths->setHttpReplyHeaderField ("Content-Type", "text/html");
                          else if (fileName.endsWith (".jpg") || fileName.endsWith (".jpeg")) ths->setHttpReplyHeaderField ("Content-Type", "image/jpeg");
                          else if (fileName.endsWith (".js"))                                 ths->setHttpReplyHeaderField ("Content-Type", "text/javascript");
                          else if (fileName.endsWith (".json"))                               ths->setHttpReplyHeaderField ("Content-Type", "application/json");
                          else if (fileName.endsWith (".mpeg"))                               ths->setHttpReplyHeaderField ("Content-Type", "video/mpeg");
                          else if (fileName.endsWith (".pdf"))                                ths->setHttpReplyHeaderField ("Content-Type", "application/pdf");
                          else if (fileName.endsWith (".png"))                                ths->setHttpReplyHeaderField ("Content-Type", "image/png");
                          else if (fileName.endsWith (".tif") || fileName.endsWith (".tiff")) ths->setHttpReplyHeaderField ("Content-Type", "image/tiff");
                          else if (fileName.endsWith (".txt"))                                ths->setHttpReplyHeaderField ("Content-Type", "text/plain");
                          // ... add more if needed but Contet-Type can often be omitted without problems ...
                        }
                  
                        File f = fileSystem.open (fileName.c_str (), FILE_READ);           
                        if (f) {
                          if (!f.isDirectory ()) {
                            #ifdef __PERFMON__
                              __perfFSBytesRead__ += f.size (); // asume the whole file will be read - update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
                            #endif
                            String httpReplyHeader = "HTTP/1.1 " + String (ths->__httpReplyStatus__) + "\r\n" + ths->__httpReplyHeader__ + "Content-Length: " + String (f.size ()) + "\r\n\r\n";
                            size_t httpReplyHeaderLen = httpReplyHeader.length ();
                            size_t outputBufferSize = httpReplyHeaderLen + f.size ();
                            if (outputBufferSize > TCP_SND_BUF) outputBufferSize = TCP_SND_BUF;
                            if (outputBufferSize < httpReplyHeaderLen) outputBufferSize = httpReplyHeaderLen;
                            char *outputBuffer = (char *) malloc (outputBufferSize);
                            if (!outputBuffer) {
                              dmesg ("[httpConnection] malloc error (out of memory)");
                              f.close ();
                              if (sendAll (ths->__connectionSocket__, reply503, strlen (reply503), HTTP_CONNECTION_TIME_OUT) <= 0) {
                                dmesg ("[httpConnection] send error: ", errno, strerror (errno));
                                goto endOfConnection;
                              }
                              goto nextHttpRequest;
                            } else {
                              strcpy (outputBuffer, (char *) httpReplyHeader.c_str ());
                              int i = strlen (outputBuffer);
                              /*
                              while (f.available ()) {
                                *(outputBuffer + i++) = f.read ();
                                if (i == outputBufferSize) { 
                                  if (sendAll (ths->__connectionSocket__, outputBuffer, outputBufferSize, HTTP_CONNECTION_TIME_OUT) <= 0) {
                                    dmesg ("[httpConnection] send error: ", errno);
                                    free (outputBuffer);
                                    f.close ();
                                    goto endOfConnection;
                                  }
                                  i = 0; 
                                }
                              }
                              if (i) { 
                                if (sendAll (ths->__connectionSocket__, outputBuffer, i, HTTP_CONNECTION_TIME_OUT) <= 0) {
                                  dmesg ("[httpConnection] send error: ", errno);
                                  free (outputBuffer);
                                  f.close ();
                                  goto endOfConnection;
                                }
                              }
                              */
                              int bytesReadThisTime = 0;
                              do {
                                int bytesSentThisTime = 0;
                                bytesReadThisTime = f.read ((uint8_t *) outputBuffer + i, outputBufferSize - i);
                                if (i || bytesReadThisTime) bytesSentThisTime = sendAll (ths->__connectionSocket__, outputBuffer, i + bytesReadThisTime, HTTP_CONNECTION_TIME_OUT);
                                if (bytesSentThisTime < i + bytesReadThisTime) { 
                                  dmesg ("[httpConnection] send error: ", errno, strerror (errno)); 
                                  free (outputBuffer);
                                  f.close ();
                                  goto endOfConnection;
                                } // error
                                i = 0;
                              } while (bytesReadThisTime);
                              
                              // the whole file successfully sent or partly sent with error
                              free (outputBuffer);
                              f.close ();
                              goto nextHttpRequest;
                            } // malloc
                          } // if file is a file, not a directory
                          f.close ();
                        } // if file is open
                      }   
                    }          
                  }
                #endif
  
                // SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY SEND 404 HTTP REPLY
  
                if (sendAll (ths->__connectionSocket__, reply404, strlen (reply404), HTTP_CONNECTION_TIME_OUT) <= 0) {
                  dmesg ("[httpConnection] send error: ", errno, strerror (errno));
                  goto endOfConnection;
                }
  
        nextHttpRequest:
              endOfHttpRequest = strstr (ths->__httpRequest__, "\r\n\r\n");
              char *p = stristr (ths->__httpRequest__, (char *) "CONNECTION: KEEP-ALIVE");
              if (p && p < endOfHttpRequest) {
                keepAlive = true;
                strcpy (ths->__httpRequest__, endOfHttpRequest + 4);
                receivedTotal = strlen (ths->__httpRequest__);
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
                     String (*httpRequestHandlerCallback) (char *httpRequest, httpConnection *hcn), // httpRequestHandler callback function provided by calling program
                     void (*wsRequestHandler) (char *wsRequest, WebSocket *webSocket),              // httpRequestHandler callback function provided by calling program      
                     // the following parameters will be handeled by httpServer instance
                     char *serverIP,                                                                // HTTP server IP address, 0.0.0.0 for all available IP addresses
                     int serverPort,                                                                // HTTP server port
                     bool (*firewallCallback) (char *connectingIP)                                  // a reference to callback function that will be celled when new connection arrives 
                   )  { 
                        // create directory structure
                        #ifdef __FILE_SYSTEM__
                          if (__fileSystemMounted__) {
                            if (!isDirectory ("/var/www/html")) { fileSystem.mkdir ("/var"); fileSystem.mkdir ("/var/www"); fileSystem.mkdir ("/var/www/html"); }
                          }
                        #endif
                        // create a local copy of parameters for later use
                        __httpRequestHandlerCallback__ = httpRequestHandlerCallback;
                        __wsRequestHandler__ = wsRequestHandler;
                        strncpy (__serverIP__, serverIP, sizeof (__serverIP__)); __serverIP__ [sizeof (__serverIP__) - 1] = 0;
                        __serverPort__ = serverPort;
                        __firewallCallback__ = firewallCallback;

                        // start listener in its own thread (task)
                        __state__ = STARTING;                        
                        #define tskNORMAL_PRIORITY 1
                        if (pdPASS != xTaskCreate (__listenerTask__, "httpServer", HTTP_SERVER_STACK_SIZE, this, tskNORMAL_PRIORITY, NULL)) {
                          dmesg ("[httpServer] xTaskCreate error");
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
        void (* __wsRequestHandler__) (char *wsRequest, WebSocket *webSocket) = NULL;
        char __serverIP__ [46] = "0.0.0.0";
        int __serverPort__ = 80;
        bool (* __firewallCallback__) (char *connectingIP) = NULL;

        int __listeningSocket__ = -1;

        static void __listenerTask__ (void *pvParameters) {
          {
            // get "this" pointer
            httpServer *ths = (httpServer *) pvParameters;  
    
            // start listener
            ths->__listeningSocket__ = socket (PF_INET, SOCK_STREAM, 0);
            if (ths->__listeningSocket__ == -1) {
              dmesg ("[httpServer] socket error: ", errno, strerror (errno));
            } else {
              // make address reusable - so we won't have to wait a few minutes in case server will be restarted
              int flag = 1;
              if (setsockopt (ths->__listeningSocket__, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag)) == -1) {
                dmesg ("[httpServer] setsockopt error: ", errno, strerror (errno));
              } else {
                // bind listening socket to IP address and port     
                struct sockaddr_in serverAddress; 
                memset (&serverAddress, 0, sizeof (struct sockaddr_in));
                serverAddress.sin_family = AF_INET;
                serverAddress.sin_addr.s_addr = inet_addr (ths->__serverIP__);
                serverAddress.sin_port = htons (ths->__serverPort__);
                if (bind (ths->__listeningSocket__, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
                  dmesg ("[httpServer] bind error: ", errno, strerror (errno));
               } else {
                 // mark socket as listening socket
                 #define BACKLOG 5
                 if (listen (ths->__listeningSocket__, TCP_LISTEN_BACKLOG) == -1) {
                  dmesg ("[httpServer] listen error: ", errno, strerror (errno));
                 } else {
          
                  // listener is ready for accepting connections
                  ths->__state__ = RUNNING;
                  dmesg ("[httpServer] started");
                  while (ths->__listeningSocket__ > -1) { // while listening socket is opened
          
                      int connectingSocket;
                      struct sockaddr_in connectingAddress;
                      socklen_t connectingAddressSize = sizeof (connectingAddress);
                      connectingSocket = accept (ths->__listeningSocket__, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
                      if (connectingSocket == -1) {
                        if (ths->__listeningSocket__ > -1) dmesg ("[httpServer] accept error: ", errno, strerror (errno));
                      } else {
                        // get client's IP address
                        char clientIP [46]; inet_ntoa_r (connectingAddress.sin_addr, clientIP, sizeof (clientIP)); 
                        // get server's IP address
                        char serverIP [46]; struct sockaddr_in thisAddress = {}; socklen_t len = sizeof (thisAddress);
                        if (getsockname (connectingSocket, (struct sockaddr *) &thisAddress, &len) != -1) inet_ntoa_r (thisAddress.sin_addr, serverIP, sizeof (serverIP));
                        // port number could also be obtained if needed: ntohs (thisAddress.sin_port);
                        if (ths->__firewallCallback__ && !ths->__firewallCallback__ (clientIP)) {
                          dmesg ("[httpServer] firewall rejected connection from ", clientIP);
                          close (connectingSocket);
                        } else {
                          // make the socket non-blocking so that we can detect time-out
                          if (fcntl (connectingSocket, F_SETFL, O_NONBLOCK) == -1) {
                            dmesg ("[httpServer] fcntl error: ", errno, strerror (errno));
                            close (connectingSocket);
                          } else {
                                // create httpConnection instence that will handle the connection, then we can lose reference to it - httpConnection will handle the rest
                                httpConnection *hcp = new httpConnection (connectingSocket, 
                                                                          ths->__httpRequestHandlerCallback__, 
                                                                          ths->__wsRequestHandler__,
                                                                          clientIP, serverIP);
                                if (!hcp) {
                                  // dmesg ("[httpServer] new httpConnection error");
                                  sendAll (connectingSocket, reply503, strlen (reply503), HTTP_CONNECTION_TIME_OUT);
                                  close (connectingSocket); // normally httpConnection would do this but if it is not created we have to do it here
                                } else {
                                  if (hcp->state () != httpConnection::RUNNING) {
                                    sendAll (connectingSocket, reply503, strlen (reply503), HTTP_CONNECTION_TIME_OUT);
                                    delete (hcp); // normally httpConnection would do this but if it is not running we have to do it here
                                  }
                                }
                                                               
                          } // fcntl
                        } // firewall
                      } // accept
                      
                  } // while accepting connections
                  dmesg ("[httpServer] stopped");
          
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


    /*  
       Internal "database" od valid web session tokens  
    */

    class webSessionToken {  
      public:

        char *generateToken (httpConnection *hcn) { // calculates new token from random number and client specific information
          sha256 (__token__, sizeof (__token__), (char *) (String (__randomNumber__) + hcn->getClientSpecificInformation ()).c_str ());
          __lastActive__ = millis ();
          return getToken ();
        }

        bool isTokenValid (httpConnection *hcn) { // checks token validity against client specific information and time-out
          if (millis () - __lastActive__ >= WEB_SESSION_TIME_OUT) return false; // web session token time-out
          if (!*__token__) return false; // web  session token not set
          char token [65];
          sha256 (token, sizeof (token), (char *) (String (__randomNumber__) + hcn->getClientSpecificInformation ()).c_str ());
          if (strcmp (__token__, token)) return false;
          __lastActive__ = millis ();
          return true;
        }

        char *getToken () {
          if (millis () - __lastActive__ >= WEB_SESSION_TIME_OUT) return NULL; // web session token time-out
          if (!*__token__) return NULL; // web  session token not set
          return __token__; // valid web session token
        }

        void deleteToken () { *__token__ = 0; }        

        void setAdditionalInformation (char *additionalInformation) { 
          __lastActive__ = millis ();
          strncpy (__additionalInformation__, additionalInformation, sizeof (__additionalInformation__)); __additionalInformation__ [sizeof (__additionalInformation__) - 1] = 0;
        };

        char *getAdditionalInformation () { 
          __lastActive__ = millis ();
          return __additionalInformation__; 
        };

      private:

        unsigned long __lastActive__;
        char __token__ [65] = {};
        long __randomNumber__ = random (2147483647);

        // any additional information connected to token like user name, ...
        char __additionalInformation__ [WEB_SESSION_TOKENS_ADDITIONAL_INFORMATION_SIZE] = {};
    };  
    
    webSessionToken __webSessionTokens__ [MAX_WEB_SESSION_TOKENS];

    String generateWebSessionToken (httpConnection *hcn) {
      for (int i = 0; i < MAX_WEB_SESSION_TOKENS; i++) { // find free slot
        if (!__webSessionTokens__ [i].getToken ()) {
          return __webSessionTokens__ [i].generateToken (hcn); // success
        }
      }
      return ""; // failure
    }

    bool isWebSessionTokenValid  (String token, httpConnection *hcn) {
      for (int i = 0; i < MAX_WEB_SESSION_TOKENS; i++) { // find token
        if (__webSessionTokens__ [i].getToken () && !strcmp (__webSessionTokens__ [i].getToken (), (char *) token.c_str ())) {
          return __webSessionTokens__ [i].isTokenValid (hcn); // check validity
        }
      }
      return false; // failure
    }

    bool deleteWebSessionToken  (String token) {
      for (int i = 0; i < MAX_WEB_SESSION_TOKENS; i++) { // find token
        if (__webSessionTokens__ [i].getToken () && !strcmp (__webSessionTokens__ [i].getToken (), (char *) token.c_str ())) {
          __webSessionTokens__ [i].deleteToken (); 
          return true; // success
        }
      }
      return false; // failure
    }

    // any additional information connected to token like user name, ...
    bool setWebSessionTokenAdditionalInformation  (String token, String additionalInformation) {
      for (int i = 0; i < MAX_WEB_SESSION_TOKENS; i++) { // find token
        if (__webSessionTokens__ [i].getToken () && !strcmp (__webSessionTokens__ [i].getToken (), (char *) token.c_str ())) {
          __webSessionTokens__ [i].setAdditionalInformation ((char *) additionalInformation.c_str ());
          return true;
        }
      }
      return false; // failure
    }

    // any additional information connected to token like user name, ...
    String getWebSessionTokenAdditionalInformation  (String token) {
      for (int i = 0; i < MAX_WEB_SESSION_TOKENS; i++) { // find token
        if (__webSessionTokens__ [i].getToken () && !strcmp (__webSessionTokens__ [i].getToken (), (char *) token.c_str ())) {
          return __webSessionTokens__ [i].getAdditionalInformation (); 
        }
      }
      return ""; // failure
    }
    
#endif
