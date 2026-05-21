/*

    Esp32_oscilloscope.ino

    This file is part of the Esp32_oscilloscope project:
    https://github.com/BojanJurca/Esp32_oscilloscope

    The ESP32 oscilloscope is also fully included in the
    Esp32_web_ftp_telnet_server_template project:
    https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    The oscilloscope is built on top of the Esp32_web_ftp_telnet_server_template.
    When used as a stand‑alone project, it includes only the components required
    for running the oscilloscope functionality.

    For details on how the servers operate, see:
    https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template


    The oscilloscope uses the ESP32_Multitasking_Network_Suite Arduino library:
    https://github.com/BojanJurca/Multitasking-Http-Ftp-Telnet-Ntp-Smtp-Servers-and-clients-for-ESP32-Arduino-Library

    and its dependencies:
        - ThreadSafeFS
          https://github.com/BojanJurca/Thread-safe-file-sytem-wrapper-Arduino-library-for-ESP32
        - LightweightSTL
          https://github.com/BojanJurca/Lightweight-Standard-Template-Library-STL-for-Arduino

    The library can be installed through the Arduino IDE or directly from GitHub.
    If you intend to use the HTTPS server instead of the HTTP server,
    you will also need the WolfSSL library.


    May 22, 2026, Bojan Jurca

*/


#define USE_FILE_SYSTEM // comment-out this definition if you don't have a file system, in this case oscilloscope.html page will be served from memory  


#include <WiFi.h>


// Configure the servers you want to use in server_config.h before including their header files.
#include "server_config.h" // first inclusion - configuration part


#include <ostream.hpp>          // convenience wrapper for using cout instead of Serial.print
#include <Cstring.hpp>          // C-style strings with C++ operators that reside on the stack (no heap allocation)


#ifdef USE_FILE_SYSTEM
    // Arduino library: ThreadSafeFS
    // https://github.com/BojanJurca/Thread-safe-file-sytem-wrapper-Arduino-library-for-ESP32
    #include <LittleFS.h>               // or SPIFFS or FFat, ...
    #include <threadSafeFS.h>           // thread-safe wrapper file system wrapper
    threadSafeFS::FS TSFS (LittleFS);   // use thread-safe wrapper arround selected LittleFS
    using File = threadSafeFS::File;    // use thread-safe wrapper for all file operations in your code from now on
#else
    #include "amber_oscilloscope_html.h"     // use RAM version
#endif


// Create the default configuration files and read from them
#include "server_config.h" // second inclusion - implementation part


#include <httpServer.h>
    httpServer_t *httpServer = NULL;

// Include oscilloscope
// #define USE_I2S_INTERFACE             // I2S interface improves web based oscilloscope analog sampling (of a single signal) if ESP32 board has one
// check INVERT_ADC1_GET_RAW and INVERT_I2S_READ #definitions in oscilloscope.h if the signals are inverted
#include "oscilloscope.h"


#ifdef USE_FILE_SYSTEM
    #include <ftpServer.h>
        ftpServer_t *ftpServer = NULL;
#endif


#ifdef USE_mDNS
    #include <ESPmDNS.h>
#endif


// ----- handle HTTP requests -----

String httpRequestHandlerCallback (const char *httpRequest, httpServer_t::httpConnection_t *hcn) { 

    // Must be reentrant !!!


    #define httpRequestIs(X) (strstr(httpRequest,X)==httpRequest)

    #ifdef USE_FILE_SYSTEM
        // if HTTP request is GET /oscilloscope.html HTTP server will fetch the file but let us redirect GET / and GET /index.html to it as well
        if (httpRequestIs ("GET / ") || httpRequestIs ("GET /index.html ")) {
            hcn->setHttpReplyHeaderField ("Location", "/oscilloscope.html");
            hcn->setHttpReplyStatus ("307 temporary redirect"); // change reply status to 307 and set Location so client browser will know where to go next
            return "Redirecting ..."; // whatever
        }
    #else
        if (httpRequestIs ("GET / ") || httpRequestIs ("GET /index.html ") || httpRequestIs ("GET /oscilloscope.html ")) {
            return amber_oscilloscope_html;
        }
    #endif

    return ""; // httpRequestHandler did not handle the request - tell httpServer to handle it internally by returning "" reply
}


void wsRequestHandlerCallback (const char *httpRequest, httpServer_t::webSocket_t *webSocket) {

    // Must be reentrant !!!
    

    #define httpRequestIs(X) (strstr(httpRequest,X)==httpRequest)
    
    if (httpRequestIs ("GET /runOscilloscope"))      
        runOscilloscope (webSocket);
}


