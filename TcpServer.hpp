/*
   TcpServer.hpp


    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    TcpServer.hpp contains a minimalistic IPv4 threaded TCP server for ESP32 / Arduino environment with:
      - time-out functionality,
      - firewall functionality.

    Four types of objects are introduced in order to make programming interface as simple as possible
    (a programmer does not have to deal with sockets and threads by himself):
      - threaded TcpServer (with firewall functionality),
      - non-threaded TcpServer (with time-out functionality while waiting for a connection, firewall functionality),
      - TcpConnection (with time-out functionality while handling a connection),
      - non-threaded TcpClient (with time-out functionality while handling the connection).

   History:
            - first release,
              October 31, 2018, Bojan Jurca
            - added user-defined connectionHandler parameter, added TcpConnection::getTimeOut (),
              November 22, 2018, Bojan Jurca
            - added SPIFFSsemaphore and delay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876),
              April 13, 2019, Bojan Jurca
            - added available () member function to TcpConnection object
              May 12, 2019, Bojan Jurca
            - added sendData (char []) and sendData (String) to TcpConnection object
              September 5, 2019, Bojan Jurca
            - bug fixes (stopping the server) and minor improvements,
              minor structural changes
              September 14, 2019, Bojan Jurca
            - increased BACKLOG from 5 to 16,
              inet_ntoa replaced with thread-safe __inet_ntos__
              December 27, 2019, Bojan Jurca
            - added TcpDmesg to log/display system messages/errors
              added support for SSL extensions, SSL extensions themselves included yet
              February 26, 2020, Bojan Jurca
            - elimination of compiler warnings and some bugs
              Jun 10, 2020, Bojan Jurca
            - port from SPIFFS to FAT file system, adjustment for Arduino 1.8.13
            - added isOpened and isClosed functions to TcpConnection
              Nov 14, 2020, Bojan Jurca

*/


#ifndef __TCP_SERVER__
#define __TCP_SERVER__

// dmesg
void __TcpDmesg__ (String message) {
  Serial.printf ("[%10lu] %s\n", millis (), message.c_str ());
}
void (* TcpDmesg) (String) = __TcpDmesg__; // use this pointer to display / record system messages


// ----- includes, definitions and supporting functions -----

#include <WiFi.h>
#include <lwip/sockets.h>

// TcpConnection can be used in two different modes:
// - threaded TcpConnection creates a new thread and runs connectionHandlerCallback function through it
//    successful instance creation can be tested using started () member function - result is available immediately after constructor returns
//      if started () then just leave instance running, it will delete () itself when connection finishes
//      else you must delete () instance yourself
//    TO DO: make constructor return NULL in unsuccessful
// - non-threaded TcpConnection can be controlled from calling program
//    you must delete () instance yourself when no longer needed

// controll TcpServer critical sections
portMUX_TYPE csTcpConnectionInternalStructure = portMUX_INITIALIZER_UNLOCKED;

// thread-safe variant of inet_ntoa
String __inet_ntos__ (ip_addr_t addr) { // equivalent of inet_ntoa (struct in_addr addr)
  // inet_ntoa returns pointer to static string which may
  // be a problem in multi-threaded environment
  return String (*(((byte *) &addr) + 0)) + "." +
         String (*(((byte *) &addr) + 1)) + "." +
         String (*(((byte *) &addr) + 2)) + "." +
         String (*(((byte *) &addr) + 3));
}
String __inet_ntos__ (in_addr addr) { // equivalent of inet_ntoa (struct in_addr addr)
  // inet_ntoa returns pointer to static string which may
  // be a problem in multi-threaded environment
  return String (*(((byte *) &addr) + 0)) + "." +
         String (*(((byte *) &addr) + 1)) + "." +
         String (*(((byte *) &addr) + 2)) + "." +
         String (*(((byte *) &addr) + 3));
}


class TcpConnection {

  public:

    // define time-out data type
    enum TIME_OUT_TYPE {
      INFINITE = 0  // infinite time-out
    };

