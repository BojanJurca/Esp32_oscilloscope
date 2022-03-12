/*

   Esp32_oscilloscope.ino

   This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
   Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
   Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those 
   parts of Esp32_web_ftp_telnet_server_template that are necessary to run Esp32 oscilloscope.
  
   Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino (with FAT partition scheme) and run on ESP32.
   
   March, 12, 2022, Bojan Jurca

*/


#include <WiFi.h>
// we are not actually using Arduino analogRead but rather adc1_get_raw instead, here are defined ADC channels
#include <driver/adc.h>


#include "dmesg_functions.h"
#include "file_system.h"
  // #define network parameters before #including network.h
  #define HOSTNAME                                  "MyESP32Server"
  #define DEFAULT_STA_SSID                          "YOUR_STA_SSID"
  #define DEFAULT_STA_PASSWORD                      "YOUR_STA_PASSWORD"
  #define DEFAULT_AP_SSID                           "" // HOSTNAME - leave empty if you don't want to use AP
  #define DEFAULT_AP_PASSWORD                       "" // "YOUR_AP_PASSWORD" - at least 8 characters
  // ... add other #definitions from network.h
#include "network.h"                      // file_system.h is needed prior to #including network.h if you want to store the default parameters
  // #define how you want to calculate local time and which NTP servers GMT wil be sinchronized with before #including time_functions.h (FTP server needs this to correctly display file creation fime)
  #define DEFAULT_NTP_SERVER_1                      "1.si.pool.ntp.org"
  #define DEFAULT_NTP_SERVER_2                      "2.si.pool.ntp.org"
  #define DEFAULT_NTP_SERVER_3                      "3.si.pool.ntp.org"
  #define TIMEZONE CET_TIMEZONE                     // or another one supported in time_functions.h
#include "time_functions.h"                         // file_system.h is needed prior to #including time_functions.h if you want to store the default parameters
  // #define what kind of user management you want before #including user_management.h
  #define USER_MANAGEMENT NO_USER_MANAGEMENT // UNIX_LIKE_USER_MANAGEMENT // HARDCODED_USER_MANAGEMENT
#include "user_management.h"                        // file_system.h is needed prior to #including user_management.h in case of UNIX_LIKE_USER_MANAGEMENT
#include "ftpServer.hpp"                            // file_system.h is also necessary to use ftpServer.h
#include "httpServer.hpp"                           // file_system.h is needed prior to #including httpServer.h if you want server also to serve .html and other files

// oscilloscope
#include "oscilloscope.h"

// take a look at https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template for details on how this works


String httpRequestHandler (char *httpRequest, httpConnection *hcn);
void wsRequestHandler (char *wsRequest, WebSocket *webSocket);


              // ----- examples - delete this code when if it is not needed -----
              // byte sinus [256];


void setup () {  
  Serial.begin (115200);
 
  // FFat.format (); // clear up flash disk to reset everithing
  mountFileSystem (true);                                                   // this is the first thing to do - all configuration files reside on the file system

  // deleteFile ("/etc/ntp.conf");                                          // contains ntp server names for time sync - deleting this file would cause creating default one
  // deleteFile ("/etc/crontab");                                           // contains cheduled tasks                 - deleting this file would cause creating empty one
  startCronDaemon (NULL);                                                   // creates /etc/ntp.conf with default NTP server names and syncronize ESP32 time with them once a day
                                                                            // creates empty /etc/crontab, reads it at startup and executes cronHandler when the time is right
                                                                            // 3 KB stack size is minimal requirement for NTP time synchronization, add more if your cronHandler requires more

  // deleteFile ("/etc/passwd");                                            // contains users' accounts information    - deleting this file would cause creating default one
  // deleteFile ("/etc/shadow");                                            // contains users' passwords               - deleting this file would cause creating default one
  initializeUsers ();                                                       // creates user management files with root and webadmin users, if they don't exist yet

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

  // start FTP server
  ftpServer *ftpSrv = new ftpServer ((char *) "0.0.0.0",                    // start FTP server on all available ip addresses
                                     21,                                    // default FTP port
                                     NULL);                                 // we won't use firewallCallback function for FTP server
  if (ftpSrv && ftpSrv->state () != ftpServer::RUNNING) dmesg ("[ftpServer] did not start.");

  
  // initialize GPIOs you are going to use here:
  // ...

              /*
              // ----- examples - delete this code when if it is not needed -----
              // example: PWM on GPIO 16
              ledcSetup (0, 83, 10);                    // channel 0, 83 Hz, 10 bit resolution (1024 values)
              ledcAttachPin (16, 0);                    // attach pin 2 (built-in led) to channel 0
              ledcWrite (0, 307);                       // PWM of built-in LED (channel 0 -> pin 2) with 1/3 of period (307 out of 1023)
              // example: ADC on GPIO 25
              analogReadResolution (12);                // set 12 bit analog read resolution (defaule)
              analogSetPinAttenuation (25, ADC_11db);   // set attenuation for pin 32 to 2600 mV range (default)
              #define BYTE2RAD 2 * PI / 256
              for (int i = 0; i < 256; i++) sinus [i] = sin (i * BYTE2RAD) * 100 + 110; // one period with 256 samples
              // after IDF 4.? we can not read the signal on GPIO2 directly but we can connec it with a wire to another GPIO and read it there
              */
              
}

void loop () {

              // ----- examples - delete this code when if it is not needed -----
              /*
              delayMicroseconds (48);
              static byte b = 0;
              dacWrite (25, sinus [b++]); 
              */
              
}


String httpRequestHandler (char *httpRequest, httpConnection *hcn) { 
  // httpServer will add HTTP header to the String that this callback function returns and send everithing to the Web browser (this callback function is suppose to return only the content part of HTTP reply)

  #define httpRequestStartsWith(X) (strstr(httpRequest,X)==httpRequest)


  if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ")) {
    // redirect browser to /var/www/html/oscilloscope.html
    String host = hcn->getHttpRequestHeaderField ((char *) "Host");
    char location [66]; sprintf (location, "http://%s/oscilloscope.html", (char *) host.c_str ());
    hcn->setHttpReplyHeaderField ("Location", location);
    hcn->setHttpReplyStatus ((char *) "307 temporary redirect"); // change reply status to 307 and set Location so client browser will know where to go next
    return "Redirecting ...";
  // } else if (httpRequestStartsWith ("GET /oscilloscope.html ") {
  //   // don't have to handle this request, httpServer will look for /var/www/html/oscilloscope.html itself
  }
  // if the request is GET /oscilloscope.html we don't have to interfere - web server will read the file from file system
 
  return ""; // HTTP request has not been handled by httpRequestHandler - let the httpServer handle it itself
}

void wsRequestHandler (char *wsRequest, WebSocket *webSocket) {
  
  #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)

  if (wsRequestStartsWith ("GET /runOscilloscope")) runOscilloscope (webSocket); // used by oscilloscope.html
}
