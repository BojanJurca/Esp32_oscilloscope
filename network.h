/*

    network.h

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    network.h reads network configuration from file system and sets both ESP32 network interfaces accordingly
 
    It is a little awkward why Unix, Linux are using so many network configuration files and how they are used):

      /network/interfaces                       - modify the code below with your IP addresses
      /etc/wpa_supplicant/wpa_supplicant.conf   - modify the code below with your WiFi SSID and password
      /etc/dhcpcd.conf                          - modify the code below with your access point IP addresses
      /etc/hostapd/hostapd.conf                 - modify the code below with your access point SSID and password

    June 25, 2023, Bojan Jurca

*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <lwip/netif.h>
    #include <netif/etharp.h>
    #include <esp_wifi.h>

    // sockets and error codes
    #include <lwip/sockets.h>
    #define EAGAIN 11
    #define ENAVAIL 119
    #define EINPROGRESS 119
    // gethostbyname
    #include <lwip/netdb.h>
    // fixed size strings
    #include "fsString.h"


#ifndef __NETWORK__
    #define __NETWORK__

    #ifndef __FILE_SYSTEM__
      #ifdef SHOW_COMPILE_TIME_INFORMATION
          #pragma message "Compiling network.h without file system (file_system.h), network.h will not use configuration files"
      #endif
    #endif


    // ----- functions and variables in this modul -----

    void startWiFi ();

    string arp (int);
    string iw (int);
    string ifconfig (int);
    uint32_t ping (char *, int, int, int, int, int);


    // ----- TUNNING PARAMETERS -----

    #ifndef HOSTNAME
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "HOSTNAME was not defined previously, #defining the default MyESP32Server in network.h"
        #endif
      #define HOSTNAME                  "MyESP32Server"  // use default if not defined previously, max 32 bytes  ESP_NETIF_HOSTNAME_MAX_SIZE
    #endif
    // define default STAtion mode parameters to be written to /etc/wpa_supplicant/wpa_supplicant.conf if you want to use ESP as WiFi station
    #ifndef DEFAULT_STA_SSID
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_SSID was not defined previously, #defining the default YOUR_STA_SSID which most likely will not work"
        #endif
      #define DEFAULT_STA_SSID          "YOUR_STA_SSID"
    #endif
    #ifndef DEFAULT_STA_PASSWORD
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_PASSWORD was not defined previously, #defining the default YOUR_STA_PASSWORD which most likely will not work"
        #endif
      #define DEFAULT_STA_PASSWORD      "YOUR_STA_PASSWORD"
    #endif
    // define default static IP, subnet mask and gateway to be written to /network/interfaces if you want ESP to connect to WiFi with static IP instead of using DHCP
    #ifndef DEFAULT_STA_IP
      #ifdef SHOW_COMPILE_TIME_INFORMATION
          #pragma message "DEFAULT_STA_IP was left undefined, DEFAULT_STA_IP is only needed when static IP address is used"
      #endif
      // #define DEFAULT_STA_IP            "10.18.1.200"       // IP that ESP32 is going to use if static IP is configured
    #endif
    #ifndef DEFAULT_STA_SUBNET_MASK
      #ifdef SHOW_COMPILE_TIME_INFORMATION
          #pragma message "DEFAULT_STA_SUBNET_MASK was left undefined, DEFAULT_STA_SUBNET_MASK is only needed when static IP address is used"
      #endif
      // #define DEFAULT_STA_SUBNET_MASK   "255.255.255.0"
    #endif
    #ifndef DEFAULT_STA_GATEWAY
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_GATEWAY was left undefined, DEFAULT_STA_GATEWAY is only needed when static IP address is used"
        #endif
      // #define DEFAULT_STA_GATEWAY       "10.18.1.1"       // your router's IP
    #endif
    #ifndef DEFAULT_STA_DNS_1
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_DNS_1 was left undefined, DEFAULT_STA_DNS_1 is only needed when static IP address is used"
        #endif
      // #define DEFAULT_STA_DNS_1         "193.189.160.13"    // or whatever your internet provider's DNS is
    #endif
    #ifndef DEFAULT_STA_DNS_2
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_DNS_2 was left undefined, DEFAULT_STA_DNS_2 is only needed when static IP address is used"
        #endif
        // #define DEFAULT_STA_DNS_2         "193.189.160.23"    // or whatever your internet provider's DNS is
    #endif  
    // define default Access Point parameters to be written to /etc/hostapd/hostapd.conf if you want ESP to serve as an access point  
    #ifndef DEFAULT_AP_SSID
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_AP_SSID was left undefined, DEFAULT_AP_SSID is only needed when A(ccess) P(point) is used"
        #endif
        #define DEFAULT_AP_SSID           "" // HOSTNAME            // change to what you want to name your AP SSID by default or "" if AP is not going to be used
    #endif
    #ifndef DEFAULT_AP_PASSWORD
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_AP_PASSWORD was not defined previously, #defining the default YOUR_AP_PASSWORD in case A(ccess) P(point) is used"
        #endif
        #define DEFAULT_AP_PASSWORD       "YOUR_AP_PASSWORD"  // at leas 8 characters if AP is used
    #endif
    // define default access point IP and subnet mask to be written to /etc/dhcpcd.conf if you want to define them yourself
    #ifndef DEFAULT_AP_IP
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_AP_IP was not defined previously, #defining the default 192.168.0.1 in case A(ccess) P(point) is used"
        #endif
        #define DEFAULT_AP_IP               "192.168.0.1"
    #endif
    #ifndef DEFAULT_AP_SUBNET_MASK
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_AP_SUBNET_MASK was not defined previously, #defining the default 255.255.255.0 in case A(ccess) P(point) is used"
        #endif
        #define DEFAULT_AP_SUBNET_MASK      "255.255.255.0"
    #endif

    #define MAX_INTERFACES_SIZE         512 // how much memory is needed to temporary store /network/interfaces
    #define MAX_WPA_SUPPLICANT_SIZE     512 // how much memory is needed to temporary store /etc/wpa_supplicant/wpa_supplicant.conf
    #define MAX_DHCPCD_SIZE             512 // how much memory is needed to temporary store /etc/dhcpcd.conf
    #define MAX_HOSTAPD_SIZE            512 // how much memory is needed to temporary store /etc/hostapd/hostapd.conf
 
    // ----- CODE -----

      // log network Traffic information for each socket
      struct networkTrafficInformationType {
          // statistics
          unsigned long bytesReceived;
          unsigned long  bytesSent;
          // debug information 
          unsigned long startMillis; // a time when socket was opened
          unsigned long lastActiveMillis; // a time when data was last sent or received
          // ... add more if needed
      };
      networkTrafficInformationType networkTrafficInformation = {}; // measure network Traffic on ESP32 level
      networkTrafficInformationType socketTrafficInformation [MEMP_NUM_NETCONN] = {}; // there are MEMP_NUM_NETCONN available sockets measure network Traffic on each socket level

      // missing hstrerror function
      #include <lwip/netdb.h>
      char *hstrerror (int h_errno) {
          switch (h_errno) {
              case EAI_NONAME:      return (char *) "Name or service is unknown";
              case EAI_SERVICE:     return (char *) "Service not supported for 'ai_socktype'";
              case EAI_FAIL:        return (char *) "Non-recoverable failure in name res";
              case EAI_MEMORY:      return (char *) "Memory allocation failure";
              case EAI_FAMILY:      return (char *) "'ai_family' not supported";
              case HOST_NOT_FOUND:  return (char *) "The specified host is unknown";
              case NO_DATA:         return (char *) "The requested name is valid but does not have an IP address";
              case NO_RECOVERY:     return (char *) "The requested name is valid but does not have an IP address";
              case TRY_AGAIN:       return (char *) "A temporary error occurred on an authoritative name server. Try again later";
              default:              return (char *) "Invalid h_errno code";
          }
      }

      // returns len which is the number of bytes actually sent or 0 indicatig an error, buf is send in separate blocks of size TCP_SND_BUF (the maximum size that ESP32 can send)
      int sendAll (int sockfd, char *buf, size_t len, unsigned long timeOut) {
        size_t sentTotal = 0;
        size_t sentThisTime;
        size_t toSendThisTime;
        unsigned long lastActive = millis ();
        while (true) {
          toSendThisTime = len - sentTotal; if (toSendThisTime > TCP_SND_BUF) toSendThisTime = TCP_SND_BUF; // 5744
          switch ((sentThisTime = send (sockfd, buf + sentTotal, toSendThisTime, 0))) {
            case -1:
                if ((errno == EAGAIN || errno == ENAVAIL) && (millis () - lastActive < timeOut || !timeOut)) break; // not time-out yet
                else return - 1;
            case 0:   // connection closed by peer
                return 0;
            default:

              networkTrafficInformation.bytesSent += sentThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              socketTrafficInformation [sockfd - LWIP_SOCKET_OFFSET].bytesSent += sentThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              socketTrafficInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
              // DEBUG: Serial.printf ("[%10lu] [network] socket %i sent %i bytes\n", millis (), sockfd, sentThisTime);

              sentTotal += sentThisTime;
              if (sentTotal == len) return sentTotal;
              lastActive = millis ();
              break;
          }
          delay (1); // reset the watchdog
        }
        return 0; // never executes
      }

      int sendAll (int sockfd, char *buf, unsigned long timeOut) { return sendAll (sockfd, buf, strlen (buf), timeOut); }

      int recvAll (int sockfd, char *buf, size_t len, char *endingString, unsigned long timeOut) {
        int receivedTotal = 0;
        int receivedThisTime;    
        unsigned long lastActive = millis ();
        while (true) { // read blocks of incoming data
          switch (receivedThisTime = recv (sockfd, buf + receivedTotal, len - 1 - receivedTotal, 0)) {
            case -1:
                if ((errno == EAGAIN || errno == ENAVAIL) && (millis () - lastActive < timeOut || !timeOut)) break; // not time-out yet 
                else return -1;
            case 0: // connection closed by peer
                return 0;
            default:

              networkTrafficInformation.bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              socketTrafficInformation [sockfd - LWIP_SOCKET_OFFSET].bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              socketTrafficInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
              // DEBUG: Serial.printf ("[%10lu] [network] socket %i received %i bytes\n", millis (), sockfd, receivedThisTime);

              receivedTotal += receivedThisTime;
              buf [receivedTotal] = 0; // close C string
              if (endingString) {
                  if (strstr (buf, endingString)) return receivedTotal; // check if whole request or reply has arrived (if there is an ending string in it)
                  if (receivedTotal >= len - 1) { // the endingStirng has not arrived yet and there is no place left in the buffer for it
                      #ifdef __DMESG__
                          dmesg ("[network][recvAll] buffer too small");
                      #endif
                      Serial.printf ("[%10lu] [network][recvAll] buffer too small\r\n", millis ());
                      return 0;
                      // return receivedTotal; // let the calling routine check the ending string again
                  }                
              } else {
                  return receivedTotal; // if there is no ending string specified, each block we receive should be ok 
              }
              lastActive = millis ();
              break;
          }  
          delay (1); // reset the watchdog
        }
        return 0; // never executes
      }

    // converts dotted (text) IP address to IPAddress structure
    IPAddress __IPAddressFromString__ (char *ipAddress) { 
      int ip1, ip2, ip3, ip4; 
      if (4 == sscanf (ipAddress, "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
        return IPAddress (ip1, ip2, ip3, ip4);
      } else {
        Serial.printf ("[network] invalid IP address %s\n", ipAddress);
        return IPAddress (0, 42, 42, 42); // == 1073421048 - invalid address - first byte of class A can not be 0
      }
    }

    // converts binary MAC address into String
    string __mac_ntos__ (byte *mac, byte macLength) {
      string s;
      char c [3];
      for (byte i = 0; i < macLength; i++) {
        sprintf (c, "%02x", *(mac ++));
        s += c;
        if (i < 5) s += ":";
      }
      return s;
    }
    
    void startWiFi () {                  // starts WiFi according to configuration files, creates configuration files if they don't exist
      // WiFi.disconnect (true);
      WiFi.mode (WIFI_OFF);
  
      // these parameters are needed to start ESP32 WiFi in different modes
      #ifdef DEFAULT_STA_SSID
        char staSSID [33] = DEFAULT_STA_SSID;
      #else
        char staSSID [33] = "";
      #endif
      #ifdef DEFAULT_STA_PASSWORD
        char staPassword [64] = DEFAULT_STA_PASSWORD;
      #else
        char staPassword [64] = "";
      #endif
      #ifdef DEFAULT_STA_IP
        char staIP [46] = DEFAULT_STA_IP;
      #else
        char staIP [46] = "";
      #endif
      #ifdef DEFAULT_STA_SUBNET_MASK
        char staSubnetMask [46] = DEFAULT_STA_SUBNET_MASK;
      #else
        char staSubnetMask [46] = "";
      #endif
      #ifdef DEFAULT_STA_GATEWAY
        char staGateway [46] = DEFAULT_STA_GATEWAY;
      #else
        char staGateway [46] = "";
      #endif
      #ifdef DEFAULT_STA_DNS_1
        char staDns1 [46] = DEFAULT_STA_DNS_1;
      #else
        char staDns1 [46] = "";
      #endif
      #ifdef DEFAULT_STA_DNS_2
        char staDns2 [46] = DEFAULT_STA_DNS_2;
      #else
        char staDns2 [46] = "";
      #endif
      #ifdef DEFAULT_AP_SSID
        char apSSID [33] = DEFAULT_AP_SSID;
      #else
        char apSSID [33] = "";
      #endif
      #ifdef DEFAULT_AP_PASSWORD
        char apPassword [64] = DEFAULT_AP_PASSWORD;
      #else
        char apPassword [64] = "";
      #endif
      #ifdef DEFAULT_AP_IP
        char apIP [46] = DEFAULT_AP_IP;
      #else
        char apIP [46] = "";
      #endif
      #ifdef DEFAULT_AP_SUBNET_MASK
        char apSubnetMask [46] = DEFAULT_AP_SUBNET_MASK;
      #else
        char apSubnetMask [46] = "";
      #endif
      #ifdef DEFAULT_AP_IP
        char apGateway [46] = DEFAULT_AP_IP;    
      #else
        char apGateway [46] = "";    
      #endif
  
      #ifdef __FILE_SYSTEM__
        if (fileSystem.mounted ()) { 
          // read interfaces configuration from /network/interfaces, create a new one if it doesn't exist
          if (!fileSystem.isFile ((char *) "/network/interfaces")) {
            // create directory structure
            if (!fileSystem.isDirectory ((char *) "/network")) { fileSystem.makeDirectory ((char *) "/network"); }
            Serial.printf ("[%10lu] [network] /network/interfaces does not exist, creating default one ... ", millis ());
            bool created = false;
            File f = fileSystem.open ((char *) "/network/interfaces", "w", true);          
            if (f) {
              #if defined DEFAULT_STA_IP && defined DEFAULT_STA_SUBNET_MASK && defined DEFAULT_STA_GATEWAY && defined DEFAULT_STA_DNS_1 && defined DEFAULT_STA_DNS_2
                String defaultContent = F ("# WiFi STA(tion) configuration - reboot for changes to take effect\r\n\r\n"
                                            "# get IP address from DHCP\r\n"
                                            "#  iface STA inet dhcp\r\n"                  
                                            "\r\n"
                                            "# use static IP address\r\n"                   
                                            "   iface STA inet static\r\n"
                                            "      address " DEFAULT_STA_IP "\r\n" 
                                            "      netmask " DEFAULT_STA_SUBNET_MASK "\r\n" 
                                            "      gateway " DEFAULT_STA_GATEWAY "\r\n"
                                            "      dns1 " DEFAULT_STA_DNS_1 "\r\n"
                                            "      dns2 " DEFAULT_STA_DNS_2 "\r\n"
                                           );
              #else
                String defaultContent = F ("# WiFi STA(tion) configuration - reboot for changes to take effect\r\n\r\n"
                                           "# get IP address from DHCP\r\n"
                                           "   iface STA inet dhcp\r\n"                  
                                           "\r\n"
                                           "# use static IP address (example below)\r\n"   
                                           "#  iface STA inet static\r\n"
                                           #ifdef DEFAULT_STA_IP
                                             "#     address " DEFAULT_STA_IP "\r\n"
                                           #else
                                             "#     address \r\n"
                                           #endif
                                           #ifdef DEFAULT_STA_SUBNET_MASK
                                             "#     netmask " DEFAULT_STA_SUBNET_MASK "\r\n"
                                           #else
                                             "#     netmask \r\n"
                                           #endif
                                           #ifdef DEFAULT_STA_GATEWAY
                                             "#     gateway " DEFAULT_STA_GATEWAY "\r\n"
                                           #else
                                             "#     gateway \r\n"
                                           #endif
                                           #ifdef DEFAULT_STA_DNS_1
                                             "#     dns1 " DEFAULT_STA_DNS_1 "\r\n"
                                           #else
                                             "#     dns1 \r\n"
                                           #endif
                                           #ifdef DEFAULT_STA_DNS_2
                                             "#     dns2 " DEFAULT_STA_DNS_2 "\r\n"
                                           #else
                                             "#     dns2 \r\n"
                                           #endif
                                          );
              #endif
              created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());                                
              f.close ();

              diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

            }
            Serial.printf (created ? "created\n" : "error\n");           
          }
          {
            Serial.printf ("[%10lu] [network] reading /network/interfaces\n", millis ());
            // /network/interfaces STA(tion) configuration - parse configuration file if it exists
            char buffer [MAX_INTERFACES_SIZE];
            strcpy (buffer, "\n");
            if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/network/interfaces")) {
              Serial.printf ("\n%s\n\n", buffer + 1);
              strcat (buffer, "\n");
              if (strstr (buffer, "iface STA inet static")) {
                strBetween (staIP, sizeof (staIP), buffer, "\naddress ", "\n");
                strBetween (staSubnetMask, sizeof (staSubnetMask), buffer, "\nnetmask ", "\n");
                strBetween (staGateway, sizeof (staGateway), buffer, "\ngateway ", "\n");
                strBetween (staDns1, sizeof (staDns1), buffer, "\ndns1 ", "\n");
                strBetween (staDns2, sizeof (staDns2), buffer, "\ndns2 ", "\n");
              } else {
                *staIP = 0;
                *staSubnetMask = 0;
                *staGateway = 0;
                *staDns1 = 0;
                *staDns2 = 0;
              }
            } 
          }


          // read STAtion credentials from /etc/wpa_supplicant/wpa_supplicant.conf, create a new one if it doesn't exist
          if (!fileSystem.isFile ((char *) "/etc/wpa_supplicant/wpa_supplicant.conf")) {
            // create directory structure
            if (!fileSystem.isDirectory ((char *) "/etc/wpa_supplicant")) { fileSystem.makeDirectory ((char *) "/etc"); fileSystem.makeDirectory ((char *) "/etc/wpa_supplicant"); }
            Serial.printf ("[%10lu] [network] /etc/wpa_supplicant/wpa_supplicant.conf does not exist, creating default one ... ", millis ());
            bool created = false;
            File f = fileSystem.open ((char *) "/etc/wpa_supplicant/wpa_supplicant.conf", "w", true);          
            if (f) {
              String defaultContent = F ("# WiFi STA (station) credentials - reboot for changes to take effect\r\n\r\n"
                                          #ifdef DEFAULT_STA_SSID
                                            "   ssid " DEFAULT_STA_SSID "\r\n"
                                          #else
                                            "   ssid \r\n"
                                          #endif
                                          #ifdef DEFAULT_STA_PASSWORD
                                            "   psk " DEFAULT_STA_PASSWORD "\r\n"
                                          #else
                                            "   psk \r\n"
                                          #endif           
                                         );
              created = (f.printf (defaultContent.c_str()) == defaultContent.length ());
              f.close ();

              diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

            }
            Serial.printf (created ? "created\n" : "error\n");           
          }
          {              
              Serial.printf ("[%10lu] [network] reading /etc/wpa_supplicant/wpa_supplicant.conf\n", millis ());
              // /etc/wpa_supplicant/wpa_supplicant.conf STA(tion) credentials - parse configuration file if it exists
              char buffer [MAX_WPA_SUPPLICANT_SIZE];
              strcpy (buffer, "\n");
              if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/wpa_supplicant/wpa_supplicant.conf")) {
                  Serial.printf ("\n%s\n\n", buffer + 1);
                  strcat (buffer, "\n");
                  strBetween (staSSID, sizeof (staSSID), buffer, "\nssid ", "\n");
                  strBetween (staPassword, sizeof (staPassword), buffer, "\npsk ", "\n");
                  // Serial.printf ("[%10lu] [network] staSSID = %s, staPassword = %s\n", millis (), staSSID, staPassword);
              } else {
                  Serial.printf ("[%10lu] [network] can't read /etc/wpa_supplicant/wpa_supplicant.conf", millis ());
              }
          }

          // read A(ccess) P(oint) configuration from /etc/dhcpcd.conf, create a new one if it doesn't exist
          if (!fileSystem.isFile ((char *) "/etc/dhcpcd.conf")) {
            // create directory structure
            if (!fileSystem.isDirectory ((char *) "/etc")) fileSystem.makeDirectory ((char *) "/etc");
            Serial.printf ("[%10lu] [network] /etc/dhcpcd.conf does not exist, creating default one ... ", millis ());
            bool created = false;
            File f = fileSystem.open ((char *) "/etc/dhcpcd.conf", "w", true);          
            if (f) {
              String defaultContent = F ("# WiFi AP configuration - reboot for changes to take effect\r\n\r\n"
                                         "iface AP\r\n"
                                         #ifdef DEFAULT_AP_IP
                                           "   static ip_address " DEFAULT_AP_IP "\r\n"
                                         #else
                                           "   static ip_address \r\n"
                                         #endif
                                         #ifdef DEFAULT_AP_SUBNET_MASK
                                           "   netmask " DEFAULT_AP_SUBNET_MASK "\r\n"
                                         #else
                                           "   netmask \r\n"
                                         #endif
                                         #ifdef DEFAULT_AP_IP
                                           "   gateway " DEFAULT_AP_IP "\r\n"
                                         #else
                                           "   gateway \r\n"
                                         #endif
                                        );
              created = (f.printf (defaultContent.c_str()) == defaultContent.length ());
              f.close ();

              diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

            }
            Serial.printf (created ? "created\n" : "error\n");           
          }
          {              
            Serial.printf ("[%10lu] [network] reading /etc/dhcpcd.conf\n", millis ());
            // /etc/dhcpcd.conf contains A(ccess) P(oint) configuration - parse configuration file if it exists
            char buffer [MAX_DHCPCD_SIZE];
            strcpy (buffer, "\n");
            if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/dhcpcd.conf")) {
              Serial.printf ("\n%s\n\n", buffer + 1);
              strcat (buffer, "\n");
              strBetween (apIP, sizeof (apIP), buffer, "\nstatic ip_address ", "\n");          
              strBetween (apSubnetMask, sizeof (apSubnetMask), buffer, "\nnetmask ", "\n");
              strBetween (apGateway, sizeof (apGateway), buffer, "\ngateway ", "\n");
            } 
          }


          // read A(ccess) P(oint) credentials from /etc/hostapd/hostapd.conf, create a new one if it doesn't exist
          if (!fileSystem.isFile ((char *) "/etc/hostapd/hostapd.conf")) {
            // create directory structure
            if (!fileSystem.isDirectory ((char *) "/etc/hostapd")) { fileSystem.makeDirectory ((char *) "/etc"); fileSystem.makeDirectory ((char *) "/etc/hostapd"); }
            Serial.printf ("[%10lu] [network] /etc/hostapd/hostapd.conf does not exist, creating default one ... ", millis ());
            bool created = false;
            File f = fileSystem.open ((char *) "/etc/hostapd/hostapd.conf", "w", true);
            if (f) {
              String defaultContent = F ("# WiFi AP credentials - reboot for changes to take effect\r\n\r\n"
                                         "iface AP\r\n"
                                         #ifdef DEFAULT_AP_SSID
                                           "   ssid " DEFAULT_AP_SSID "\r\n"
                                         #else
                                           "   ssid \r\n"
                                         #endif
                                         "   # use at least 8 characters for wpa_passphrase\r\n"
                                         #ifdef DEFAULT_AP_PASSWORD
                                           "   wpa_passphrase " DEFAULT_AP_PASSWORD "\r\n"
                                         #else
                                         "   wpa_passphrase \r\n"
                                         #endif                
                                        );
              created = (f.printf (defaultContent.c_str()) == defaultContent.length ());
              f.close ();

              diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

            }
            Serial.printf (created ? "created\n" : "error\n");           
          }
          {              
              Serial.printf ("[%10lu] [network] reading /etc/hostapd/hostapd.conf\n", millis ());
              // /etc/hostapd/hostapd.conf contains A(ccess) P(oint) credentials - parse configuration file if it exists
              char buffer [MAX_HOSTAPD_SIZE];
              strcpy (buffer, "\n");
              if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/hostapd/hostapd.conf")) {
                  Serial.printf ("\n%s\n\n", buffer + 1);
                  strcat (buffer, "\n");
                  strBetween (apSSID, sizeof (apSSID), buffer, "\nssid ", "\n");          
                  strBetween (apPassword, sizeof (apSubnetMask), buffer, "\nwpa_passphrase ", "\n");
                  // Serial.printf ("[%10lu] [network] apSSID = %s, apPassword = %s\n", millis (), apSSID, apPassword);
              } else {
                  Serial.printf ("[%10lu] [network] can't read /etc/hostapd/hostapd.conf", millis ());
              }
          }

        } else 
      #endif    
            {
              Serial.printf ("[%10lu] [network] file system not mounted, can't read or write configuration files\n", millis ());
            }

  
      // network event logging - see: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
      WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {
        static bool staStarted = false; // to prevent unneccessary messages
        switch (event) {
            case SYSTEM_EVENT_WIFI_READY:           // do not report this event - it is too frequent
                                                    break;
            case SYSTEM_EVENT_SCAN_DONE:            Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] completed scan for access points");
                                                    break;
            case SYSTEM_EVENT_STA_START:            if (!staStarted) {
                                                      staStarted = true;
                                                      Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi client started");
                                                    }
                                                    break;
            case SYSTEM_EVENT_STA_STOP:             Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi clients stopped");
                                                    break;
            case SYSTEM_EVENT_STA_CONNECTED:        Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][STA] connected to WiFi ", (char *) WiFi.SSID ().c_str ());
                                                    break;
            case SYSTEM_EVENT_STA_DISCONNECTED:     if (staStarted) {
                                                      staStarted = false;
                                                      #ifdef __DMESG__
                                                          dmesg ("[network][STA] disconnected from WiFi");
                                                      #endif
                                                      Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] disconnected from WiFi");
                                                    }
                                                    break;
            case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] authentication mode has changed");
                                                    break;
            case SYSTEM_EVENT_STA_GOT_IP:           
                                                    #ifdef __DMESG__
                                                        dmesg ("[network][STA] got IP address: ", (char *) WiFi.localIP ().toString ().c_str ());
                                                    #endif
                                                    Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][STA] got IP address: ", (char *) WiFi.localIP ().toString ().c_str ());
                                                    break;
            case SYSTEM_EVENT_STA_LOST_IP:
                                                    #ifdef __DMESG__
                                                        dmesg ("[network][STA] lost IP address. IP address reset to 0.0.0.0");
                                                    #endif
                                                    Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] lost IP address. IP address reset to 0.0.0.0");
                                                    break;
            case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:   Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi Protected Setup (WPS): succeeded in enrollee mode");
                                                    break;
            case SYSTEM_EVENT_STA_WPS_ER_FAILED:    Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi Protected Setup (WPS): failed in enrollee mode");
                                                    break;
            case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:   Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi Protected Setup (WPS): timeout in enrollee mode");
                                                    break;
            case SYSTEM_EVENT_STA_WPS_ER_PIN:       Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] WiFi Protected Setup (WPS): pin code in enrollee mode");
                                                    break;
            case SYSTEM_EVENT_AP_START:             
                                                    #ifdef __DMESG__
                                                        dmesg ("[network][AP] WiFi access point started");
                                                    #endif
                                                    Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] WiFi access point started");
                                                    // AP hostname can't be set until AP interface is mounted 
                                                    { 
                                                        esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); // outdated, use: esp_netif_set_hostname
                                                        if (e != ESP_OK) {
                                                            #ifdef __DMESG__
                                                                dmesg ("[network][AP] couldn't change AP adapter hostname"); 
                                                            #endif
                                                            Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] couldn't change AP adapter hostname");
                                                        }
                                                    } 
                                                    break;
            case SYSTEM_EVENT_AP_STOP:              
                                                    #ifdef __DMESG__
                                                        dmesg ("[network][AP] WiFi access point stopped");
                                                    #endif
                                                    Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] WiFi access point stopped");
                                                    break;
            case SYSTEM_EVENT_AP_STACONNECTED:      // do not report this event - it is too frequent
                                                    break;
            case SYSTEM_EVENT_AP_STADISCONNECTED:   // do not report this event - it is too frequent
                                                    break;
            case SYSTEM_EVENT_AP_STAIPASSIGNED:     // do not report this event - it is too frequent
                                                    break;
            case SYSTEM_EVENT_AP_PROBEREQRECVED:    break;
            case SYSTEM_EVENT_GOT_IP6:              break;
            /*
            case SYSTEM_EVENT_ETH_START:            dmesg ("[network] ethernet started");
                                                    break;
            case SYSTEM_EVENT_ETH_STOP:             dmesg ("[network] ethernet stopped");
                                                    break;
            case SYSTEM_EVENT_ETH_CONNECTED:        dmesg ("[network] ethernet connected");
                                                    break;
            case SYSTEM_EVENT_ETH_DISCONNECTED:     dmesg ("[network] ethernet disconnected");
                                                    break;
            case SYSTEM_EVENT_ETH_GOT_IP:           dmesg ("[network] ethernet got IP address");
                                                    break;        
            */
            default:                                
                                                    #ifdef __DMESG__
                                                        dmesg ("[network] event: ", event); // shouldn't happen
                                                    #endif
                                                    Serial.printf ("[%10lu] %s%i\r\n", millis (), "[network] event: ", event);
                                                    break;
        }
      });    
  
      // connect STA and AP 
      if (*staSSID) { 
          #ifdef __DMESG__
              dmesg ("[network][STA] connecting to router: ", staSSID);
          #endif
          Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][STA] connecting to router: ", staSSID);

          if (*staIP) { 
              // dmesg ("[network][STA] connecting STAtion to router with static IP: ", (char *) (String (staIP) + " GW: " + String (staGateway) + " MSK: " + String (staSubnetMask) + " DNS: " + String (staDns1) + ", " + String (staDns2)).c_str ());
              #ifdef __DMESG__
                  dmesg ("[network][STA] using static IP: ", staIP);
              #endif
              Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][STA] using static IP: ", staIP);
              WiFi.config (__IPAddressFromString__ (staIP), __IPAddressFromString__ (staGateway), __IPAddressFromString__ (staSubnetMask), *staDns1 ? __IPAddressFromString__ (staDns1) : IPAddress (255, 255, 255, 255), *staDns2 ? __IPAddressFromString__ (staDns2) : IPAddress (255, 255, 255, 255)); // INADDR_NONE == 255.255.255.255
          } else { 
              #ifdef __DMESG__
                  dmesg ("[network][STA] using DHCP");
              #endif
              Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] using DHCP");
          }
          // Serial.printf ("[%10lu] [network][STA] staSSID = %s, staPassword = %s\n", millis (), staSSID, staPassword);
          WiFi.begin (staSSID, staPassword);
      }

      // set hostname for all netifs: https://github.com/espressif/esp-idf/blob/master/components/esp_netif/include/esp_netif.h
      // esp_netif_t *netif = esp_netif_next (NULL);
      // while (netif != NULL) {
      //     if (esp_netif_set_hostname (netif, HOSTNAME) != ESP_OK)
      //         dmesg ("[network] couldn't change AP adapter hostname");
      //     netif = esp_netif_next (netif);
      // }

      if (*apSSID) { // setup AP
          #ifdef __DMESG__
              dmesg ("[network][AP] settint up access point: ", apSSID);
          #endif
          Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][AP] settint up access point: ", apSSID);

          if (WiFi.softAP (apSSID, apPassword)) { 
              // dmesg ("[network] [AP] initializing access point: ", (char *) (String (apSSID) + "/" + String (apPassword) + ", " + String (apIP) + ", " + String (apGateway) + ", " + String (apSubnetMask)).c_str ()); 
              WiFi.softAPConfig (__IPAddressFromString__ (apIP), __IPAddressFromString__ (apGateway), __IPAddressFromString__ (apSubnetMask));
              WiFi.begin ();
              #ifdef __DMESG__
                  dmesg ("[network][AP] access point IP: ", (char *) WiFi.softAPIP ().toString ().c_str ());
              #endif
              Serial.printf ("[%10lu] %s%s\r\n", millis (), "[network][AP] access point IP: ", (char *) WiFi.softAPIP ().toString ().c_str ());
          } else {
              // ESP.restart ();
              #ifdef __DMESG__
                  dmesg ("[network][AP] failed to initialize access point"); 
              #endif
              Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] failed to initialize access point");
          }
      } 

      // set WiFi mode
      if (*staSSID) { 
        if (*apSSID) {

            if (tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME) != ESP_OK) { // outdated, use: esp_netif_set_hostname
                #ifdef __DMESG__
                    dmesg ("[network][AP] couldn't change AP adapter hostname"); 
                #endif
                Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] couldn't change AP adapter hostname");
            }
            if (tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME) != ESP_OK) { // outdated, use: esp_netif_set_hostname
                #ifdef __DMESG__
                    dmesg ("[network][STA] couldn't change STA adapter hostname"); 
                #endif
                Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] couldn't change STA adapter hostname");
            } 
            WiFi.mode (WIFI_AP_STA); // both, AP and STA modes

        } else {

            if (tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME) != ESP_OK) { // outdated, use: esp_netif_set_hostname
                #ifdef __DMESG__
                    dmesg ("[network][STA] couldn't change STA adapter hostname"); 
                #endif
                Serial.printf ("[%10lu] %s\r\n", millis (), "[network][STA] couldn't change STA adapter hostname");
            }
            WiFi.mode (WIFI_STA); // only STA mode

        }
      } else {
        
        if (*apSSID) {

            if (tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME) != ESP_OK) { // outdated, use: esp_netif_set_hostname
                #ifdef __DMESG__
                    dmesg ("[network][AP] couldn't change AP adapter hostname"); 
                #endif
                Serial.printf ("[%10lu] %s\r\n", millis (), "[network][AP] couldn't change AP adapter hostname");
            }
            WiFi.mode (WIFI_AP); // only AP mode
        
        }

      }  
      arp (-1); // call arp immediatelly after network is set up to obtain pointer to ARP table    
    }
  
    wifi_mode_t getWiFiMode () {
        wifi_mode_t retVal = WIFI_OFF;
        if (esp_wifi_get_mode (&retVal) != ESP_OK); // dmesg ("[network] couldn't get WiFi mode");
        return retVal;
    }

    // ----- arp reference:  https://github.com/yarrick/lwip/blob/master/src/core/ipv4/etharp.c -----

    // first (re)make a definition of ARP table and get a pointer to it (not very elegant but I have no other idea how to get reference to arp table)
    enum etharp_state {
        ETHARP_STATE_EMPTY = 0,
        ETHARP_STATE_PENDING,
        ETHARP_STATE_STABLE,
        ETHARP_STATE_STABLE_REREQUESTING_1,
        ETHARP_STATE_STABLE_REREQUESTING_2
    #if ETHARP_SUPPORT_STATIC_ENTRIES
        ,ETHARP_STATE_STATIC
    #endif
    };
    
    struct etharp_entry {
    #if ARP_QUEUEING
        struct etharp_q_entry *q;
    #else
        struct pbuf *q;
    #endif
        ip4_addr_t ipaddr;
        struct netif *netif;
        struct eth_addr ethaddr;
        u16_t ctime;
        u8_t state;
    };
  
    // returns output of arp (telnet) command
    string arp (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that arp can display intermediate results over TCP connection
        // get pointer to arp table the first time function is called
        struct etharp_entry *arpTablePointer = NULL;
        if (!arpTablePointer) {
            // check if the first entry is stable so we can obtain pointer to it, then we can calculate pointer to ARP table from it
            ip4_addr_t *ipaddr;
            struct netif *netif;
            struct eth_addr *mac;
            if (etharp_get_entry (0, &ipaddr, &netif, &mac)) {
                byte offset = (byte *) &arpTablePointer->ipaddr - (byte *) arpTablePointer;
                arpTablePointer = (struct etharp_entry *) ((byte *) ipaddr - offset); // success
            }
        }
        if (!arpTablePointer) return "ARP table is not accessible right now.";
    
        // scan through ARP table
        struct netif *netif;
        // ip4_addr_t *ipaddr;
        // scan ARP table it for each netif  
        string s;
        for (netif = netif_list; netif; netif = netif->next) {
            struct etharp_entry *p = arpTablePointer; // start scan of ARP table from the beginning with the next netif
            if (netif_is_up (netif)) {
                if (s != "")
                    s += "\r\n\n";
                char ip_addr [46]; inet_ntoa_r (netif->ip_addr, ip_addr, sizeof (ip_addr));
                s += netif->name [0];
                s += netif->name [1];
                s += netif->num;
                s += ": ";
                s += ip_addr;
                s += "\r\n  Internet Address      Physical Address      Type";
                if (arpTablePointer) { // we have got a valid pointer to ARP table
                    for (int i = 0; i < ARP_TABLE_SIZE; i++) { // scan through ARP table
                        if (p->state != ETHARP_STATE_EMPTY) {
                            struct netif *arp_table_netif = p->netif; // make a copy of a pointer to netif in case arp_table entry is just beeing deleted
                            if (arp_table_netif && arp_table_netif->num == netif->num) { // if ARP entry is for the same as netif we are displaying
                                if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                                    sendAll (telnetSocket, s, 1000); 
                                    s = "";
                                }
                                s += "\r\n  ";
                                string ip_addr; inet_ntoa_r (p->ipaddr, ip_addr, sizeof (ip_addr));
                                ip_addr.rPad (22, ' ');
                                s += ip_addr;
                                s += __mac_ntos__ ((byte *) &p->ethaddr, NETIF_MAX_HWADDR_LEN);
                                s += (p->state > ETHARP_STATE_STABLE_REREQUESTING_2 ? "     static" : "     dynamic");
                            } 
                        }
                        p ++;
                    } // scan through ARP table
                } // we have got a valid pointer to ARP table
            }
        }
        return s;
    }  
  
  
    //------ IW -----
  
    static SemaphoreHandle_t __WiFiSnifferSemaphore__ = xSemaphoreCreateMutex (); // to prevent two threads to start sniffing simultaneously
    static char * __macToSniffRssiFor__;  // input to __sniffWiFiForRssi__ function
    static int __rssiSniffedForMac__;     // output of __sniffWiFiForRssi__ function
  
    typedef struct {
        unsigned frame_ctrl:16;
        unsigned duration_id:16;
        uint8_t addr1 [6]; // receiver address 
        uint8_t addr2 [6]; // sender address 
        uint8_t addr3 [6]; // filtering address 
        unsigned sequence_ctrl:16;
        uint8_t addr4 [6]; // optional 
    } wifi_ieee80211_mac_hdr_t;
    
    typedef struct {
        wifi_ieee80211_mac_hdr_t hdr;
        uint8_t payload [0]; // network data ended with 4 bytes csum (CRC32)
    } wifi_ieee80211_packet_t;      
  
    int __sniffWiFiForRssi__ (char *stationMac) { // sniff WiFi Traffic for station RSSI - since we are sniffing connected stations we can stay on AP WiFi channel
                                                  // sniffing WiFi is not well documented, there are some working examples on internet however:
                                                  // https://www.hackster.io/p99will/esp32-wifi-mac-scanner-sniffer-promiscuous-4c12f4
                                                  // https://esp32.com/viewtopic.php?t=1314
                                                  // https://blog.podkalicki.com/esp32-wifi-sniffer/
      int rssi;                                          
      xSemaphoreTake (__WiFiSnifferSemaphore__, portMAX_DELAY);
          __macToSniffRssiFor__ = stationMac;
          __rssiSniffedForMac__ = 0;
          esp_wifi_set_promiscuous (true);
          const wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};      
          esp_wifi_set_promiscuous_filter (&filter);
          // esp_wifi_set_promiscuous_rx_cb (&__WiFiSniffer__);
          esp_wifi_set_promiscuous_rx_cb ([] (void* buf, wifi_promiscuous_pkt_type_t type) {
                                                                                              const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *) buf;
                                                                                              const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *) ppkt->payload;
                                                                                              const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
                                                                                              // TO DO: I'm not 100% sure that this works in all cases since source mac address may not be
                                                                                              //        always in the same place for all types and subtypes of frame
                                                                                              if (!strcmp (__macToSniffRssiFor__, __mac_ntos__ ((byte *) hdr->addr2, NETIF_MAX_HWADDR_LEN))) __rssiSniffedForMac__ = ppkt->rx_ctrl.rssi;
                                                                                              return;
                                                                                          });
            unsigned long startTime = millis ();
            while (__rssiSniffedForMac__ == 0 && millis () - startTime < 5000) delay (1); // sniff max 5 second, it should be enough
            rssi = __rssiSniffedForMac__;
            esp_wifi_set_promiscuous (false);
        xSemaphoreGive (__WiFiSnifferSemaphore__);
        return rssi;
    }
  
    string iw (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that iw can display intermediate results over TCP connection
        string s;
        struct netif *netif;
        for (netif = netif_list; netif; netif = netif->next) {
            if (netif_is_up (netif)) {
                if (s != "")
                    s += "\r\n";
                // display the following information for STA and AP interface (similar to ifconfig)
                char ip_addr [46]; inet_ntoa_r (netif->ip_addr, ip_addr, sizeof (ip_addr));
                s += netif->name [0];
                s += netif->name [1];
                s += netif->num;
                s += "     hostname: ";
                s += netif->hostname; // no need to check if NULL, fsString will do this itself
                s += "\r\n        hwaddr: ";
                s += __mac_ntos__ (netif->hwaddr, netif->hwaddr_len);
                s += "\r\n        inet addr: ";
                s += ip_addr;
                s += "\r\n";
        
                // display the following information for STA interface
                if (!strcmp (ip_addr, (char *) WiFi.localIP ().toString ().c_str ())) {
                    if (WiFi.status () == WL_CONNECTED) {
                        if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                            sendAll (telnetSocket, s, 1000); // display intermediate result                
                            s = "";
                        }
                        int rssi = WiFi.RSSI ();
                        char *rssiDescription = (char *) ""; if (rssi == 0) rssiDescription = (char *) "not available"; else if (rssi >= -30) rssiDescription = (char *) "excelent"; else if (rssi >= -67) rssiDescription = (char *) "very good"; else if (rssi >= -70) rssiDescription = (char *) "okay"; else if (rssi >= -80) rssiDescription = (char *) "not good"; else if (rssi >= -90) rssiDescription = (char *) "bad"; else /* if (rssi < -90) */ rssiDescription = (char *) "unusable";
                        s += "           STAtion is connected to router:\r\n\n"
                             "              inet addr: ";
                        s += WiFi.gatewayIP ().toString ().c_str ();
                        s += "     RSSI: ";
                        s += string (rssi);
                        s += " dBm (";
                        s += rssiDescription;
                        s += ")\r\n";
                    } else {
                        s += "           STAtion is disconnected from router\r\n";
                    }

                // display the following information for local loopback interface
                } else if (!strcmp (ip_addr, "127.0.0.1")) {
                    s += "           local loopback\r\n";
                // display the following information for AP interface
                } else {
                    wifi_sta_list_t wifi_sta_list = {};
                    tcpip_adapter_sta_list_t adapter_sta_list = {};
                    esp_wifi_ap_get_sta_list (&wifi_sta_list);
                    tcpip_adapter_get_sta_list (&wifi_sta_list, &adapter_sta_list);
                    if (adapter_sta_list.num) {
                        s += "           stations connected to Access Point (";
                        s += string (adapter_sta_list.num);
                        s += "):\r\n\n";
                        for (int i = 0; i < adapter_sta_list.num; i++) {
                            if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                                sendAll (telnetSocket, s, 1000); // display intermediate result                
                                s = "";
                            }
                            tcpip_adapter_sta_info_t station = adapter_sta_list.sta [i];
                            char ip_addr [46]; inet_ntoa_r (station.ip, ip_addr, sizeof (ip_addr));
                            s += "              inet addr: ";
                            s += ip_addr;
                            int rssi = __sniffWiFiForRssi__ (__mac_ntos__ ((byte *) &station.mac, NETIF_MAX_HWADDR_LEN));
                            char *rssiDescription = (char *) ""; if (rssi == 0) rssiDescription = (char *) "not available"; else if (rssi >= -30) rssiDescription = (char *) "excelent"; else if (rssi >= -67) rssiDescription = (char *) "very good"; else if (rssi >= -70) rssiDescription = (char *) "okay"; else if (rssi >= -80) rssiDescription = (char *) "not good"; else if (rssi >= -90) rssiDescription = (char *) "bad"; else /* if (rssi < -90) */ rssiDescription = (char *) "unusable";
                            s += "     RSSI: ";
                            s += rssi;
                            s += " dBm (";
                            s += rssiDescription;
                            s += ")     hwaddr: ";
                            s += __mac_ntos__ ((byte *) &station.mac, NETIF_MAX_HWADDR_LEN);
                            s += "\r\n";
                        }
                    } else {
                        s += "           there are no stations connected to Access Point\r\n";
                    } // ap
                } // adapter: ap, st or lo
            } // f netiu is up
        } // for netif
        return s;
    }
  
  
    //------ IFCONFIG -----
  
    string ifconfig (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that ifconfig can display intermediate results over TCP connection
        string s;

        // implementation with esp_netif, the information missing is: MTU and local loopback
        /*
        {
            esp_netif_t *netif = esp_netif_next (NULL);
            while (netif != NULL) {
                if (esp_netif_is_netif_up (netif)) {

                    if (s != "") {
                        if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                            sendAll (telnetSocket, s, 1000);
                            s = "";
                        }
                        s += "\r\n\n";
                    } 

                    fsString<6> netifName; // maximum interface name size (6 characters for lwIP)
                    if (esp_netif_get_netif_impl_name (netif, netifName.c_str ()) == ESP_OK) {
                        netifName.rPad (6, ' ');
                        s += netifName;
                    }                

                    const char *netifHostname;
                    if (esp_netif_get_hostname (netif, &netifHostname) == ESP_OK) {
                        s += "  ";
                        s += (char *) netifHostname;
                    } 

                    s += "\r\n        hwaddr: ";
                    uint8_t mac [NETIF_MAX_HWADDR_LEN]; // 48 bits, 6 bytes
                    if (esp_netif_get_mac (netif, mac) == ESP_OK) {
                        s += __mac_ntos__ (mac, NETIF_MAX_HWADDR_LEN);
                    }

                    s += "\r\n        ip: ";
                    esp_netif_ip_info_t ip_info;
                    if (esp_netif_get_ip_info (netif, &ip_info) == ESP_OK) {
                        char ip_addr [46]; 
                        inet_ntoa_r (ip_info.ip.addr, ip_addr, sizeof (ip_addr));
                        s += ip_addr;
                        s += "  netmask: ";
                        inet_ntoa_r (ip_info. netmask.addr, ip_addr, sizeof (ip_addr));
                        s += ip_addr;
                        s += "  gw: ";
                        inet_ntoa_r (ip_info.gw.addr, ip_addr, sizeof (ip_addr));
                        s += ip_addr;
                    }

                } // neif is up
                netif = esp_netif_next (netif);
            } // while
        }
        return s;
        */
        
        // implementation with lwip netif
        {
            struct netif *netif;
            for (netif = netif_list; netif; netif = netif->next) {
                if (netif_is_up (netif)) {
                    if (s != "") {
                      if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                          sendAll (telnetSocket, s, 1000);
                          s = "";
                      }
                      s += "\r\n\n";
                    } 
                    char ip_addr [46]; inet_ntoa_r (netif->ip_addr, ip_addr, sizeof (ip_addr));
                    s += netif->name [0];
                    s += netif->name [1];
                    s += netif->num;
                    s += + "     hostname: ";
                    s += netif->hostname; // no need to check if NULL, fsString will do this itself
                    s += "\r\n        hwaddr: ";
                    s += __mac_ntos__ (netif->hwaddr, netif->hwaddr_len);
                    s += "\r\n        inet addr: ";
                    s += ip_addr;
                    s += "\r\n        mtu: ";
                    s += netif->mtu;
                }
            }
        }
        return s;    
    }
  
    //------ PING, thank you pbecchi (https://github.com/pbecchi/ESP32_ping) -----
  
    #include <lwip/inet_chksum.h>
    #include <lwip/ip.h>
    #include <lwip/ip4.h>
    #include <lwip/err.h>
    #include <lwip/icmp.h>
    #include <lwip/sockets.h>
    #include <lwip/sys.h>
    #include <lwip/netdb.h>
    #include <lwip/dns.h>
  
    #define PING_DEFAULT_COUNT     4
    #define PING_DEFAULT_INTERVAL  1
    #define PING_DEFAULT_SIZE     32
    #define PING_DEFAULT_TIMEOUT   1
  
    struct __pingDataStructure__ {
      uint16_t ID;
      uint16_t pingSeqNum;
      uint8_t stopped = 0;
      uint32_t transmitted = 0;
      uint32_t received = 0;
      float minTime = 0;
      float maxTime = 0;
      float meanTime = 0;
      float lastMeanTime = 0;
      float varTime = 0;
    };
  
    static void __pingPrepareEcho__ (__pingDataStructure__ *pds, struct icmp_echo_hdr *iecho, uint16_t len) {
      size_t i;
      size_t data_len = len - sizeof (struct icmp_echo_hdr);
    
      ICMPH_TYPE_SET (iecho, ICMP_ECHO);
      ICMPH_CODE_SET (iecho, 0);
      iecho->chksum = 0;
      iecho->id = pds->ID;
      iecho->seqno = htons (++pds->pingSeqNum);
    
      /* fill the additional data buffer with some data */
      for (i = 0; i < data_len; i++) ((char*) iecho)[sizeof (struct icmp_echo_hdr) + i] = (char) i;
    
      iecho->chksum = inet_chksum (iecho, len);
    }
  
    static err_t __pingSend__ (__pingDataStructure__ *pds, int s, ip4_addr_t *addr, int pingSize) {
      struct icmp_echo_hdr *iecho;
      struct sockaddr_in to;
      size_t ping_size = sizeof (struct icmp_echo_hdr) + pingSize;
      int err;
    
      if (!(iecho = (struct icmp_echo_hdr *) mem_malloc ((mem_size_t) ping_size))) return ERR_MEM;
    
      __pingPrepareEcho__ (pds, iecho, (uint16_t) ping_size);
    
      to.sin_len = sizeof (to);
      to.sin_family = AF_INET;
      to.sin_addr = *(in_addr *) addr; // inet_addr_from_ipaddr (&to.sin_addr, addr);
      
      if ((err = sendto (s, iecho, ping_size, 0, (struct sockaddr*) &to, sizeof (to)))) pds->transmitted ++;
    
      return (err ? ERR_OK : ERR_VAL);
    }
  
    static bool __pingRecv__ (__pingDataStructure__ *pds, int telnetSocket, int s) { // dispalys intermediate result if called from tlnet server (telnetSocket > 0)
      char buf [64];
      int fromlen, len;
      struct sockaddr_in from;
      struct ip_hdr *iphdr;
      struct icmp_echo_hdr *iecho = NULL;
      char ipa [16];
      struct timeval begin;
      struct timeval end;
      uint64_t microsBegin;
      uint64_t microsEnd;
      float elapsed;
  
      char cstr [255];    
    
      // Register begin time
      gettimeofday (&begin, NULL);
    
      // Send
      while ((len = recvfrom (s, buf, sizeof (buf), 0, (struct sockaddr *) &from, (socklen_t *) &fromlen)) > 0) {
        if (len >= (int)(sizeof(struct ip_hdr) + sizeof (struct icmp_echo_hdr))) {

          #ifdef __FILE_SYSTEM__
              diskTrafficInformation.bytesRead += len; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
          #endif
          
          // Register end time
          gettimeofday (&end, NULL);
    
          // Get from IP address
          ip4_addr_t fromaddr;
          fromaddr = *(ip4_addr_t *) &from.sin_addr; // inet_addr_to_ipaddr (&fromaddr, &from.sin_addr);
          inet_ntoa_r (fromaddr, ipa, sizeof (ipa)); 
    
          // Get echo
          iphdr = (struct ip_hdr *) buf;
          iecho = (struct icmp_echo_hdr *) (buf + (IPH_HL (iphdr) * 4));
    
          // Print ....
          if ((iecho->id == pds->ID) && (iecho->seqno == htons (pds->pingSeqNum))) {
            pds->received ++;
    
            // Get elapsed time in milliseconds
            microsBegin = begin.tv_sec * 1000000;
            microsBegin += begin.tv_usec;
    
            microsEnd = end.tv_sec * 1000000;
            microsEnd += end.tv_usec;
    
            elapsed = (float) (microsEnd - microsBegin) / (float) 1000.0;
    
            // Update statistics
            // Mean and variance are computed in an incremental way
            if (elapsed < pds->minTime) pds->minTime = elapsed;
            if (elapsed > pds->maxTime) pds->maxTime = elapsed;
    
            pds->lastMeanTime = pds->meanTime;
            pds->meanTime = (((pds->received - 1) * pds->meanTime) + elapsed) / pds->received;
    
            if (pds->received > 1) pds->varTime = pds->varTime + ((elapsed - pds->lastMeanTime) * (elapsed - pds->meanTime));
    
            // Print ...
            sprintf (cstr, "%d bytes from %s: icmp_seq = %d time = %.3f ms\r\n", len, ipa, ntohs (iecho->seqno), elapsed);
            if (telnetSocket >= 0) if (sendAll (telnetSocket, cstr, strlen (cstr), 1000) <= 0) return false; // dispalys intermediate result if called from tlnet server (telnetSocket > 0)      
            
            return true;
          }
          else {
            // TODO: 
          }
        }
      }
    
      if (len < 0) {
        sprintf (cstr, "Request timeout for icmp_seq %d\r\n", pds->pingSeqNum);
        if (telnetSocket >= 0) if (sendAll (telnetSocket, cstr, strlen (cstr), 1000) <= 0) return false; // dispalys intermediate result if called from tlnet server (telnetSocket > 0)      
      }
      return false;
    }  
  
    uint32_t ping (char *targetName, int pingCount = PING_DEFAULT_COUNT, int pingInterval = PING_DEFAULT_INTERVAL, int pingSize = PING_DEFAULT_SIZE, int timeOut = PING_DEFAULT_TIMEOUT, int telnetSocket = -1) { // dispalys intermediate result if called from tlnet server (telnetSocket > 0)
      // get target address
      struct hostent *he = gethostbyname (targetName);
      if (!he) {
        #ifdef __DMESG__
            dmesg ("[network][ping] gethostbyname () error: ", h_errno, hstrerror (h_errno));
        #endif
        if (telnetSocket >= 0) sendAll (telnetSocket, (char *) "gethostbyname () error", strlen ("gethostbyname () error"), 1000); // if called from Telnet server
        return 0;
      }

      // struct sockaddr_in address;
      ip4_addr_t pingTarget;
      int s;
      char cstr [256];

      // Create socket
      if ((s = socket (AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0) {
        if (telnetSocket >= 0) sendAll (telnetSocket, (char *) "Error creating socket.", strlen ("Error creating socket."), 1000); // if called from Telnet server
        return 0;
      }
      
      pingTarget.addr = *(in_addr_t *) he->h_addr;
      
      // Setup socket
      struct timeval tOut;
      
      // Timeout
      tOut.tv_sec = timeOut;
      tOut.tv_usec = 0;
      
      if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tOut, sizeof (tOut)) < 0) {
        closesocket (s);
        if (telnetSocket >= 0) sendAll (telnetSocket, (char *) "Error setting socket options.", strlen ("Error setting socket options."), 1000); // dispalys intermediate result if called from tlnet server (telnetSocket > 0)
        return 0;
      }
      
      __pingDataStructure__ pds = {};
      pds.ID = random (0, 0xFFFF); // each consequently running ping command should have its own unique ID otherwise we won't be able to distinguish packets 
      pds.minTime = 1.E+9; // FLT_MAX;
      
      // Begin ping ...
  
      strcpy (cstr, "ping ");
      inet_ntoa_r (pingTarget, cstr + 5, sizeof (cstr) - 5);
      sprintf (cstr + strlen (cstr), " with %d data bytes\r\n", pingSize);
      if (telnetSocket >= 0) if (sendAll (telnetSocket, cstr, strlen (cstr), 1000) <= 0) return 0; // dispalys intermediate result if called from tlnet server (telnetSocket > 0)
      
      while ((pds.pingSeqNum < pingCount) && (!pds.stopped)) {
        if (__pingSend__ (&pds, s, &pingTarget, pingSize) == ERR_OK) if (!__pingRecv__ (&pds, telnetSocket, s)) return pds.received;

        networkTrafficInformation.bytesSent += pingSize; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

        delay (pingInterval * 1000L);
      }
      
      closesocket (s);
      
      sprintf (cstr, "\r\n%u packets transmitted, %u packets received, %.1f%% packet loss\r\n", pds.transmitted, pds.received, ((((float) pds.transmitted - (float) pds.received) / (float) pds.transmitted) * 100.0));
      if (pds.received) { // ok
        sprintf (cstr + strlen (cstr), "round-trip min / avg / max / stddev = %.3f / %.3f / %.3f / %.3f ms\r\n", pds.minTime, pds.meanTime, pds.maxTime, sqrt (pds.varTime / pds.received));
      } // else errd
      if (telnetSocket >= 0) sendAll (telnetSocket, cstr, strlen (cstr), 1000); // dispalys intermediate result if called from tlnet server (telnetSocket > 0)
      
      return pds.received;
    }

#endif