    // threaded mode constructor
    TcpConnection (void (* connectionHandlerCallback) (TcpConnection *, void *),  // a reference to callback function that will handle the connection
                   void *connectionHandlerCallbackParamater,                      // a reference to parameter that will be passed to connectionHandlerCallback
                   unsigned int stackSize,                                        // stack size of a thread where connection runs - this value depends on what server really does (see connectionHandler function) should be set appropriately
                   int socket,                                                    // connection socket
                   char *otherSideIP,                                             // IP address of the other side of connection - 15 characters at most!
                   unsigned long timeOutMillis)                                   // connection time-out in milli seconds
    {
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] threaded constructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket);
      // copy constructor parameters to local structure
      __connectionHandlerCallback__ = connectionHandlerCallback;
      __connectionHandlerCallbackParamater__ = connectionHandlerCallbackParamater;
      __socket__ = socket;
      strcpy (__otherSideIP__, otherSideIP);
      __timeOutMillis__ = timeOutMillis;

      // start connection handler thread (threaded mode)
      __connectionState__ = TcpConnection::RUNNING;
      if (connectionHandlerCallback) {
        #define tskNORMAL_PRIORITY 1
        if (pdPASS != xTaskCreate ( __connectionHandler__,
                                    "TcpConnection",
                                    stackSize,
                                    this, // pass "this" pointer to static member function
                                    tskNORMAL_PRIORITY,
                                    NULL)) {
          __connectionState__ = TcpConnection::NOT_STARTED;
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] threaded constructor: xTaskCreate () error\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket);
          // TO DO: make constructor return NULL
        }
      }
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] } threaded constructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket);
    }

    // non-threaded mode constructor
    TcpConnection (int socket,                                   // connection socket
                   char *otherSideIP,                            // IP address of the other side of connection - 15 characters at most!
                   unsigned long timeOutMillis)                  // connection time-out in milli seconds
    {
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] non-threaded constructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket);
      // copy constructor parameters to local structure
      __socket__ = socket;
      strcpy (__otherSideIP__, otherSideIP);
      __timeOutMillis__ = timeOutMillis;
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] } non-threaded constructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), socket);
    }

    virtual ~TcpConnection ()                 {
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] destructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
      // close connection socket if it is still opened - this will, in consequence, cause __connectionHandlerCallback__ to finish -
      // we may not use vTaskDelete here since __connectionHandlerCallback__ variables would still remain in memory which would cause memory leaks -
      // __connectionHandlerCallback__ must finish regulary by itself and clean up ist memory before returning
      closeConnection ();
      // wait for __connectionHandler__ to finish before releasing the memory occupied by this instance
      while (__connectionState__ < TcpConnection::FINISHED) delay (1);
      // __connectionHandler__ thread will terminate itself
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] } destructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
      // Serial.printf ("~TcpConnection ()\n");
    }

    virtual void closeConnection ()           {
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] closeConnection {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
      int connectionSocket;
      portENTER_CRITICAL (&csTcpConnectionInternalStructure);
        connectionSocket = __socket__;
        __socket__ = -1;
      portEXIT_CRITICAL (&csTcpConnectionInternalStructure);
      if (connectionSocket != -1) { // can not close socket inside of critical section, close it now
        // if (shutdown (connectionSocket, SHUT_RD) == -1) log_e ("[Thread:%i][Core:%i][Socket:%i] closeConnection: shutdown () error %i\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno);
        // if (close (connectionSocket) == -1);            // log_e ("[Thread:%i][Core:%i][Socket:%i] closeConnection: close () error %i\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno);
        close (connectionSocket);
      }
      // log_v ("[Thread:%lu][Core:%i][Socket:%i] } closeConnection\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
    }

    virtual bool isClosed ()                  { return __socket__ == -1; }

    virtual bool isOpened ()                  { return __socket__ != -1; }

    int getSocket () { return __socket__; }
    
    char *getThisSideIP ()                    {
      // we can not get this information from constructor since connection is not necessarily established when constructor is called
      // if this is a server then we are looking for server side IP, if this is a client then we are looking for client side IP
      struct sockaddr_in thisAddress = {};
      socklen_t len = sizeof (thisAddress);
      if (getsockname (__socket__, (struct sockaddr *) &thisAddress, &len) == -1) {
        // log_e ("[Thread:%lu][Core:%i][Socket:%i] getThisSideIP: getsockname () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, errno);
        *__thisSideIP__ = 0; // return empty string rather than NULL (error handling is easier - you can sscanf () from "" but not from NULL)
      } else {
        portENTER_CRITICAL (&csTcpConnectionInternalStructure);
          if (!*__thisSideIP__) strcpy (__thisSideIP__, (char *) __inet_ntos__ (thisAddress.sin_addr).c_str ());
        portEXIT_CRITICAL (&csTcpConnectionInternalStructure);
      }
      // port number can be found this way if needed: ntohs (thisAddress.sin_port);
      return __thisSideIP__;
    }

    char *getOtherSideIP ()                   {
      return __otherSideIP__;  // information from constructor
    }

    virtual int recvData (char *buffer, int bufferSize)                   // returns the number of bytes actually received or 0 indicating error or closed connection
    {
      // Serial.printf ("recvData (%lu, %i)\n", (unsigned long) buffer, bufferSize);
      while (true) {
        yield ();
        if (__socket__ == -1) return 0;
        switch (int recvTotal = recv (__socket__, buffer, bufferSize, 0)) {
          case -1:
            // Serial.printf ("recvData errno: %i timeout: %i\n", errno, millis () - __lastActiveMillis__);
            #define EAGAIN 11
            #define ENAVAIL 119
            if (errno == EAGAIN || errno == ENAVAIL) {
              if ((__timeOutMillis__ == TcpConnection::INFINITE) || (millis () - __lastActiveMillis__ < __timeOutMillis__)) { // non-blocking -----
                delay (1);
                break;
              }
            }
            // else close and continue to case 0
            // Serial.printf ("[%s] TcpConnection time-out\n", __func__);
            __timeOut__ = true;
            closeConnection ();
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] recvData: time-out\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
          case 0:   // connection is already closed
            return 0;
          default:
            __lastActiveMillis__ = millis ();
            // log_i ("[Thread:%lu][Core:%i][Socket:%i] recvData: %i bytes\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, recvTotal);
            return recvTotal;
        }
      }
    }

    // define available data types
    enum AVAILABLE_TYPE {
      NOT_AVAILABLE = 0,  // no data is available to be read
      AVAILABLE = 1,      // data is available to be read
      ERROR = 3           // error in communication
    };
    AVAILABLE_TYPE available ()               { // checks if incoming data is pending to be read
      char buffer;
      if (-1 == recv (__socket__, &buffer, sizeof (buffer), MSG_PEEK)) {
        #define EAGAIN 11
        if (errno == EAGAIN || errno == EBADF) {
          if ((__timeOutMillis__ != TcpConnection::INFINITE) && (millis () - __lastActiveMillis__ >= __timeOutMillis__)) {
            // Serial.printf ("[%s] TcpConnection time-out\n", __func__);
            __timeOut__ = true;
            closeConnection ();
            // log_e ("[Thread:%lu][Core:%i][Socket:%i] sendData: time-out\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
            return TcpConnection::ERROR;
          }
          return TcpConnection::NOT_AVAILABLE;
        } else {
          // Serial.printf ("recv (MSG_PEEK) error: %s: %i\n", strerror (errno), errno);
          return TcpConnection::ERROR;
        }
      } else {
        return TcpConnection::AVAILABLE;
      }
    }

    virtual int sendData (char *buffer, int bufferSize)                   // returns the number of bytes actually sent or 0 indicatig error or closed connection
    {
      // Serial.printf ("sendData (%lu, %i)\n", (unsigned long) buffer, bufferSize);
      int writtenTotal = 0;
      #define min(a,b) ((a)<(b)?(a):(b))
      while (bufferSize) {
        yield ();
        if (__socket__ == -1) return writtenTotal;
        //switch (int written = send (__socket__, buffer, min (bufferSize, 2048), 0)) { // ESP can send packets length of max 2 KB but MTU (default) size of 1500
        switch (int written = send (__socket__, buffer, bufferSize, 0)) { // seems like ESP32 can send even larger packets
          case -1:
            // Serial.printf ("sendData errno: %i timeout: %i\n", errno, millis () - __lastActiveMillis__);
            #define EAGAIN 11
            #define ENAVAIL 119
            if (errno == EAGAIN || errno == ENAVAIL) {
              if ((__timeOutMillis__ == TcpConnection::INFINITE) || (millis () - __lastActiveMillis__ < __timeOutMillis__)) {
                delay (1);
                break;
              }
            }
            // else close and continue to case 0
            // Serial.printf ("[%s] TcpConnection time-out\n", __func__);
            __timeOut__ = true;
            closeConnection ();
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] sendData: time-out\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
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
      // log_i ("[Thread:%lu][Core:%i][Socket:%i] sendData: %i bytes\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__, writtenTotal);
      return writtenTotal;
    }

    virtual int sendData (char string []) { return (sendData (string, strlen (string))); }
    
    virtual int sendData (String string) { return (sendData ((char *) string.c_str (), strlen (string.c_str ()))); }

    virtual bool started ()                   {
      return __connectionState__ == TcpConnection::RUNNING || !__connectionHandlerCallback__;  // returns true if connection thread has already started - this flag is set before the constructor returns - or if connection runs in non-threaded mode
    }

    bool timeOut ()                           { return __timeOut__; } // if time-out occured

    void setTimeOut (unsigned long timeOutMillis)                 // user defined time-out if it differs from default one
    {
      __timeOutMillis__ = timeOutMillis;
      __lastActiveMillis__ = millis ();
    }

    unsigned long getTimeOut ()               { return __timeOutMillis__; } // returns time-out milliseconds

  private:

    void (* __connectionHandlerCallback__) (TcpConnection *, void *) = NULL;  // local copy of constructor parameters
    void *__connectionHandlerCallbackParamater__ = NULL;
    int __socket__ = -1;
    char __otherSideIP__ [16];
    unsigned long __timeOutMillis__;

    unsigned long __lastActiveMillis__ = millis ();                   // needed for time-out detection
    bool __timeOut__ = false;                                         // "time-out" flag
    char __thisSideIP__ [16] = {};                                    // if this is a server socket then this is going to be a server IP, if this is a client socket then this is going to be client IP

    enum CONNECTION_THREAD_STATE_TYPE {
      NOT_STARTED = 9,                                                // initial state
      RUNNING = 1,                                                    // connection thread has started
      FINISHED = 2                                                    // connection thread has finished, instance can unload
    };
    CONNECTION_THREAD_STATE_TYPE __connectionState__ = TcpConnection::NOT_STARTED;

    virtual void __callConnectionHandlerCallback__ () {               // calls connection handler function (just one time from another thread)
      // log_i ("[Thread:%lu][Core:%i][Socket:%i] connection started\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
      if (__connectionHandlerCallback__) __connectionHandlerCallback__ (this, __connectionHandlerCallbackParamater__);
      // log_i ("[Thread:%lu][Core:%i][Socket:%i] connection ended\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), __socket__);
    }

    static void __connectionHandler__ (void *threadParameters) {                                         // envelope for connection handler callback function
      TcpConnection *ths = (TcpConnection *) threadParameters; // this is how you pass "this" pointer to static memeber function
      // log_v ("[Thread:%lu][Core:%i] __connectionHandler__ {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      ths->__callConnectionHandlerCallback__ ();
      ths->__connectionState__ = TcpConnection::FINISHED;
      delete (ths);
      // log_v ("[Thread:%lu][Core:%i] } __connectionHandler__\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      vTaskDelete (NULL);
    }

};


