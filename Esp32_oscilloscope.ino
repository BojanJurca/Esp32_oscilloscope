/*
 *
 * Esp32_oscilloscope.ino
 *
 *  This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
 *  Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *  
 *  Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those 
 *  parts of Esp32_web_ftp_telnet_server_template that are necessary to run Esp32 oscilloscope.
 *  
 *  Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino (with FAT partition scheme) and run on ESP32.
 *   
 * History:
 *          - first release as a stand-alone project, 
 *            August 24, 2019, Bojan Jurca
 *          - updatet to use latest version of server files,
 *            some oscilloscope improvements
 *            February 22, 2021, Bojan Jurca
 *  
 */

#include <WiFi.h>

// we'll need FAT file system for storing uploaded oscilloscope.html  
#include "file_system.h"

// network settings
#define HOSTNAME                                      "Oscilloscope"      // define the name of your ESP32 here
#define MACHINETYPE                                   "ESP32 NodeMCU"     // describe your hardware here
#define DEFAULT_STA_SSID          "YOUR-STA-SSID" // define default WiFi settings
#define DEFAULT_STA_PASSWORD      "YOUR-STA-PASSWORD"
#define DEFAULT_AP_SSID                               "" // HOSTNAME 
#define DEFAULT_AP_PASSWORD                           "" // "YOUR_AP_PASSWORD" // must be at leas 8 characters long
#include "network.h"

// simplify the use of FTP server in this project by avoiding user management - anyone is alowed to use it
#define USER_MANAGEMENT NO_USER_MANAGEMENT
#include "user_management.h"

// include FTP server - we'll need it for uploading oscilloscope.html to ESP32
#include "ftpServer.hpp"
ftpServer *ftpSrv;

// include Web server - we'll need it to handle oscilloscope WS requests
#include "webServer.hpp"
httpServer *webSrv;

// oscilloscope
#include "oscilloscope.h"

// take a look at https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template for details on how this works

String httpRequestHandler (String& httpRequest, httpServer::wwwSessionParameters *wsp) {  
  // handle HTTP requests
  // note that webServer handles HTTP requests to .html files by itself (in our case oscilloscope.html if uploaded into /var/www/html/ with FTP previously)
  // so you don't have to handle this kind of requests yourself
  
  // example: 
  if (httpRequest.substring (0, 12) == "GET /upTime ") { return "{\"id\":\"esp32\",\"upTime\":\"" + String (millis () / 1000) + " sec\"}"; } // return up-time in JSON format

  return ""; // HTTP request has not been handled by httpRequestHandler - let the webServer handle it itself
}

void wsRequestHandler (String& wsRequest, WebSocket *webSocket) {  
  // handle WS requests. GET /runOscilloscope is the only WS request we'll get from oscilloscope.html, 
  // runOscilloscope function will handle it on the server side - it will communicate with Javascript client in oscilloscope.html

  if (wsRequest.substring (0, 21) == "GET /runOscilloscope " ) { runOscilloscope (webSocket); } // called from oscilloscope.html
}

// setup (), loop () 

void setup () {  
  Serial.begin (115200);

  // mount file system, WEB server will search for files in /var/www/html directory
  // FFat.format (); // uncomment this if something goes wrong to delete all configuration files already written
  mountFileSystem (true);
  if (!isDirectory ("/var/www/html")) { FFat.mkdir ("/var"); FFat.mkdir ("/var/www"); FFat.mkdir ("/var/www/html"); } // so that you won't have to make them manualy yourself

  // connect ESP STAtion to WiFi
  startNetworkAndInitializeItAtFirstCall ();
 
  // start FTP server so we can upload .html and .png files into /var/www/html directory
  ftpSrv = new ftpServer ((char *) "0.0.0.0", 21, NULL);
  if (ftpSrv) // did ESP create FTP server instance?
    if (ftpSrv->started ()) Serial.printf ("[%10lu] FTP server has started.\n", millis ());
    else                    Serial.printf ("[%10lu] FTP server did not start. Compile it in Verbose Debug level to find the error.\n", millis ());
  else                      Serial.printf ("[%10lu] not enough free memory to start FTP server\n", millis ()); // shouldn't really happen

  // start WEB server 
  webSrv = new httpServer (httpRequestHandler, wsRequestHandler, 8192, (char *) "0.0.0.0", 80, NULL);
  if (webSrv) // did ESP create HttpServer instance?
    if (webSrv->started ()) Serial.printf ("[%10lu] WEB server has started.\n", millis ());
    else                    Serial.printf ("[%10lu] WEB server did not start. Compile it in Verbose Debug level to find the error.\n", millis ());
  else                      Serial.printf ("[%10lu] not enough free memory to start WEB server\n", millis ()); // shouldn't really happen

  // initialize GPIOs you are going to use here:
  // ...

              // ----- examples - delete this code when if it is not needed -----
              // example: PWM on built-in led (GPIO 2)
              ledcSetup (0, 83, 10);                    // channel 0, 83 Hz, 10 bit resolution (1024 values)
              ledcAttachPin (2, 0);                     // attach pin 2 (built-in led) to channel 0
              ledcWrite (0, 307);                       // PWM of built-in LED (channel 0 -> pin 2) with 1/3 of period (307 out of 1023)
              // example: ADC on GPIO 32
              analogReadResolution (12);                // set 12 bit analog read resolution (defaule)
              analogSetPinAttenuation (32, ADC_11db);   // set attenuation for pin 32 to 2600 mV range (default)

}

void loop () {

}
