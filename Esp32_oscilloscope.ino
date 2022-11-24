/*

   Esp32_oscilloscope.ino

   This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
   Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
   Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those 
   parts of Esp32_web_ftp_telnet_server_template that are necessary to run Esp32 oscilloscope.

   See https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template for details on how the servers work.

   Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino (with FAT partition scheme) and run on ESP32.
   
   October, 23, 2022, Bojan Jurca

*/

// Compile this code with Arduino for:
// - one of ESP32 boards (Tools | Board) and 
// - one of FAT partition schemes (Tools | Partition scheme) and
// - at least 80 MHz CPU (Tools | CPU Frequency)


#include <WiFi.h>
#include <soc/rtc_wdt.h>  // disable watchdog - it gets occasionally triggered when loaded heavily


// When file system is used a disc will be created and formatted. You can later FTP .html and other files to /var/www/html directory, 
// which is the root directory of a HTTP server.
// Some ESP32 boards do not have a flash disk. In this case just comment the following #include line and Oscilloscope.html will be stored in program memory.
// This is where HTTP server will fetch it from.

#include "file_system.h"


// #define network parameters before #including network.h
#define DEFAULT_STA_SSID                          "YOUR_STA_SSID"
#define DEFAULT_STA_PASSWORD                      "YOUR_STA_PASSWORD"
#define DEFAULT_AP_SSID                           "" // HOSTNAME - leave empty if you don't want to use AP
#define DEFAULT_AP_PASSWORD                       "" // "YOUR_AP_PASSWORD" - at least 8 characters
// ... add other #definitions from network.h
#include "version_of_servers.h"
#define HOSTNAME                                  "Oscilloscope" // choose your own
#define MACHINETYPE                               "ESP32 Dev Modue" // your board

#include "network.h"

// #define what kind of user management you want before #including user_management.h
#define USER_MANAGEMENT NO_USER_MANAGEMENT // UNIX_LIKE_USER_MANAGEMENT // HARDCODED_USER_MANAGEMENT
#include "user_management.h"                      // file_system.h is needed prior to #including user_management.h in case of UNIX_LIKE_USER_MANAGEMENT

#include "httpServer.hpp"                         // file_system.h is needed prior to #including httpServer.h if you want server also to serve .html and other files

#ifdef __FILE_SYSTEM__
  #include "ftpServer.hpp"                        // file_system.h is also necessary to use ftpServer.h
#else
  #include "progMem.h"                            // for ESP32 boards without file system oscilloscope.html is also available in PROGMEM
#endif

// oscilloscope
#include "oscilloscope.h"

String httpRequestHandler (char *httpRequest, httpConnection *hcn);
void wsRequestHandler (char *wsRequest, WebSocket *webSocket);


#include <soc/gpio_sig_map.h>
#include <soc/io_mux_reg.h>


