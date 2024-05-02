/*

    Esp32_web_ftp_telnet_server_template.ino

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
 
    File contains a working template for some operating system functionalities that can support your projects.
 
    Copy all files in the package to Esp32_web_ftp_telnet_server_template directory, compile them with Arduino and run on ESP32.

    May 22, 2024, Bojan Jurca

 */

// Compile this code with Arduino for one of ESP32 boards (Tools | Board) and one of FAT partition schemes (Tools | Partition scheme)!

#include <WiFi.h>

#include "servers/std/Cstring.hpp"
#include "servers/std/Map.hpp"
#include "servers/std/console.hpp"

#include "Esp32_servers_config.h"

#include "soc/rtc_wdt.h"

 
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

// ----- telnet command handler example - if you don't want to handle telnet commands yourself just delete this function and pass NULL to telnetSrv instead of its address -----

String telnetCommandHandlerCallback (int argc, char *argv [], telnetConnection *tcn) {

    #define argv0is(X) (argc > 0 && !strcmp (argv[0], X))  
    #define argv1is(X) (argc > 1 && !strcmp (argv[1], X))
    #define argv2is(X) (argc > 2 && !strcmp (argv[2], X)) 


    // get Telnet session private memory to keep track of the session - this memory will be autmatically freed at the end of Telnet connection
    oscSharedMemory *pOscSharedMemory = (oscSharedMemory *) tcn->privateMemory;

    if (tcn->privateMemory == NULL) { // if not allocated yet
        tcn->privateMemory = malloc (sizeof (oscSharedMemory));
        if (tcn->privateMemory == NULL) // if memory allocation failed
            return "Out of memory";

        // fill in the default values
        pOscSharedMemory = (oscSharedMemory *) tcn->privateMemory;
        pOscSharedMemory->analog = true; // ------------------------ Are we going to read only analog values ???
    }


    // process SCPI commands
    if (argv0is ("*IDN?")) {
        if (argc == 1) {
            #ifdef DEFAULT_AP_SSID
                return String ("*IDN" HOSTNAME " " VERSION_OF_SERVERS ", SN: ") + WiFi.softAPmacAddress ().c_str ();
            #else
                return String ("*IDN" HOSTNAME " " VERSION_OF_SERVERS ", SN: ") + WiFi.macAddress ().c_str ();
            #endif
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("SARA")) {
        if (argc == 2) {
            float sara;
            if (sscanf (argv [1], "%fSa/s", sara) == 1 && sara > 0 && sara <= 1000) {

              // sampling time (calculated from sara) sara value would go into    pOscSharedMemory->samplingTime

                return "OK";
            } else {
                return "Parameter out of range";
            }
        } else {
            return "Wrong number of parameters";  
        }
    }

    else if (argv0is ("SARA?")) {
        if (argc == 1) {
            return "733";
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("CHAN")) {
        int gpio1, gpio2;
        switch (argc) {
            case 3:   gpio2 = atoi (argv [2]);
                      if (gpio2 < 0 || gpio2 > 39) {
                          return String ("Wrong channel ") + argv [2];
                      }
                      pOscSharedMemory->gpio2 = gpio2;

            case 2:   gpio1 = atoi (argv [1]);
                      if (gpio1 < 0 || gpio1 > 39) {
                          return String ("Wrong channel ") + argv [1];
                      }
                      pOscSharedMemory->gpio1 = gpio1;
                      break;
            default:  return "Wrong number of parameters";
        }
        return "OK";
    }

    else if (argv0is ("NS")) {
        if (argc == 2) {
            int noOfSamples = atoi (argv [1]);
            if (noOfSamples > 0 && noOfSamples <= 1000) {
                return "OK";
            } else {
                return "Parameter out of range";
            }
        } else {
            return "Wrong number of parameters";  
        }
    }

    else if (argv0is ("TRSE")) {
        if (argc == 5) {
            return "Can this be simplified? As to channel, slope and value?";
        } else {
            return "Wrong number of parameters";  
        }
    }

    else if (argv0is ("ARM")) {
        if (argc == 1) {
            return "1.1 2.2 3.3 and so on until STOP. Which commands MUST be issued before ARM? SARA? CHAN? NS? What happens if they are not?";
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("STOP")) {
        if (argc == 1) {
            return "OK";
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("STAST?")) {
        if (argc == 1) {
            return "Ready";
        } else {
            return "Wrong number of parameters";
        }
    }

    return ""; // let Telnet server try to handle the command itself
}


void setup () {

    // disable watchdog if you can afford it - watchdog gets occasionally triggered when loaded heavily
    #if CONFIG_FREERTOS_UNICORE // CONFIG_FREERTOS_UNICORE == 1 => 1 core ESP32
        disableCore0WDT ();     
    #else // CONFIG_FREERTOS_UNICORE == 0 => 2 core ESP32
        disableCore0WDT ();     
        disableCore1WDT (); 
    #endif


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
    if (!httpSrv || httpSrv->state () != httpServer::RUNNING) 
        cout << "[httpServer] did not start.";

    // start FTP server
    ftpServer *ftpSrv = new ftpServer ();
    if (!ftpSrv || ftpSrv->state () != ftpServer::RUNNING) 
        cout << "[ftpServer] did not start.";
    
    // start telnet server on port 5025 for SCPI communication
    telnetServer *telnetSrv = new telnetServer (telnetCommandHandlerCallback, "0.0.0.0", 5025);
    if (!telnetSrv || telnetSrv->state () != telnetServer::RUNNING) 
        cout << "[telnetServer] did not start.";
    
}

void loop () {

}
