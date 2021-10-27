/*

    TcpServer.hpp

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    TcpServer.hpp contains a minimalistic IPv4 threaded TCP server for ESP32 / Arduino environment with:
      - time-out functionality,
      - firewall functionality.

    Five types of objects are introduced in order to make programming interface as simple as possible
    (a programmer does not have to deal with sockets and threads by himself):
    
      - threaded TcpConnection (with time-out functionality while handling a connection),
      - not-threaded TcpConnection (with time-out functionality while handling a connection),
      - threaded TcpServer (with firewall functionality),
      - not-threaded TcpServer (with time-out functionality while waiting for a connection, firewall functionality),
      - TcpClient (with time-out functionality while handling the connection).

    September, 13, 2021, Bojan Jurca

*/


#ifndef __TCP_SERVER__
  #define __TCP_SERVER__

  #include <WiFi.h>
  #include <lwip/sockets.h>
  #include "common_functions.h" // inet_ntos

  
  /*

     Support for telnet dmesg command. If telnetServer.hpp is included in the project __TcpDmesg__ function will be redirected
     to message queue defined there and dmesg command will display its contetn. Otherwise it will just display message on the
     Serial console.
     
  */


  void __TcpServerDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__  // use dmesg from telnet server if possible
      dmesg (message);
    #else                     // if not just output to Serial
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ()); 
    #endif
  }
  void (* TcpServerDmesg) (String) = __TcpServerDmesg__; // use this pointer to display / record system messages

  #ifndef dmesg
    #define dmesg TcpServerDmesg
  #endif
  

  // control TcpServer critical sections
  static SemaphoreHandle_t __TcpServerSemaphore__ = xSemaphoreCreateMutex (); 
  

  /*
      There are two types of TcpConnections:
       
        Threaded TcpConnections run in their own threads and communicate with calling program through connection handler callback function.
        This type of TcpConnections is normally used by TcpServers.
      
        Not-threaded TcpConnections run in threads of calling programs. Calling program can call instance member functions. 
        This type of TcpConnections is normally used by TcpClients.

  */


  // define time-out data type
  enum TIME_OUT_TYPE: unsigned long {
    INFINITE = 0  // infinite time-out
                  // everithing else is time-out in milli seconds
  };


  class TcpConnection {
  
    public:

    
      /*
    
         TcpConnection constructor that creates threaded TcpConnection. Threaded TcpConnections run in their own threads and communicate with 
         calling program through connection handler callback function. Success can be tested with started () function after constructor returns.
     
      */

      
      TcpConnection (void (* connectionHandlerCallback) (TcpConnection *connection, void *parameter), // a reference to callback function that will handle the connection
                     void *connectionHandlerCallbackParamater,                                        // a reference to parameter that will be passed to connectionHandlerCallback
                     size_t stackSize,                                                                // stack size of a thread where connection runs - this value depends on what server really does (see connectionHandler function) should be set appropriately
                     int socket,                                                                      // connection socket
                     String otherSideIP,                                                              // IP address of the other side of connection
                     TIME_OUT_TYPE timeOutMillis)                                                     // connection time-out in milli seconds
      {
        // copy constructor parameters to local structure
        __connectionHandlerCallback__ = connectionHandlerCallback;
        __connectionHandlerCallbackParamater__ = connectionHandlerCallbackParamater;
        __socket__ = socket;
        __otherSideIP__ = otherSideIP;
        __timeOutMillis__ = timeOutMillis;
  
        // start thread that will handle the connection
        if (connectionHandlerCallback) {
          #define tskNORMAL_PRIORITY 1
          if (pdPASS == xTaskCreate ( __connectionHandler__,
                                      "TcpConnection",
                                      stackSize,
                                      this, // pass "this" pointer to static member function
                                      tskNORMAL_PRIORITY,
                                      NULL)) {
            __connectionState__ = TcpConnection::RUNNING;                                        
          } else {
            // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] xTaskCreate error in threaded constructor %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket, __func__);
            dmesg ("[TcpConnection] xTaskCreate error.");
          }
        }
      }


      /*
    
         TcpConnection constructor that creates not-threaded TcpConnection. Not-threaded TcpConnections run in threads of calling programs.
         Calling program can call instance member functions. Success can be tested if constructor returns not NULL (or with started () function after constructor returns.
     
      */


      TcpConnection (int socket,                                   // connection socket
                     String otherSideIP,                           // IP address of the other side of connection
                     TIME_OUT_TYPE timeOutMillis)                  // connection time-out in milli seconds
      {
        // copy constructor parameters to local structure
        __socket__ = socket;
        __otherSideIP__ = otherSideIP;
        __timeOutMillis__ = timeOutMillis;

        // set state
        __connectionState__ = TcpConnection::NOT_THREADED;
      }
  
      virtual ~TcpConnection () {
        closeConnection (); // close connection socket if it is still open - this will, in consequence, cause __connectionHandlerCallback__ to finish regulary by itself and clean up ist memory before returning (if threaded mode is used)
        while (__connectionState__ < TcpConnection::FINISHED) delay (1); // wait for __connectionHandler__ to finish before releasing the memory occupied by this instance (if threaded mode is used)
      }
  
      void closeConnection () {
        int connectionSocket;
        xSemaphoreTake (__TcpServerSemaphore__, portMAX_DELAY);
          connectionSocket = __socket__;
          __socket__ = -1;
        xSemaphoreGive (__TcpServerSemaphore__);
        if (connectionSocket != -1) close (connectionSocket); // can't close socket inside critical section, close it now
      }
  
      bool isClosed () { return __socket__ == -1; }
  
      bool isOpen () { return __socket__ != -1; }
  
      int getSocket () { return __socket__; }

      String getOtherSideIP () { return __otherSideIP__;  }
      
      String getThisSideIP () { // we can not get this information from constructor since connection is not necessarily established when constructor is called
        struct sockaddr_in thisAddress = {};
        socklen_t len = sizeof (thisAddress);
        if (getsockname (__socket__, (struct sockaddr *) &thisAddress, &len) == -1) return "";
        else                                                                        return inet_ntos (thisAddress.sin_addr);
        // port number could also be obtained if needed: ntohs (thisAddress.sin_port);
      }

      // define available data types
      enum AVAILABLE_TYPE {
        NOT_AVAILABLE = 0,  // no data is available to be read
        AVAILABLE = 1,      // data is available to be read
        ERROR = 3           // error in communication
      };

      AVAILABLE_TYPE available () { // checks if incoming data is pending to be read
        char buffer;
        if (-1 == recv (__socket__, &buffer, sizeof (buffer), MSG_PEEK)) {
          #define EAGAIN 11
          if (errno == EAGAIN || errno == EBADF) {
            if ((__timeOutMillis__ != INFINITE) && (millis () - __lastActiveMillis__ >= __timeOutMillis__)) {
              // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, millis () - __lastActiveMillis__, __func__);              
              __timeOut__ = true;
              closeConnection ();
              return TcpConnection::ERROR;
            }
            return TcpConnection::NOT_AVAILABLE;
          } else {
            // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] MSG_PEEK error: %i, time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno, millis () - __lastActiveMillis__, __func__);
            return TcpConnection::ERROR;
          }
        } else {
          return TcpConnection::AVAILABLE;
        }
      }

      int recvData (char *buffer, int bufferSize) { // returns the number of bytes actually received or 0 indicating error or closed connection
        while (true) {
          yield ();
          if (__socket__ == -1) return 0;
          switch (int recvTotal = recv (__socket__, buffer, bufferSize, 0)) {
            case -1:
              // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] recv error: %i, time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno, millis () - __lastActiveMillis__, __func__);
              #define EAGAIN 11
              #define ENAVAIL 119
              if (errno == EAGAIN || errno == ENAVAIL) {
                if ((__timeOutMillis__ == INFINITE) || (millis () - __lastActiveMillis__ < __timeOutMillis__)) {
                  delay (1);
                  break;
                }
              }
              // else close and continue to case 0
              // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, millis () - __lastActiveMillis__, __func__);              
              __timeOut__ = true;
              closeConnection ();
            case 0:   // connection is already closed
              return 0;
            default:
              __lastActiveMillis__ = millis ();
              return recvTotal;
          }
        }
      }

      int sendData (char *buffer, int bufferSize) { // returns the number of bytes actually sent or 0 indicatig error or closed connection
        int writtenTotal = 0;
        #define min(a,b) ((a)<(b)?(a):(b))
        while (bufferSize) {
          yield ();
          if (__socket__ == -1) return writtenTotal;
          switch (int written = send (__socket__, buffer, bufferSize, 0)) { // seems like ESP32 can send even larger packets
            case -1:
              // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] send error: %i, time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno, millis () - __lastActiveMillis__, __func__);
              #define EAGAIN 11
              #define ENAVAIL 119
              if (errno == EAGAIN || errno == ENAVAIL) {
                if ((__timeOutMillis__ == INFINITE) || (millis () - __lastActiveMillis__ < __timeOutMillis__)) {
                  delay (1);
                  break;
                }
              }
              // else close and continue to case 0
              // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] time-out: %lu in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, millis () - __lastActiveMillis__, __func__);              
              __timeOut__ = true;
              closeConnection ();
            case 0:   // socket is already closed
              return writtenTotal;
            default:
              writtenTotal += written;
              buffer += written;
              bufferSize -= written;
              __lastActiveMillis__ = millis ();
              break;
          }
        }
        return writtenTotal;
      }

      int sendData (char string []) { return (sendData (string, strlen (string))); }
      
      int sendData (String string) { return (sendData ((char *) string.c_str (), strlen (string.c_str ()))); }
  
      bool started () { return __connectionState__ == TcpConnection::RUNNING || __connectionState__ == TcpConnection::NOT_THREADED; } // returns true if connection thread has already started or if it runs in not-threaded mode
  
      bool timeOut () { return __timeOut__; } // if time-out occured
  
      void setTimeOut (TIME_OUT_TYPE timeOutMillis) { // user defined time-out if it differs from default one
        __timeOutMillis__ = timeOutMillis;
        __lastActiveMillis__ = millis ();
      }
  
      TIME_OUT_TYPE getTimeOut () { return __timeOutMillis__; } // returns time-out milliseconds

    private:
    
      void (* __connectionHandlerCallback__) (TcpConnection *, void *) = NULL;  // local copy of constructor parameters
      void *__connectionHandlerCallbackParamater__ = NULL;
      int __socket__ = -1;
      String __otherSideIP__;
      TIME_OUT_TYPE __timeOutMillis__;
  
      unsigned long __lastActiveMillis__ = millis ();                   // needed for time-out detection
      bool __timeOut__ = false;                                         // "time-out" flag
  
      enum CONNECTION_THREAD_STATE_TYPE {
        NOT_STARTED = 9,                                                // initial state
        NOT_THREADED = 8,                                               // the only state for not-threaded connections
        RUNNING = 1,                                                    // connection thread has started
        FINISHED = 2                                                    // connection thread has finished, instance can unload
      };
      
      CONNECTION_THREAD_STATE_TYPE __connectionState__ = TcpConnection::NOT_STARTED;
    
      static void __connectionHandler__ (void *threadParameters) {      // envelope for connection handler callback function
        TcpConnection *ths = (TcpConnection *) threadParameters;        // get "this" pointer
        if (ths->__connectionHandlerCallback__) ths->__connectionHandlerCallback__ (ths, ths->__connectionHandlerCallbackParamater__);
        ths->__connectionState__ = TcpConnection::FINISHED;
        delete (ths);                                                   // unload instnance, do not address any instance members form this point further
        vTaskDelete (NULL);
      }
  
  };
  
  
  /*
      There are two types of TcpServers:
       
        Threaded TcpServers run listeners in their own threads, accept incoming connections and start new threaded TcpConnections for each one.
        This is an usual type of TcpServer.
      
        Not-threaded TcpServers run in threads of calling programs. They only wait or a single connection to arrive and handle it.
        This type of TcpServer is only used for pasive FTP data connections.

  */

  
  class TcpServer {                                                
    
    public:
  

      /*
    
         TcpServer constructor that creates threaded TcpServer. Listener runs in its own thread and creates a new thread for each
         arriving connection. Success can be tested with started () function after constructor returns.
     
      */

      
      TcpServer      (void (* connectionHandlerCallback) (TcpConnection *connection, void *parameter),  // a reference to callback function that will handle the connection
                      void *connectionHandlerCallbackParameter,                                         // a reference to parameter that will be passed to connectionHandlerCallback
                      size_t connectionStackSize,                                                       // stack size of a thread where connection runs - this value depends on what server really does (see connectionHandler function) should be set appropriately (not to use too much memory and not to crush ESP)
                      TIME_OUT_TYPE timeOutMillis,                                                      // connection time-out in milli seconds
                      String serverIP,                                                                  // server IP address, 0.0.0.0 for all available IP addresses
                      int serverPort,                                                                   // server port
                      bool (* firewallCallback) (String connectingIP)                                   // a reference to callback function that will be celled when new connection arrives
                     ) {
        // copy constructor parameters to local structure
        __connectionHandlerCallback__ = connectionHandlerCallback;
        __connectionHandlerCallbackParameter__ = connectionHandlerCallbackParameter;
        __connectionStackSize__ = connectionStackSize;
        __timeOutMillis__ = timeOutMillis;
        __serverIP__ = serverIP;
        __serverPort__ = serverPort;
        __firewallCallback__ = firewallCallback;
  
        // start listener thread
        #define tskNORMAL_PRIORITY 1
        if (pdPASS == xTaskCreate (__listener__,
                                   "TcpServer",
                                   2048, // 2 KB stack is large enough for TCP listener
                                   this, // pass "this" pointer to static member function
                                   tskNORMAL_PRIORITY,
                                   NULL)) {
          while (__listenerState__ == TcpServer::NOT_RUNNING) delay (1); // listener thread has started successfully and will change listener state soon
        } else {
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i] xTaskCreate error in threaded constructor %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __func__);
          dmesg ("[TcpServer] xTaskCreate error in threaded constructor.");
        }
      }
  

      /*
    
         TcpServer constructor that creates not-threaded TcpServer. Listener runs in its own thread and creates only one TcpConnection
         once it arrives. Success can be tested with started () function after constructor returns.
     
      */

      
      TcpServer      (TIME_OUT_TYPE timeOutMillis,                    // connection time-out in milli seconds
                      String serverIP,                                // server IP address, 0.0.0.0 for all available IP addresses
                      int serverPort,                                 // server port
                      bool (* firewallCallback) (String connectingIP) // a reference to callback function that will be celled when new connection arrives
                     ) {
        // copy constructor parameters to local structure
        __timeOutMillis__ = timeOutMillis;
        __serverIP__ =  serverIP;
        __serverPort__ = serverPort;
        __firewallCallback__ = firewallCallback;
        
        // start listener thread
        #define tskNORMAL_PRIORITY 1
        if (pdPASS == xTaskCreate (__listener__,
                                   "TcpServer",
                                   2048, // 2 KB stack is large enough for TCP listener
                                   this, // pass "this" pointer to static member function
                                   tskNORMAL_PRIORITY,
                                   NULL)) {
          while (__listenerState__ == TcpServer::NOT_RUNNING) delay (1); // listener thread has started successfully and will change listener state soon
        } else {
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i] xTaskCreate error in not-threaded constructor %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __func__);
          dmesg ("[TcpServer] xTaskCreate error in not-threaded constructor.");
        }
      }
  
      virtual ~TcpServer ()                     {
        if (__connection__) delete (__connection__); // close not-threaded connection if it has been established
        __instanceUnloading__ = true; // signal __listener__ to stop
        while (__listenerState__ < TcpServer::FINISHED) delay (1); // wait for __listener__ to finish before releasing the memory occupied by this instance
      }
  
      String getServerIP () { return __serverIP__; } // information from constructor
  
      int getServerPort () { return __serverPort__; } // information from constructor
  
      TcpConnection *connection () { return __connection__; } // not-threaded TcpServer only: calling program will handle the connection through this reference
  
      bool timeOut () { // not-threaded TcpServer only: returns true if time-out has occured while not-threaded TcpServer is waiting for a connection
        if (__threadedMode__ ()) return false; 
        if (__timeOutMillis__ == INFINITE) return false;
        if (millis () - __lastActiveMillis__ > __timeOutMillis__) return true;
        return false;
      }
  
      bool started (void) { // returns true if listener thread has already started - this flag is set before the constructor returns
        while (__listenerState__ < TcpServer::ACCEPTING_CONNECTIONS) delay (10); // wait if listener is still getting ready
        return (__listenerState__ == TcpServer::ACCEPTING_CONNECTIONS);
      }
  
    private:
      friend class telnetServer;
      friend class ftpServer;
      friend class httpServer;
      friend class httpsServer;
  
      void (* __connectionHandlerCallback__) (TcpConnection *, void *) = NULL; // local copy of constructor parameters
      void *__connectionHandlerCallbackParameter__ = NULL;
      size_t __connectionStackSize__;
      TIME_OUT_TYPE __timeOutMillis__;
      String __serverIP__;
      int __serverPort__;
      bool (* __firewallCallback__) (String);
  
      TcpConnection *__connection__ = NULL;                           // pointer to TcpConnection instance (not-threaded TcpServer only)
      enum LISTENER_STATE_TYPE {
        NOT_RUNNING = 9,                                              // initial state
        RUNNING = 1,                                                  // preparing listening socket to start accepting connections
        ACCEPTING_CONNECTIONS = 2,                                    // listening socket started accepting connections
        STOPPED = 3,                                                  // stopped accepting connections
        FINISHED = 4                                                  // listener thread has finished, instance can unload
      };
      LISTENER_STATE_TYPE __listenerState__ = NOT_RUNNING;
      bool __instanceUnloading__ = false;                             // instance "unloading" flag
  
      unsigned long __lastActiveMillis__ = millis ();                 // used for time-out detection (not-threaded TcpServer only)
      bool __timeOut__ = false;                                       // used to report time-out (not-threaded TcpServer only)
  
      bool __threadedMode__ () { return (__connectionHandlerCallback__ != NULL); } // returns true if server is working in threaded mode - in this case it has to have a callback function
  
      bool __callFirewallCallback__ (String connectingIP) { return __firewallCallback__ ? __firewallCallback__ (connectingIP) : true; } // calls firewall function

      void __newConnection__ (int connectionSocket, String otherSideIP) { // creates new TcpConnection instance for connectionSocket
        TcpConnection *newConnection;
        if (__threadedMode__ ()) { // in threaded mode we pass connectionHandler address to TcpConnection instance
          newConnection = new TcpConnection (__connectionHandlerCallback__, __connectionHandlerCallbackParameter__, __connectionStackSize__, connectionSocket, otherSideIP, __timeOutMillis__);
          if (newConnection) {
            if (!newConnection->started ()) delete (newConnection); // this also closes the connection
          } else {
            close (connectionSocket); // close the connection
          }
        } else { // in non-threaded mode create non-threaded TcpConnection instance
          newConnection = new TcpConnection (connectionSocket, otherSideIP, __timeOutMillis__);
          __connection__ = newConnection; // in not-threaded mode calling program will need reference to a connection
        }
      }
  
      static void __listener__ (void *taskParameters) { // listener running in its own thread imlemented as static memeber function
        TcpServer *ths = (TcpServer *) taskParameters; // get "this" pointer 
        ths->__listenerState__ = TcpServer::RUNNING; 
        int listenerSocket = -1;
        while (!ths->__instanceUnloading__) { // prepare listener socket - repeat this in a loop in case something goes wrong
          // make listener TCP socket (SOCK_STREAM) for internet protocol family (PF_INET) - protocol family and address family are connected (PF__INET protokol and AF_INET)
          listenerSocket = socket (PF_INET, SOCK_STREAM, 0);
          if (listenerSocket == -1) {
            delay (1000); // try again after 1 s
            continue;
          }
          // make address reusable - so we won't have to wait a few minutes in case server will be restarted
          int flag = 1;
          setsockopt (listenerSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof (flag));
          // bind listener socket to IP address and port
          struct sockaddr_in serverAddress;
          memset (&serverAddress, 0, sizeof (struct sockaddr_in));
          serverAddress.sin_family = AF_INET;
          serverAddress.sin_addr.s_addr = inet_addr (ths->getServerIP ().c_str ());
          serverAddress.sin_port = htons (ths->getServerPort ());
          if (bind (listenerSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) goto terminateListener;
          // mark socket as listening socket
          #define BACKLOG 16 // queue lengthe of (simultaneously) arrived connections - actual active connection number might be larger 
          if (listen (listenerSocket, BACKLOG) == -1) goto terminateListener;
          // make socket non-blocking
          if (fcntl (listenerSocket, F_SETFL, O_NONBLOCK) == -1) goto terminateListener;
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] listener started on %s:%i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), listenerSocket, ths->getServerIP ().c_str (), ths->getServerPort ());
          while (!ths->__instanceUnloading__) { // handle incoming connections
            delay (1);
            if (!ths->__threadedMode__ () && ths->timeOut ()) goto terminateListener; // checing time-out makes sense only when working as not-threaded TcpServer
            // accept new connection
            ths->__listenerState__ = TcpServer::ACCEPTING_CONNECTIONS; 
            int connectionSocket;
            struct sockaddr_in connectingAddress;
            socklen_t connectingAddressSize = sizeof (connectingAddress);
            connectionSocket = accept (listenerSocket, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
            if (connectionSocket != -1) { // non-blocking socket keeps returning -1 until new connection arrives
              if (!ths->__callFirewallCallback__ (inet_ntos (connectingAddress.sin_addr))) {
                close (connectionSocket);
                dmesg ("[TcpServer] firewall rejected connection from " + inet_ntos (connectingAddress.sin_addr));
                continue;
              }
              if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
                close (connectionSocket);
                continue;
              }
              ths->__newConnection__ (connectionSocket, inet_ntos (connectingAddress.sin_addr));
              if (!ths->__threadedMode__ ()) goto terminateListener; // in non-threaded mode server only accepts one connection
            } // new connection
          } // handle incomming connections
        } // prepare listener socket
  terminateListener:
        ths->__listenerState__ = TcpServer::STOPPED;
        close (listenerSocket);
        ths->__listenerState__ = TcpServer::FINISHED;
        vTaskDelete (NULL); // terminate this thread
      }
  
  };
  
    
  /*

    TcpClient creates not-threaded TcpConnection. 
    
    Successful instance creation can be tested using connection () != NULL after constructor returns, but that doesn't mean that
    the TcpConnections is already established, it has only been requested so far. If connection fails it will be shown through
    TcpConnection time-out (). Therefore it may not be a good idea to set time-out to INFINITE.

  */
  

  class TcpClient {
  
    public:
  
      TcpClient      (String serverName,                            // server name or IP address
                      int serverPort,                               // server port
                      TIME_OUT_TYPE timeOutMillis                   // connection time-out in milli seconds
                     ) {

        // get server IP address
        IPAddress serverIP;
        if (!WiFi.hostByName (serverName.c_str (), serverIP)) { 
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] hostByName error in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, __func__);
          dmesg ("[TcpClient] hostByName could not find host " + serverName);
          return;
        } 
                              
        // make TCP socket (SOCK_STREAM) for internet protocol family (PF_INET) - protocol family and address family are connected (PF__INET protokol and AF__INET)
        int connectionSocket = socket (PF_INET, SOCK_STREAM, 0);
        if (connectionSocket == -1) {
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] socket error in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, __func__);
          dmesg ("[TcpClient] soclet error.");          
          return;
        }
        // make the socket non-blocking - needed for time-out detection
        if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] fcntl error in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, __func__);
          dmesg ("[TcpClient] fcntl error.");          
          close (connectionSocket);
          return;
        }
        // connect to server
        struct sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons (serverPort);
        serverAddress.sin_addr.s_addr = inet_addr (serverIP.toString ().c_str ());
        if (connect (connectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
          // DEBUG: Serial.printf ("[Thread: %lu][Core: %i][Socket: %i] connect error in %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, __func__);
          #define EINPROGRESS 119
          if (errno != EINPROGRESS) {
            dmesg ("[TcpClient] connect error " + String (errno) + ".");          
            close (connectionSocket);
            return;
          }
        } // it is likely that socket is not open yet at this point
        __connection__ = new TcpConnection (connectionSocket, serverIP.toString (), timeOutMillis); // load not-threaded TcpConnection instance
        if (!__connection__) {
          dmesg ("[TcpClient] could not create a new TcpConnection.");
          close (connectionSocket);
          return;
        }
      }
  
      ~TcpClient () { delete (__connection__); }
  
      TcpConnection *connection () { return __connection__; }         // calling program will handle the connection through this reference - connection is set, before constructor returns but TCP connection may not be established (yet) at this time or possibly even not at all

    private:
  
      TcpConnection *__connection__ = NULL;                           // TcpConnection instance used by TcpClient instance
  
  };

#endif