// TcpServer can be used in two different modes:
// - threaded TcpServer starts new thread that listens for incomming connection and creates a threaded TcpConnection instance for each of them
//    successful listener thread creation can be tested using started () member function - result is available immediately after constructor returns
//    TO DO: make constructor return NULL in unsuccessful
// - non-threaded TcpServer starts new thread that accepts only one connection and creates a non-threaded TcpConnection instance for it
//    successful listener thread creation can be tested using started () member function - result is available immediately after constructor returns
//    when connection arrives connection () memmber funcion will hold a reference to it is not likely to happen when constructor returns,
//    calling program should test connection () to NULL value until connection () or timeOut () occur
//    TO DO: make constructor return NULL in unsuccessful

class TcpServer {                                                 // threaded TCP server

  public:

    // constructor of a threaded TCP server
    TcpServer      (void (* connectionHandlerCallback) (TcpConnection *, void *), // a reference to callback function that will handle the connection
                    void *connectionHandlerCallbackParameter,             // a reference to parameter that will be passed to connectionHandlerCallback
                    unsigned int connectionStackSize,                     // stack size of a thread where connection runs - this value depends on what server really does (see connectionHandler function) should be set appropriately (not to use too much memory and not to crush ESP)
                    unsigned long timeOutMillis,                          // connection time-out in milli seconds
                    char *serverIP,                                       // server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                    int serverPort,                                       // server port
                    bool (* firewallCallback) (char *)                    // a reference to callback function that will be celled when new connection arrives
                   )                          {
      // log_v ("[Thread:%lu][Core:%i] threaded constructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      // copy constructor parameters to local structure
      __connectionHandlerCallback__ = connectionHandlerCallback;
      __connectionHandlerCallbackParameter__ = connectionHandlerCallbackParameter;
      __connectionStackSize__ = connectionStackSize;
      __timeOutMillis__ = timeOutMillis;
      strcpy (__serverIP__, serverIP);
      __serverPort__ = serverPort;
      __firewallCallback__ = firewallCallback;

      // start listener thread
      __listenerState__ = TcpServer::NOT_RUNNING;
#define tskNORMAL_PRIORITY 1
      if (pdPASS != xTaskCreate (__listener__,
                                 "TcpListener",
                                 2048, // 2 KB stack is large enough for TCP listener
                                 this, // pass "this" pointer to static member function
                                 tskNORMAL_PRIORITY,
                                 NULL)) {
        // log_e ("[Thread:%lu][Core:%i] threaded constructor: xTaskCreate () error\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
        // TO DO: make constructor return NULL
      }
      while (__listenerState__ == TcpServer::NOT_RUNNING) delay (1); // listener thread has started successfully and will change listener state soon
      // log_v ("[Thread:%lu][Core:%i] } threaded constructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
    }