void setup () {  

  // disable watchdog if you can afford it - watchdog gets occasionally triggered when loaded heavily
  // rtc_wdt_protect_off ();
  // rtc_wdt_disable ();
  // disableCore0WDT ();
  // disableCore1WDT ();
  // disableLoopWDT ();

  
  Serial.begin (115200);
  Serial.println (String (MACHINETYPE) + " (" + ESP.getCpuFreqMHz () + " MHz) " + HOSTNAME + " SDK: " + ESP.getSdkVersion () + " " + VERSION_OF_SERVERS + ", compiled at: " + String (__DATE__) + " " + String (__TIME__));


ledcSetup (0, 1000, 10);
ledcAttachPin (22, 0); // built-in LED
ledcWrite (0, 307); // 1/3 duty cycle
// PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG [22]);

  
  #ifdef __FILE_SYSTEM__
  
    // FFat.format (); Serial.printf ("\nFormatting FAT file system ...\n\n"); // format flash disk to reset everithing and start from the scratch
    mountFileSystem (true);                                                 // this is the first thing to do - all configuration files reside on the file system

  #endif

  // initializeUsers ();                                                    // creates user management files with root and webadmin users, if they don't exist yet, not needed for NO_USER_MANAGEMENT 

  // deleteFile ("/network/interfaces");                                    // contation STA(tion) configuration       - deleting this file would cause creating default one
  // deleteFile ("/etc/wpa_supplicant/wpa_supplicant.conf");                // contation STA(tion) credentials         - deleting this file would cause creating default one
  // deleteFile ("/etc/dhcpcd.conf");                                       // contains A(ccess) P(oint) configuration - deleting this file would cause creating default one
  // deleteFile ("/etc/hostapd/hostapd.conf");                              // contains A(ccess) P(oint) credentials   - deleting this file would cause creating default one
  startWiFi ();                                                             // starts WiFi according to configuration files, creates configuration files if they don't exist

  // start web server 
  httpServer *httpSrv = new httpServer (httpRequestHandler,                 // a callback function that will handle HTTP requests that are not handled by webServer itself
                                        wsRequestHandler,                   // a callback function that will handle WS requests, NULL to ignore WS requests
                                        (char *) "0.0.0.0",                 // start HTTP server on all available IP addresses
                                        80,                                 // default HTTP port
                                        NULL);                              // we won't use firewallCallback function for HTTP server
  if (!httpSrv && httpSrv->state () != httpServer::RUNNING) dmesg ("[httpServer] did not start.");

  #ifdef __FILE_SYSTEM__

    // start FTP server
    ftpServer *ftpSrv = new ftpServer ((char *) "0.0.0.0",                    // start FTP server on all available ip addresses
                                       21,                                    // default FTP port
                                       NULL);                                 // we won't use firewallCallback function for FTP server
    if (ftpSrv && ftpSrv->state () != ftpServer::RUNNING) dmesg ("[ftpServer] did not start.");

  #endif
  
  // initialize GPIOs you are going to use here:
  // ...

              // ----- examples - delete this code when if it is not needed -----
              #undef LED_BUILTIN
              #define LED_BUILTIN 2
              Serial.printf ("\nGenerating 1 KHz PWM signal on built-in LED pin %i, just for demonstration purposes.\n"
                             "Please, delete this code when if it is no longer needed.\n\n", LED_BUILTIN);
              ledcSetup (0, 1000, 10);        // channel, freqency, resolution_bits
              ledcAttachPin (LED_BUILTIN, 0); // GPIO, channel
              ledcWrite (0, 307);             // channel, 1/3 duty cycle (307 out of 1024 (10 bit resolution))
                       
}

void loop () {

}


String httpRequestHandler (char *httpRequest, httpConnection *hcn) { 
  // httpServer will add HTTP header to the String that this callback function returns and send everithing to the Web browser (this callback function is suppose to return only the content part of HTTP reply)

  #define httpRequestStartsWith(X) (strstr(httpRequest,X)==httpRequest)

  #ifdef __FILE_SYSTEM__

    // if HTTP request is GET /oscilloscope.html HTTP server will fetch the file but let us redirect GET / and GET /index.html to it as well
    if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ")) {
      hcn->setHttpReplyHeaderField ("Location", "/oscilloscope.html");
      hcn->setHttpReplyStatus ((char *) "307 temporary redirect"); // change reply status to 307 and set Location so client browser will know where to go next
      return "Redirecting ..."; // whatever
    }

  #else

    if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ") || httpRequestStartsWith ("GET /oscilloscope.html ")) {
      return F (oscilloscopeHtml);  
    }

  #endif

  // if the request is GET /oscilloscope.html we don't have to interfere - web server will read the file from file system
  return ""; // HTTP request has not been handled by httpRequestHandler - let the httpServer handle it itself
}

void wsRequestHandler (char *wsRequest, WebSocket *webSocket) {
  
  #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)

  if (wsRequestStartsWith ("GET /runOscilloscope")) runOscilloscope (webSocket); // used by oscilloscope.html
}
