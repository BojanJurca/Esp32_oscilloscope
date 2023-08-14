/*

   Esp32_oscilloscope.ino

   This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
   Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
   Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those 
   parts of Esp32_web_ftp_telnet_server_template that are necessary to run Esp32 oscilloscope.

   See https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template for details on how the servers work.

   Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino (with FAT partition scheme) and run on ESP32.
   
   August 12, 2023, Bojan Jurca
 
*/

// Compile this code with Arduino for:
// - one of ESP32 boards (Tools | Board) and 
// - one of FAT partition schemes (Tools | Partition scheme) and
// - at least 80 MHz CPU (Tools | CPU Frequency)


// ------------------------------------------ Esp32_web_ftp_telnet_server_template configuration ------------------------------------------
// you can skip some files #included if you don't need the whole functionality


// uncomment the following line to get additional compile-time information about how the project gets compiled
// #define SHOW_COMPILE_TIME_INFORMATION


// include version_of_servers.h to include version information
#include "version_of_servers.h"
// include dmesg_functions.h which is useful for run-time debugging - for dmesg telnet command
// #include "dmesg_functions.h"


// 1. TIME:    #define which time settings, wil be used with time_functions.h - will be included later
    // define which 3 NTP servers will be called to get current GMT (time) from
    // this information will be written into /etc/ntp.conf file if file_system.h will be included
    //    #define DEFAULT_NTP_SERVER_1                      "1.si.pool.ntp.org"   // <- replace with your information
    //    #define DEFAULT_NTP_SERVER_2                      "2.si.pool.ntp.org"   // <- replace with your information
    //    #define DEFAULT_NTP_SERVER_3                      "3.si.pool.ntp.org"   // <- replace with your information
    // define time zone to calculate local time from GMT
    //    #define TZ                                        "CET-1CEST,M3.5.0,M10.5.0/3" // default: Europe/Ljubljana, or select another (POSIX) time zones: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv


// 2. FILE SYSTEM:     #define which file system you want to use 
    // the file system must correspond to Tools | Partition scheme setting: FAT for FAT partition scheme, LittleFS for SPIFFS partition scheme)

    // COMMENT THIS DEFINITION OUT IF YOUR ESP32 DOES NOT HAVE A FLASH DISK
    #define FILE_SYSTEM    FILE_SYSTEM_FAT // FILE_SYSTEM_FAT  // the file system must correspond to Tools | Partition scheme setting: FILE_SYSTEM_FAT (for FAT partition scheme), FILE_SYSTEM_LITTLEFS (for SPIFFS partition scheme) or FILE_SYSTEM_SD_CARD (if SC card is attached)
                                            // FAT file system can be bitwise combined with FILE_SYSTEM_SD_CARD, like #define FILE_SYSTEM (FILE_SYSTEM_FAT | FILE_SYSTEM_SD_CARD)

    // When file system is used a disc will be created and formatted. You can later FTP .html and other files to /var/www/html directory, 
    // which is the root directory of a HTTP server.
    // Some ESP32 boards do not have a flash disk. In this case just comment the #include line above and Oscilloscope.html will be stored in program memory.
    // This is where HTTP server will fetch it from.