    // constructor of a non-threaded TCP server
    TcpServer      (unsigned long timeOutMillis,                  // connection time-out in milli seconds
                    char *serverIP,                               // server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                    int serverPort,                               // server port
                    bool (* firewallCallback) (char *)            // a reference to callback function that will be celled when new connection arrives
                   )                          {
      // log_v ("[Thread:%lu][Core:%i] non-threaded constructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      // copy constructor parameters to local structure
      __timeOutMillis__ = timeOutMillis;
      strcpy (__serverIP__, serverIP);
      __serverPort__ = serverPort;
      __firewallCallback__ = firewallCallback;
      // start listener thread
      __listenerState__ = TcpServer::NOT_RUNNING;
#define tskNORMAL_PRIORITY 1
      if (pdPASS != xTaskCreate (__listener__,
                                 "TcpListener",
                                 2048, // 2 KB stack is large enough for TCP listener
                                 this, // pass "this" pointer to static member function
                                 tskNORMAL_PRIORITY,
                                 NULL)) {
        // log_e ("[Thread:%lu][Core:%i] non-threaded constructor: xTaskCreate () error\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
        // TO DO: make constructor return NULL
      }
      while (__listenerState__ == TcpServer::NOT_RUNNING) delay (1); // listener thread has started successfully and will change listener state soon
      // log_v ("[Thread:%lu][Core:%i] } non-threaded constructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
    }