void setup () {

    // Initialize console output.
    // Optional arguments: waitForSerial=false, waitAfterSerial=100 ms, serialSpeed=115200.
    cinit ();

    #ifdef USE_FILE_SYSTEM
        // If a file system is used, mount it now.
        LittleFS.begin (true);
        // LittleFS.format (); // start from scratch
    #endif

    // Connect to the WiFi router.
    WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            cout << "[WiFi][STA] " "connected";
            #ifdef POWER_SAVING
                esp_err_t err = esp_wifi_set_ps (POWER_SAVING);
                if (err == ESP_OK)
                    cout << "[power saving] " "is on";
                else
                    cout << "[power saving] " "couldn't set power saving";
            #endif
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            cout << "[WiFi][STA] " "got IP address: " << WiFi.localIP ();

            #ifdef USE_mDNS
                if (MDNS.begin (HOSTNAME))
                    cout << "[mDNS] started for " << HOSTNAME;
                else
                    cout << "[mDNS] did not start";
                #ifdef USE_FILE_SYSTEM
                    MDNS.addService ("ftp", "tcp", 21);            
                #endif
                MDNS.addService ("http", "tcp", 80);
            #endif            
        }
    });
    // read WiFi settings from configuration files which are stored on TSFS
    #ifdef USE_FILE_SYSTEM
        WiFi_start (TSFS); // if not file system is used skip TSFS argument
    #else
        WiFi_start ();
    #endif
    // if no file system is used call WiFi_start () with no arguments which uses DEFAULT_ ... definitions instead,
    // or simply call WiFi.begin ("YOUR STA SSID", "YOUR STA PASSWORD");


    // Start the HTTP server. To save ~3 KB of RAM, the listener can run inside the
    // setup/loop task instead of its own task.
    // In this mode you must call httpServer->accept() manually from loop().
    #ifdef USE_FILE_SYSTEM
        httpServer = new (std::nothrow) httpServer_t (TSFS,                         // threadSafeFS::FS& fileSystem,
                                                      httpRequestHandlerCallback,   // String httpRequestHandlerCallback (const char *httpRequest, httpServer_t::httpConnection_t *hcn) = NULL,
                                                      wsRequestHandlerCallback);    // void (*wsRequestHandlerCallback) (const char *httpRequest, httpServer_t::webSocket_t *webSck) = NULL,
                                                                                    // int serverPort = 80,
                                                                                    // bool (*firewallCallback) (char *clientIP, char *serverIP) = NULL,
                                                                                    // bool runListenerInItsOwnTask = true
    #else
        httpServer = new (std::nothrow) httpServer_t (httpRequestHandlerCallback,   // String httpRequestHandlerCallback (const char *httpRequest, httpServer_t::httpConnection_t *hcn) = NULL,
                                                      wsRequestHandlerCallback);    // void (*wsRequestHandlerCallback) (const char *httpRequest, httpServer_t::webSocket_t *webSck) = NULL,
                                                                                    // int serverPort = 80,
                                                                                    // bool (*firewallCallback) (char *clientIP, char *serverIP) = NULL,
                                                                                    // bool runListenerInItsOwnTask = true
    #endif

    if (httpServer && *httpServer)
        cout << "[httpServer] " "started";
    else
        cout << "[httpServer] " "did not start";


    #ifdef USE_FILE_SYSTEM
        // Start the FTP server. To save ~3 KB of RAM, the listener can run inside the
        // setup/loop task instead of its own task.
        // In this mode you must call ftpServer->accept() manually from loop().
        ftpServer = new (std::nothrow) ftpServer_t (TSFS);  // threadSafeFS::FS& fileSystem,
                                                            // Optional arguments:
                                                            // Cstring<255> (*getUserHomeDirectory) (const Cstring<64>& userName, const Cstring<64>& password) = NULL
                                                            // int serverPort = 21
                                                            // bool (*firewallCallback) (char *clientIP, char *serverIP) = NULL
                                                            // bool runListenerInItsOwnTask = true

        if (ftpServer && *ftpServer)
            cout << "[ftpServer] " "started";
        else
            cout << "[ftpServer] " "did not start";
    #endif
}

void loop () {
    #ifdef USE_FILE_SYSTEM
        // ftpServer has its own listening task, so accept() is not called here.
        // if (ftpServer) ftpServer->accept ();
    #endif
    // httpServer has its own listening task, so accept() is not called here.
    // if (httpServer) httpServer->accept ();


}