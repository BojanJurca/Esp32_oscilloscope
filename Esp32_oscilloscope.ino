/*

  Esp32_oscilloscope.ino

   This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
   Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
   Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those 
   parts of Esp32_web_ftp_telnet_sfileserver_template that are necessary to run Esp32 oscilloscope.

   See https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template for details on how the servers work.

   Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino (with FAT partition scheme) and run on ESP32.
   
  May 22, 2024, Bojan Jurca

*/

// Compile this code with Arduino for one of ESP32 boards (Tools | Board) and one of FAT partition schemes (Tools | Partition scheme)!

#include <WiFi.h>

#include "servers/std/Cstring.hpp"
//#include "servers/std/Map.hpp"
#include "servers/std/console.hpp"

#include "Esp32_servers_config.h"

 
// ----- HTTP request handler example - if you don't want to handle HTTP requests just delete this function and pass NULL to httpSrv instead of its address -----
//       normally httpRequest is HTTP request, function returns a reply in HTML, json, ... formats or "" if request is unhandeled by httpRequestHandler
//       httpRequestHandler is supposed to be used with smaller replies,
//       if you want to reply with larger pages you may consider FTP-ing .html files onto the file system (into /var/www/html/ directory)


#ifndef FILE_SYSTEM
    #include "oscilloscope_amber.h" // use RAM version
#endif

String httpRequestHandler (char *httpRequest, httpConnection *hcn) { 

    #define httpRequestStartsWith(X) (strstr(httpRequest,X)==httpRequest)


    #ifdef __OSCILLOSCOPE__

        #ifdef __FILE_SYSTEM__

            // if HTTP request is GET /oscilloscope.html HTTP server will fetch the file but let us redirect GET / and GET /index.html to it as well
            if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ")) {
                hcn->setHttpReplyHeaderField ("Location", "/oscilloscope.html");
                hcn->setHttpReplyStatus ((char *) "307 temporary redirect"); // change reply status to 307 and set Location so client browser will know where to go next
                return "Redirecting ..."; // whatever
            }

        #else

            if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ") || httpRequestStartsWith ("GET /oscilloscope.html ")) {
                return F (oscilloscope_amber);  
            }

        #endif

    #endif
                                                                    

    return ""; // httpRequestHandler did not handle the request - tell httpServer to handle it internally by returning "" reply
}


void wsRequestHandler (char *wsRequest, WebSocket *webSocket) {
  
    #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)
    
    #ifdef __OSCILLOSCOPE__
          if (wsRequestStartsWith ("GET /runOscilloscope"))      runOscilloscope (webSocket);      // used by oscilloscope.html
    #endif

}


void setup () {

    cinit ();
  
    #ifdef FILE_SYSTEM
        fileSystem.mountLittleFs (true);                                // this is the first thing to do - all configuration files are on file system
        // fileSystem.formatLittleFs ();        
    #endif

    // deleteFile ("/etc/ntp.conf");                                    // contains ntp server names for time sync - deleting this file would cause creating default one
    // deleteFile ("/etc/crontab");                                     // contains cheduled tasks                 - deleting this file would cause creating empty one
    // startCronDaemon (cronHandler);                                      // creates /etc/ntp.conf with default NTP server names and syncronize ESP32 time with them once a day
                                                                        // creates empty /etc/crontab, reads it at startup and executes cronHandler when the time is right
                                                                        // 3 KB stack size is minimal requirement for NTP time synchronization, add more if your cronHandler requires more

    // deleteFile ("/etc/passwd");                                      // contains users' accounts information    - deleting this file would cause creating default one
    // deleteFile ("/etc/shadow");                                      // contains users' passwords               - deleting this file would cause creating default one
    // userManagement.initialize ();                                       // creates user management files with root, webadmin, webserver and telnetserver users, if they don't exist

    // deleteFile ("/network/interfaces");                              // contation STA(tion) configuration       - deleting this file would cause creating default one
    // deleteFile ("/etc/wpa_supplicant/wpa_supplicant.conf");          // contation STA(tion) credentials         - deleting this file would cause creating default one
    // deleteFile ("/etc/dhcpcd.conf");                                 // contains A(ccess) P(oint) configuration - deleting this file would cause creating default one
    // deleteFile ("/etc/hostapd/hostapd.conf");                        // contains A(ccess) P(oint) credentials   - deleting this file would cause creating default one
    startWiFi ();                                                       // starts WiFi according to configuration files, creates configuration files if they don't exist

    // start web server 
    httpServer *httpSrv = new httpServer (httpRequestHandler,           // a callback function that will handle HTTP requests that are not handled by webServer itself
                                          wsRequestHandler);            // a callback function that will handle WS requests, NULL to ignore WS requests
    if (!httpSrv || httpSrv->state () != httpServer::RUNNING) {
        #ifdef __DMESG__
            dmesgQueue << "[httpServer] did not start";
        #endif
        cout << "[httpServer] did not start";
    }

    #ifdef FILE_SYSTEM
        // start FTP server
        ftpServer *ftpSrv = new ftpServer ();
        if (!ftpSrv || ftpSrv->state () != ftpServer::RUNNING) {
            #ifdef __DMESG__
                dmesgQueue << "[ftpServer] did not start";
            #endif
            cout << "[ftpServer] did not start";
        }
    #endif

}

void loop () {

}