    virtual ~TcpServer ()                     {
      // log_v ("[Thread:%lu] destructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      if (__connection__) delete (__connection__); // close non-threaded mode connection if it has been established
      __instanceUnloading__ = true; // signal __listener__ to stop
      while (__listenerState__ < TcpServer::FINISHED) delay (1); // wait for __listener__ to finish before releasing the memory occupied by this instance
      // log_v ("[Thread:%lu][Core:%i] } destructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
    }

    char *getServerIP ()                      {
      return __serverIP__;  // information from constructor
    }

    int getServerPort ()                      {
      return __serverPort__;  // information from constructor
    }

    TcpConnection *connection ()              {
      return __connection__;  // calling program will handle the connection through this reference (non-threaded mode only)
    }

    bool timeOut ()                           {                   // returns true if time-out has occured while non-threaded TCP server is waiting for a connection (it makes no sense in threaded TCP servers)
      if (__threadedMode__ ()) return false; // time-out makes no sense for threaded TcpServer
      if (__timeOutMillis__ == TcpConnection::INFINITE) return false;
      else if (millis () - __lastActiveMillis__ > __timeOutMillis__) {
        // log_e ("[Thread:%lu][Core:%i] time-out\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
        return true;
      } else return false;
    }

    virtual bool started (void)               { // returns true if listener thread has already started - this flag is set before the constructor returns
      while (__listenerState__ < TcpServer::ACCEPTING_CONNECTIONS) delay (10); // wait if listener is getting ready
      return (__listenerState__ == TcpServer::ACCEPTING_CONNECTIONS);
    }