// 3. NETWORK:     #define how ESP32 will use the network
    // STA(tion)
    // #define how ESP32 will connecto to WiFi router
    // this information will be written into /etc/wpa_supplicant/wpa_supplicant.conf file if file_system.h will be included
    // if these #definitions are missing STAtion will not be set up
    #define DEFAULT_STA_SSID                          "YOUR_STA_SSID"       // <- replace with your information
    #define DEFAULT_STA_PASSWORD                      "YOUR_STA_PASSWORD"   // <- replace with your information
    // the use of DHCP or static IP address wil be set in /network/interfaces if file_system.h will be included, the following is information needed for static IP configuration
    // if these #definitions are missing DHCP will be assumed
    // #define DEFAULT_STA_IP                            "10.18.1.200"      // <- replace with your information
    // #define DEFAULT_STA_SUBNET_MASK                   "255.255.255.0"    // <- replace with your information
    // #define DEFAULT_STA_GATEWAY                       "10.18.1.1"        // <- replace with your information
    // #define DEFAULT_STA_DNS_1                         "193.189.160.13"   // <- replace with your information
    // #define DEFAULT_STA_DNS_2                         "193.189.160.23"   // <- replace with your information

    // A(ccess) P(oint)
    // #define how ESP32 will set up its access point
    // this information will be writte into /etc/dhcpcd.conf and /etc/hostapd/hostapd.conf files if file_system.h will be included
    // if these #definitions are missing Access Point will not be set up
    // #define DEFAULT_AP_SSID                           HOSTNAME           // <- replace with your information,
    // #define DEFAULT_AP_PASSWORD                       "YOUR_AP_PASSWORD" // <- replace with your information, at least 8 characters
    // #define DEFAULT_AP_IP                             "192.168.0.1"      // <- replace with your information
    // #define DEFAULT_AP_SUBNET_MASK                    "255.255.255.0"    // <- replace with your information

    // define the name Esp32 will use as its host name 
    #define HOSTNAME                                   "MyEsp32Server"      // <- replace with your information,  max 32 bytes
    #define MACHINETYPE                                "ESP32 Dev Module"   // <- replace with your information, machine type, it is only used in uname telnet command


// 4. USERS:     #define what kind of user management you want before #including user_management.h
    #define USER_MANAGEMENT                           NO_USER_MANAGEMENT //  or UNIX_LIKE_USER_MANAGEMENT or HARDCODED_USER_MANAGEMENT
    // if UNIX_LIKE_USER_MANAGEMENT is selected you must also include file_system.h to be able to use /etc/passwd and /etc/shadow files
    // #define DEFAULT_ROOT_PASSWORD                     "rootpassword"        // <- replace with your information if UNIX_LIKE_USER_MANAGEMENT or HARDCODED_USER_MANAGEMENT are used
    // #define DEFAULT_WEBADMIN_PASSWORD                 "webadminpassword"    // <- replace with your information if UNIX_LIKE_USER_MANAGEMENT is used
    // #define DEFAULT_USER_PASSWORD                     "changeimmediatelly"  // <- replace with your information if UNIX_LIKE_USER_MANAGEMENT is used


// #include (or comment-out) the functionalities you want (or don't want) to use
#ifdef FILE_SYSTEM
    #include "fileSystem.hpp"
#endif
// #include "time_functions.h"               // file_system.h is needed prior to #including time_functions.h if you want to store the default parameters
#include "network.h"                      // file_system.h is needed prior to #including network.h if you want to store the default parameters
// #include "httpClient.h"
// #include "ftpClient.h"                    // file_system.h is needed prior to #including ftpClient.h if you want to store the default parameters
// #include "smtpClient.h"                   // file_system.h is needed prior to #including smtpClient.h if you want to store the default parameters
#include "userManagement.hpp"             // file_system.h is needed prior to #including userManagement.hpp in case of UNIX_LIKE_USER_MANAGEMENT
// #include "telnetServer.hpp"               // needs almost all the above files for whole functionality, but can also work without them
#ifdef FILE_SYSTEM
    #include "ftpServer.hpp"                  // file_system.h is also necessary to use ftpServer.h
#endif
#include "httpServer.hpp"                 // file_system.h is needed prior to #including httpServer.h if you want server also to serve .html and other files

// ------------------------------------------------------ end of configuration ------------------------------------------------------------


#include <WiFi.h>
#include <soc/rtc_wdt.h>  // disable watchdog - it gets occasionally triggered when loaded heavily


// oscilloscope
#include "oscilloscope.h"

String httpRequestHandler (char *httpRequest, httpConnection *hcn);
void wsRequestHandler (char *wsRequest, WebSocket *webSocket);


#include <soc/gpio_sig_map.h>
#include <soc/io_mux_reg.h>