  private:
    friend class telnetServer;
    friend class ftpServer;
    friend class httpServer;

    void (* __connectionHandlerCallback__) (TcpConnection *, void *) = NULL; // local copy of constructor parameters
    void *__connectionHandlerCallbackParameter__ = NULL;
    unsigned int __connectionStackSize__;
    unsigned long __timeOutMillis__;
    char __serverIP__ [16];
    int __serverPort__;
    bool (* __firewallCallback__) (char *IP);

    TcpConnection *__connection__ = NULL;                           // pointer to TcpConnection instance (non-threaded mode only)
    enum LISTENER_STATE_TYPE {
      NOT_RUNNING = 9,                                              // initial state
      RUNNING = 1,                                                  // preparing listening socket to start accepting connections
      ACCEPTING_CONNECTIONS = 2,                                    // listening socket started accepting connections
      STOPPED = 3,                                                  // stopped accepting connections
      FINISHED = 4                                                  // listener thread has finished, instance can unload
    };
    LISTENER_STATE_TYPE __listenerState__ = NOT_RUNNING;
    bool __instanceUnloading__ = false;                             // instance "unloading" flag

    unsigned long __lastActiveMillis__ = millis ();                 // used for time-out detection in non-threaded mode
    bool __timeOut__ = false;                                       // used to report time-out

    bool __threadedMode__ ()                  {
      return (__connectionHandlerCallback__ != NULL);  // returns true if server is working in threaded mode
    }

    bool __callFirewallCallback__ (char *IP)  {
      return __firewallCallback__ ? __firewallCallback__ (IP) : true;  // calls firewall function
    }

    virtual void __newConnection__ (int connectionSocket, char *clientIP)   // creates new TcpConnection instance for connectionSocket
    {
      TcpConnection *newConnection;
      if (__threadedMode__ ()) { // in threaded mode we pass connectionHandler address to TcpConnection instance
        newConnection = new TcpConnection (__connectionHandlerCallback__, __connectionHandlerCallbackParameter__, __connectionStackSize__, connectionSocket, clientIP, __timeOutMillis__);
        if (newConnection) {
          if (!newConnection->started ()) {
            delete (newConnection); // also closes the connection
          }
        } else {
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] new () error\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket);
          close (connectionSocket); // close the connection
        }
      } else { // in non-threaded mode create non-threaded TcpConnection instance
        newConnection = new TcpConnection (connectionSocket, clientIP, __timeOutMillis__);
        __connection__ = newConnection; // in not-threaded mode calling program will need reference to a connection
      }
    }

    static void __listener__ (void *taskParameters) {                                        // listener running in its own thread imlemented as static memeber function
      TcpServer *ths = (TcpServer *) taskParameters; // this is how you pass "this" pointer to static memeber function
      // log_v ("[Thread:%lu][Core:%i] __listener__ {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      ths->__listenerState__ = TcpServer::RUNNING;
      int listenerSocket = -1;
      while (!ths->__instanceUnloading__) { // prepare listener socket - repeat this in a loop in case something goes wrong
        // make listener TCP socket (SOCK_STREAM) for Internet Protocol Family (PF_INET)
        // Protocol Family and Address Family are connected (PF__INET protokol and AF_INET)
        listenerSocket = socket (PF_INET, SOCK_STREAM, 0);
        if (listenerSocket == -1) {
          // log_e ("[Thread:%lu][Core:%i] __listener__: socket () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), errno);
          delay (1000); // try again after 1 s
          continue;
        }
        // make address reusable - so we won't have to wait a few minutes in case server will be restarted
        int flag = 1;
        setsockopt (listenerSocket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
        // bind listener socket to IP address and port
        struct sockaddr_in serverAddress;
        memset (&serverAddress, 0, sizeof (struct sockaddr_in));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = inet_addr (ths->getServerIP ());
        serverAddress.sin_port = htons (ths->getServerPort ());
        if (bind (listenerSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] __listener__: bind () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), listenerSocket, errno);
          goto terminateListener;
          //close (listenerSocket);
          //delay (1000); // try again after 1 s
          //continue;
        }
        // mark socket as listening socket
#define BACKLOG 16 // queue lengthe of (simultaneously) arrived connections - actual active connection number might be larger 
        if (listen (listenerSocket, BACKLOG) == -1) {
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] __listener__: listen () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), listenerSocket, errno);
          goto terminateListener;
          //close (listenerSocket);
          //delay (1000); // try again after 1 s
          //continue;
        }
        // make socket non-blocking
        if (fcntl (listenerSocket, F_SETFL, O_NONBLOCK) == -1) {
          // log_e ("[Thread:%lu][Core:%i][Socket:%i] __listener__: listener socket fcntl () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), listenerSocket, errno);
          goto terminateListener;
          //close (listenerSocket);
          //delay (1000); // try again after 1 s
          //continue;
        }
        // log_i ("[Thread:%lu][Core:%i] __listener__: started accepting connections on %s : %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), ths->getServerIP (), ths->getServerPort ());

        while (!ths->__instanceUnloading__) { // handle incomming connections
          delay (1);
          if (!ths->__threadedMode__ ()) { // checing time-out makes sense only when working as non-threaded TCP server
            if (ths->timeOut ()) goto terminateListener;
          }
          // accept new connection
          ths->__listenerState__ = TcpServer::ACCEPTING_CONNECTIONS;
          int connectionSocket;
          struct sockaddr_in connectingAddress;
          socklen_t connectingAddressSize = sizeof (connectingAddress);
          connectionSocket = accept (listenerSocket, (struct sockaddr *) &connectingAddress, &connectingAddressSize);
          if (connectionSocket != -1) { // non-blocking socket keeps returning -1 until new connection arrives
            // log_i ("[Thread:%lu][Core:%i][Socket:%i] __listener__: new connection from %s\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, (char *) __inet_ntos__ (connectingAddress.sin_addr).c_str ());
            if (!ths->__callFirewallCallback__ ((char *) __inet_ntos__ (connectingAddress.sin_addr).c_str ())) {
              close (connectionSocket);
              // log_e ("[Thread:%lu][Core:%i][Socket:%i] __listener__: %s was rejected by firewall\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, (char *) __inet_ntos__ (connectingAddress.sin_addr).c_str ());
              continue;
            } else {
              // log_i ("[Thread:%lu][Core:%i][Socket:%i] __listener__: firewall let %s through\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, (char *) __inet_ntos__ (connectingAddress.sin_addr).c_str ());
            }
            if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
              // log_e ("[Thread:%lu][Core:%i][Socket:%i] __listener__: connection socket fcntl () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), connectionSocket, errno);
              close (connectionSocket);
              continue;
            }
            ths->__newConnection__ (connectionSocket, (char *) __inet_ntos__ (connectingAddress.sin_addr).c_str ());
            if (!ths->__threadedMode__ ()) goto terminateListener; // in non-threaded mode server only accepts one connection
          } // new connection
        } // handle incomming connections
      } // prepare listener socket