void setup () {  

  // disable watchdog if you can afford it - watchdog gets occasionally triggered when loaded heavily
  rtc_wdt_protect_off ();
  rtc_wdt_disable ();
  disableCore0WDT ();
  disableCore1WDT ();
  disableLoopWDT ();

  
  Serial.begin (115200);
  Serial.println (string (MACHINETYPE " (") + string ((int) ESP.getCpuFreqMHz ()) + (char *) " MHz) " HOSTNAME " SDK: " + ESP.getSdkVersion () + (char *) " " VERSION_OF_SERVERS " compiled at: " __DATE__ " " __TIME__); 

  #ifdef __FILE_SYSTEM__
      #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
          // fileSystem.formatLittleFs (); Serial.printf ("\nFormatting FAT file system ...\n\n"); // format flash disk to reset everithing and start from the scratch
          fileSystem.mountLittleFs (true);                                      // this is the first thing to do - all configuration files reside on the file system
      #endif
      #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
          // fileSystem.formatFAT (); Serial.printf ("\nFormatting FAT file system ...\n\n"); // format flash disk to reset everithing and start from the scratch
          fileSystem.mountFAT (true);                                                 // this is the first thing to do - all configuration files reside on the file system
      #endif  
  #endif
  
  // userManagement.initialize ();                                          // creates user management files with root and webadmin users, if they don't exist yet, not needed for NO_USER_MANAGEMENT 

  // fileSystem.deleteFile ("/network/interfaces");                         // contation STA(tion) configuration       - deleting this file would cause creating default one
  // fileSystem.deleteFile ("/etc/wpa_supplicant/wpa_supplicant.conf");     // contation STA(tion) credentials         - deleting this file would cause creating default one
  // fileSystem.deleteFile ("/etc/dhcpcd.conf");                            // contains A(ccess) P(oint) configuration - deleting this file would cause creating default one
  // fileSystem.deleteFile ("/etc/hostapd/hostapd.conf");                   // contains A(ccess) P(oint) credentials   - deleting this file would cause creating default one
  startWiFi ();                                                             // starts WiFi according to configuration files, creates configuration files if they don't exist

  // start web server 
  httpServer *httpSrv = new httpServer (httpRequestHandler,                 // a callback function that will handle HTTP requests that are not handled by webServer itself
                                        wsRequestHandler,                   // a callback function that will handle WS requests, NULL to ignore WS requests
                                        (char *) "0.0.0.0",                 // start HTTP server on all available IP addresses
                                        80,                                 // default HTTP port
                                        NULL);                              // we won't use firewallCallback function for HTTP server
  if (!httpSrv && httpSrv->state () != httpServer::RUNNING) Serial.printf ("[httpServer] did not start.\n");

  #ifdef __FILE_SYSTEM__

    // start FTP server
    ftpServer *ftpSrv = new ftpServer ((char *) "0.0.0.0",                    // start FTP server on all available ip addresses
                                       21,                                    // default FTP port
                                       NULL);                                 // we won't use firewallCallback function for FTP server
    if (ftpSrv && ftpSrv->state () != ftpServer::RUNNING) Serial.printf ("[ftpServer] did not start.\n");

  #endif
  
  // initialize GPIOs you are going to use here:
  // ...

              /* test - generate the signal that can be viewed with oscilloscope
              #undef BUILTIN_LED
              #define BUILTIN_LED 2
              Serial.printf ("\nGenerating 1 KHz PWM signal on built-in LED pin %i, just for demonstration purposes.\n"
                              "Please, delete this code when if it is no longer needed.\n\n", BUILTIN_LED);
              ledcSetup (0, 1000, 10);        // channel, freqency, resolution_bits
              ledcAttachPin (BUILTIN_LED, 0); // GPIO, channel
              ledcWrite (0, 307);             // channel, 1/3 duty cycle (307 out of 1024 (10 bit resolution))
              */
                       
}

void loop () {

}


#include "oscilloscope_amber.h"

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
      return F (oscilloscope_amber);  
    }

  #endif

  // if the request is GET /oscilloscope.html we don't have to interfere - web server will read the file from file system
  return ""; // HTTP request has not been handled by httpRequestHandler - let the httpServer handle it itself
}

void wsRequestHandler (char *wsRequest, WebSocket *webSocket) {
  
  #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)

  if (wsRequestStartsWith ("GET /runOscilloscope")) runOscilloscope (webSocket); // used by oscilloscope.html
}