terminateListener:
      ths->__listenerState__ = TcpServer::STOPPED;
      close (listenerSocket);
      // log_v ("[Thread:%lu][Core:%i] } __listener__\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      ths->__listenerState__ = TcpServer::FINISHED;
      vTaskDelete (NULL); // terminate this thread
    }

};


// non-threaded TcpClient initializes a non-threaded TcpConnection instance and passes its reference to the calling program through connection () member function
//   successful instance creation can be tested using connection () member function to not NULL value - result is available immediately after constructor returns
//     if connection () reference exists it means only tht TCP connection has been initialized but it may not be established (yet) at this time or possibly even not at all,
//     failing to connect can only be detected through connection time-out


class TcpClient {

  public:

    TcpClient      (char *serverIP,                               // server IP address - 15 characters at most!
                    int serverPort,                               // server port
                    unsigned long timeOutMillis                   // connection time-out in milli seconds
                   )                          {
      // log_v ("[Thread:%lu][Core:%i] non-threaded constructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      // make TCP socket (SOCK_STREAM) for Internet Protocol Family (PF_INET)
      // Protocol Family and Address Family are connected (PF__INET protokol and AF__INET)
      int connectionSocket = socket (PF_INET, SOCK_STREAM, 0);
      if (connectionSocket == -1) {
        // log_e ("[Thread:%lu][Core:%i] non-threaded constructor: socket () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), errno);
        return;
      }
      // make the socket non-blocking - needed for time-out detection
      if (fcntl (connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
        close (connectionSocket);
        // log_e ("[Thread:%lu][Core:%i] non-threaded constructor: fcntl () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), errno);
        return;
      }
      // connect to server
      struct sockaddr_in serverAddress;
      serverAddress.sin_family = AF_INET;
      serverAddress.sin_port = htons (serverPort);
      serverAddress.sin_addr.s_addr = inet_addr (serverIP);
      if (connect (connectionSocket, (struct sockaddr *) &serverAddress, sizeof (serverAddress)) == -1) {
        // Serial.printf ("errno: %i\n", errno);
#define EINPROGRESS 119
        if (errno != EINPROGRESS) {
          close (connectionSocket);
          // log_e ("[Thread:%lu][Core:%i] non-threaded constructor: connect () error %i\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID (), errno);
          return;
        }
      } // it is likely that socket is not opened yet at this point
      // load non-threaded TcpConnection instance
      __connection__ = new TcpConnection (connectionSocket, serverIP, timeOutMillis);
      if (!__connection__) {
        close (connectionSocket);
        // log_e ("[Thread:%lu][Core:%i] non-threaded constructor: new () error\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
        return;
      }
      // log_v ("[Thread:%lu][Core:%i] } non-threaded constructor\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
    }

    ~TcpClient ()                             {
      // log_v ("[Thread:%lu][Core:%i] destructor {\n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      delete (__connection__);
      // log_v ("[Thread:%lu][Core:%i] } destructor \n", (unsigned long) xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
    }

    TcpConnection *connection ()              {
      return __connection__;  // calling program will handle the connection through this reference - connection is set, before constructor returns but TCP connection may not be established (yet) at this time or possibly even not at all
    }

  private:

    TcpConnection *__connection__ = NULL;                           // TcpConnection instance used by TcpClient instance

};

#endif
