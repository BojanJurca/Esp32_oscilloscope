/*

    network.h

    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino

    network.h reads network configuration from file system and sets both ESP32 network interfaces accordingly
 
    It is a little awkward why Unix, Linux are using so many network configuration files and how they are used):

      /network/interfaces                       - modify the code below with your IP addresses
      /etc/wpa_supplicant/wpa_supplicant.conf   - modify the code below with your WiFi SSID and password
      /etc/dhcpcd.conf                          - modify the code below with your access point IP addresses
      /etc/hostapd/hostapd.conf                 - modify the code below with your access point SSID and password

    May 22, 2024, Bojan Jurca

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
    #include "std/Cstring.hpp"
    #include "std/console.hpp"


#ifndef __NETWK__
    #define __NETWK__

    #ifndef __FILE_SYSTEM__
      #ifdef SHOW_COMPILE_TIME_INFORMATION
          #pragma message "Compiling network.h without file system (file_system.h), network.h will not use configuration files"
      #endif
    #endif


    // ----- functions and variables in this modul -----

    void startWiFi ();

    cstring arp (int);
    cstring iw (int);
    cstring ifconfig (int);


    // ----- TUNNING PARAMETERS -----

    #ifndef HOSTNAME
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "HOSTNAME was not defined previously, #defining the default MyESP32Server in network.h"
        #endif
      #define HOSTNAME                  "YourESP32Server"  // use default if not defined previously, max 32 bytes  ESP_NETIF_HOSTNAME_MAX_SIZE
    #endif
    // define default STAtion mode parameters to be written to /etc/wpa_supplicant/wpa_supplicant.conf if you want to use ESP as WiFi station
    #ifndef DEFAULT_STA_SSID
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_STA_SSID was not defined previously, #defining the default YOUR_STA_SSID which most likely will not work"
        #endif
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

      // define use types of sockets, this is just to log statistic and other information that netstat telnet command would use
      #define __LISTENING_SOCKET__ 1
      #define __TELNET_SERVER_SOCKET__ 2
      #define __TELNET_CLIENT_SOCKET__ 3
      #define __FTP_SERVER_SOCKET__ 4 // server, control connetion
      #define __FTP_CLIENT_SOCKET__ 5 // clietn, control connection
      #define __FTP_DATA_SOCKET__ 6   // server or client, data connetion
      #define __HTTP_SERVER_SOCKET__ 7
      #define __HTTP_CLIENT_SOCKET__ 8
      #define __SMTP_CLIENT_SOCKET__ 9
      #define __NTP_SOCKET__ 10
      #define __PING_SOCKET__ 11

      // log network Traffic information for each socket
      struct additionalNetworkInformation_t {
          // usage
          byte lastUsedAs; // LISTENING_SOCKET, ...
          // statistics
          unsigned long bytesReceived;
          unsigned long  bytesSent;
          // debug information 
          unsigned long startMillis; // a time when socket was opened
          unsigned long lastActiveMillis; // a time when data was last sent or received
          // ... add more if needed
      };
      additionalNetworkInformation_t additionalNetworkInformation = {}; // measure network Traffic on ESP32 level
      additionalNetworkInformation_t additionalSocketInformation [MEMP_NUM_NETCONN] = {}; // there are MEMP_NUM_NETCONN available sockets measure network Traffic on each socket level

      // missing hstrerror function
      #include <lwip/netdb.h>
      const char *hstrerror (int h_errno) {
          switch (h_errno) {
              case EAI_NONAME:      return "Name or service is unknown";
              case EAI_SERVICE:     return "Service not supported for 'ai_socktype'";
              case EAI_FAIL:        return "Non-recoverable failure in name res";
              case EAI_MEMORY:      return "Memory allocation failure";
              case EAI_FAMILY:      return "'ai_family' not supported";
              case HOST_NOT_FOUND:  return "The specified host is unknown";
              case NO_DATA:         return "The requested name is valid but does not have an IP address";
              case NO_RECOVERY:     return "The requested name is valid but does not have an IP address";
              case TRY_AGAIN:       return "A temporary error occurred on an authoritative name server. Try again later";
              default:              return "Invalid h_errno code";
          }
      }

      // returns len which is the number of bytes actually sent or 0 indicatig an error, buf is send in separate blocks of size TCP_SND_BUF (the maximum size that ESP32 can send)
      int sendAll (int sockfd, const char *buf, size_t len, unsigned long timeOut) {
        size_t sentTotal = 0;
        int sentThisTime;
        size_t toSendThisTime;
        unsigned long lastActive = millis ();
        while (true) {
          toSendThisTime = len - sentTotal; if (toSendThisTime > 1500) toSendThisTime = 1500; // TCP_SND_BUF = 5744 (max ESP32 can send in one row), but keep it in MAX MTU size (1500 bytes)
          switch ((sentThisTime = send (sockfd, buf + sentTotal, toSendThisTime, 0))) {
            case -1:
                if ((errno == EAGAIN || errno == ENAVAIL) && (millis () - lastActive < timeOut || !timeOut)) break; // not time-out yet
                else return - 1;
            case 0:   // connection closed by peer
                return 0;
            default:

              additionalNetworkInformation.bytesSent += sentThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].bytesSent += sentThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();

              sentTotal += sentThisTime;
              if (sentTotal == len) return sentTotal;
              lastActive = millis ();
              break;
          }
          delay (2); // reset the watchdog
        }
        return 0; // never executes
      }

      int sendAll (int sockfd, const char *buf, unsigned long timeOut) { return sendAll (sockfd, buf, strlen (buf), timeOut); }

      int recvAll (int sockfd, char *buf, size_t len, const char *endingString, unsigned long timeOut) {
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

              additionalNetworkInformation.bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].bytesReceived += receivedThisTime; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
              additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();

              receivedTotal += receivedThisTime;
              buf [receivedTotal] = 0; // close C string
              if (endingString) {
                  if (strstr (buf, endingString)) return receivedTotal; // check if whole request or reply has arrived (if there is an ending string in it)
                  if (receivedTotal >= len - 1) { // the endingStirng has not arrived yet and there is no place left in the buffer for it
                      #ifdef __DMESG__
                          dmesgQueue << "[network][recvAll] buffer too small";
                      #endif
                      cout << "[network][recvAll] buffer too small\n";
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
        cout << "[network] invalid IP address " << ipAddress << endl;
        return IPAddress (0, 42, 42, 42); // == 1073421048 - invalid address - first byte of class A can not be 0
      }
    }

    // converts binary MAC address into String
    cstring __mac_ntos__ (byte *mac, byte macLength) {
        cstring s;
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
            char staSSID [34] = DEFAULT_STA_SSID; // enough for max 32 characters for SSID
        #else
            char staSSID [34] = ""; // enough for max 32 characters for SSID
        #endif
        #ifdef DEFAULT_STA_PASSWORD
            char staPassword [65] = DEFAULT_STA_PASSWORD; // enough for max 63 characters for password
        #else
            char staPassword [65] = ""; // enough for max 63 characters for password
        #endif
        #ifdef DEFAULT_STA_IP
            char staIP [17] = DEFAULT_STA_IP; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #else
            char staIP [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #endif
        #ifdef DEFAULT_STA_SUBNET_MASK
            char staSubnetMask [17] = DEFAULT_STA_SUBNET_MASK; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #else
            char staSubnetMask [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #endif
        #ifdef DEFAULT_STA_GATEWAY
            char staGateway [17] = DEFAULT_STA_GATEWAY; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #else
            char staGateway [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #endif
        #ifdef DEFAULT_STA_DNS_1
            char staDns1 [17] = DEFAULT_STA_DNS_1; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #else
            char staDns1 [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #endif
        #ifdef DEFAULT_STA_DNS_2
            char staDns2 [17] = DEFAULT_STA_DNS_2; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #else
            char staDns2 [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters) 
        #endif
        #ifdef DEFAULT_AP_SSID
            char apSSID [34] = DEFAULT_AP_SSID; // enough for max 32 characters for SSID
        #else
            char apSSID [34] = ""; // enough for max 32 characters for SSID
        #endif
        #ifdef DEFAULT_AP_PASSWORD
            char apPassword [65] = DEFAULT_AP_PASSWORD; // enough for max 63 characters for password
        #else
            char apPassword [65] = ""; // enough for max 63 characters for password
        #endif
        #ifdef DEFAULT_AP_IP
            char apIP [17] = DEFAULT_AP_IP; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #else
            char apIP [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #endif
        #ifdef DEFAULT_AP_SUBNET_MASK
            char apSubnetMask [17] = DEFAULT_AP_SUBNET_MASK; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #else
            char apSubnetMask [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #endif
        #ifdef DEFAULT_AP_IP
            char apGateway [17] = DEFAULT_AP_IP; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #else
            char apGateway [17] = ""; // enough for IPv4 (max 15 characters, IPv6 has max 39 characters)
        #endif
    
        #ifdef __FILE_SYSTEM__
            if (fileSystem.mounted ()) { 
              // read interfaces configuration from /network/interfaces, create a new one if it doesn't exist
              if (!fileSystem.isFile ("/network/interfaces")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ("/network")) { fileSystem.makeDirectory ("/network"); }
                  cout << "[network] /network/interfaces does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ("/network/interfaces", "w");          
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
                  cout << (created ? "created\n" : "error\n");
              }
              {
                  cout << "[network] reading /network/interfaces ... ";
                  // /network/interfaces STA(tion) configuration - parse configuration file if it exists
                  char buffer [MAX_INTERFACES_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/network/interfaces")) {
                      strcat (buffer, "\n");
                      // cout << buffer;

                      if (stristr (buffer, "iface STA inet static")) {
                          *staIP = *staSubnetMask = *staGateway = *staDns1 = *staDns2 = 0;
                          char *p;                    
                          if ((p = stristr (buffer, "\naddress"))) sscanf (p + 8, "%*[ =]%16[0-9.]", staIP);
                          if ((p = stristr (buffer, "\nnetmask"))) sscanf (p + 8, "%*[ =]%16[0-9.]", staSubnetMask);
                          if ((p = stristr (buffer, "\ngateway"))) sscanf (p + 8, "%*[ =]%16[0-9.]", staGateway);
                          if ((p = stristr (buffer, "\ndns1"))) sscanf (p + 5, "%*[ =]%16[0-9.]", staDns1);
                          if ((p = stristr (buffer, "\ndns2"))) sscanf (p + 5, "%*[ =]%16[0-9.]", staDns2);

                          // cout << staIP << ", " << staSubnetMask << ", " << staGateway << ", " << staDns1 << ", " <<  staDns2 << endl;
                          cout << "OK, using static IP\n";
                      } else {
                          cout << "OK, using DHCP\n";
                      }
                  } else {
                      cout << "error\n";
                  } 
              }


              // read STAtion credentials from /etc/wpa_supplicant/wpa_supplicant.conf, create a new one if it doesn't exist
              if (!fileSystem.isFile ("/etc/wpa_supplicant/wpa_supplicant.conf")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ("/etc/wpa_supplicant")) { fileSystem.makeDirectory ("/etc"); fileSystem.makeDirectory ("/etc/wpa_supplicant"); }
                  cout << "[network] /etc/wpa_supplicant/wpa_supplicant.conf does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ("/etc/wpa_supplicant/wpa_supplicant.conf", "w");          
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
                  cout << (created ? "created\n" : "error\n");
              }
              {              
                  cout << "[network] reading /etc/wpa_supplicant/wpa_supplicant.conf ... ";
                  // /etc/wpa_supplicant/wpa_supplicant.conf STA(tion) credentials - parse configuration file if it exists
                  char buffer [MAX_WPA_SUPPLICANT_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/etc/wpa_supplicant/wpa_supplicant.conf")) {
                      strcat (buffer, "\n");                
                      // cout << buffer;

                      *staSSID = *staPassword = 0;
                      char *p;                    
                      if ((p = stristr (buffer, "\nssid"))) sscanf (p + 5, "%*[ =]%33[!-~]", staSSID);
                      if ((p = stristr (buffer, "\npsk"))) sscanf (p + 4, "%*[ =]%63[!-~]", staPassword);
                      
                      // cout << staSSID << ", " << staPassword << " ";
                      cout << "OK\n";
                  } else {
                      cout << "error\n";
                  }
              }


              // read A(ccess) P(oint) configuration from /etc/dhcpcd.conf, create a new one if it doesn't exist
              if (!fileSystem.isFile ("/etc/dhcpcd.conf")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ("/etc")) fileSystem.makeDirectory ("/etc");
                  cout << "[network] /etc/dhcpcd.conf does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ("/etc/dhcpcd.conf", "w");          
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
                  cout << (created ? "created\n" : "error\n");           
              }
              {              
                  cout << "[network] reading /etc/dhcpcd.conf ... ";
                  // /etc/dhcpcd.conf contains A(ccess) P(oint) configuration - parse configuration file if it exists
                  char buffer [MAX_DHCPCD_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/etc/dhcpcd.conf")) {
                      strcat (buffer, "\n");
                      // cout << buffer;

                      *apIP = *apSubnetMask = *apGateway = 0;
                      char *p;                    
                      if ((p = stristr (buffer, "\nstatic ip_address"))) sscanf (p + 18, "%*[ =]%16[0-9.]", apIP);
                      if ((p = stristr (buffer, "\nnetmask"))) sscanf (p + 8, "%*[ =]%16[0-9.]", apSubnetMask);
                      if ((p = stristr (buffer, "\ngateway"))) sscanf (p + 8, "%*[ =]%16[0-9.]", apGateway);
                      
                      // cout << apIP << ", " << apSubnetMask << ", " << apGateway << " ";
                      cout << "OK\n";
                  } else {
                      cout << "error\n";
                  }
              }


              // read A(ccess) P(oint) credentials from /etc/hostapd/hostapd.conf, create a new one if it doesn't exist
              if (!fileSystem.isFile ("/etc/hostapd/hostapd.conf")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ("/etc/hostapd")) { fileSystem.makeDirectory ("/etc"); fileSystem.makeDirectory ("/etc/hostapd"); }
                  cout << "[network] /etc/hostapd/hostapd.conf does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ("/etc/hostapd/hostapd.conf", "w");
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
                    cout << (created ? "created\n" : "error\n");           
                }
                {              
                    cout << "[network] reading /etc/hostapd/hostapd.conf ... ";
                    // /etc/hostapd/hostapd.conf contains A(ccess) P(oint) credentials - parse configuration file if it exists
                    char buffer [MAX_HOSTAPD_SIZE] = "\n";
                    if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/etc/hostapd/hostapd.conf")) {
                        strcat (buffer, "\n");
                        // cout << buffer;
                        
                        *apSSID = *apPassword = 0;
                        char *p;                    
                        if ((p = stristr (buffer, "\nssid"))) sscanf (p + 5, "%*[ =]%33[!-~]", apSSID);
                        if ((p = stristr (buffer, "\nwpa_passphrase"))) sscanf (p + 15, "%*[ =]%63[!-~]", apPassword);
                        
                        // cout << apSSID << ", " << apPassword << " ";
                        cout << "OK, " << apSSID << endl;
                    } else {
                        cout << "error\n";
                    }
                }

            } else 
        #endif    
            {
                cout << "[network] file system not mounted, can't read or write configuration files\n";
            }

    
        // network event logging: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
        WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {

            switch (event) {
                case ARDUINO_EVENT_WIFI_READY: 
                    cout << "[network][STA] interface ready\n";
                    break;
                case ARDUINO_EVENT_WIFI_SCAN_DONE:
                    cout << "[network][STA] completed scan for access points\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_START:
                    cout << "[network][STA] client started\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_STOP:
                    cout << "[network][STA] clients stopped\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][STA] connected to " << WiFi.SSID ().c_str ();
                    #endif
                    cout << "[network][STA] connected to " << WiFi.SSID ().c_str () << endl;
                    break;
                case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][STA] disconnected";
                    #endif
                    cout << "[network][STA] disconnected\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
                    cout << "[network][STA] authentication mode has changed\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][STA] got IP address " << WiFi.localIP ().toString ().c_str ();
                    #endif
                    cout << "[network][STA] got IP address " << WiFi.localIP ().toString ().c_str () << endl;
                    break;
                case ARDUINO_EVENT_WIFI_STA_LOST_IP:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][STA] lost IP address. IP address reset to 0.0.0.0";
                    #endif
                    cout << "[network][STA] lost IP address. IP address reset to 0.0.0.0\n";
                    break;
                case ARDUINO_EVENT_WPS_ER_SUCCESS:
                    cout << "[network][STA] WiFi Protected Setup (WPS) succeeded in enrollee mode\n";
                    break;
                case ARDUINO_EVENT_WPS_ER_FAILED:
                    cout << "[network][STA] WiFi Protected Setup (WPS) failed in enrollee mode\n";
                    break;
                case ARDUINO_EVENT_WPS_ER_TIMEOUT:
                    cout << "[network][STA] WiFi Protected Setup (WPS) timeout in enrollee mode\n";
                    break;
                case ARDUINO_EVENT_WPS_ER_PIN:
                    cout << "[network][STA] WiFi Protected Setup (WPS) pin code in enrollee mode\n";
                    break;

                case ARDUINO_EVENT_WIFI_AP_START:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][AP] access point started";
                    #endif
                    cout << "[network][AP] access point started\n";
                    // AP hostname can't be set until AP interface is mounted 
                    { 
                        /*
                        esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); // outdated, use: esp_netif_set_hostname
                        if (e != ESP_OK) {
                            #ifdef __DMESG__
                                dmesgQueue << "[network][AP] couldn't change AP adapter hostname"; 
                            #endif
                            cout << "[network][AP] couldn't change AP adapter hostname\n";
                        }
                        */
                    } 
                    break;
                case ARDUINO_EVENT_WIFI_AP_STOP:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][AP] access point stopped";
                    #endif
                    cout << "[network][AP] access point stopped\n";
                    break;
                case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                    // Serial.println("Client connected");
                    break;
                case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                    // Serial.println("Client disconnected");
                    break;
                case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
                    // Serial.println("Assigned IP address to client");
                    break;
                case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
                    // Serial.println("Received probe request");
                    break;

                case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
                    cout << "[network][AP] IPv6 is preferred\n";
                    break;
                case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
                    cout << "[network][STA] IPv6 is preferred\n";
                    break;
                case ARDUINO_EVENT_ETH_GOT_IP6:
                    cout << "[network][ETH] IPv6 is preferred\n";
                    break;

                case ARDUINO_EVENT_ETH_START:
                    cout << "[network][ETH] started\n";
                    break;
                case ARDUINO_EVENT_ETH_STOP:
                    cout << "[network][ETH] clients stopped\n";
                    break;
                case ARDUINO_EVENT_ETH_CONNECTED:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][ETH] connected";
                    #endif
                    cout << "[network][ETH] connected\n"; 
                    break;
                case ARDUINO_EVENT_ETH_DISCONNECTED:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][ETH] disconnected";
                    #endif
                    cout << "[network][ETH] disconnected\n";
                    break;
                case ARDUINO_EVENT_ETH_GOT_IP:
                    #ifdef __DMESG__
                        dmesgQueue << "[network][ETH] got IP address"; // TO DO: add IP
                    #endif
                    cout << "[network][ETH] got IP address\n"; // TO DO: add IP
                    break;
                default: break;
            }

        });    
    
        // set up STA 
        if (*staSSID) { 
            #ifdef __DMESG__
                dmesgQueue << "[network][STA] connecting to: " << staSSID;
            #endif
            cout << "[network][STA] connecting to: " << staSSID << endl;

            if (*staIP) { 
                // dmesg ("[network][STA] connecting STAtion to router with static IP: ", (char *) (String (staIP) + " GW: " + String (staGateway) + " MSK: " + String (staSubnetMask) + " DNS: " + String (staDns1) + ", " + String (staDns2)).c_str ());
                #ifdef __DMESG__
                    dmesgQueue << "[network][STA] using static IP: " << staIP;
                #endif
                cout << "[network][STA] using static IP: " << staIP << endl;
                WiFi.config (__IPAddressFromString__ (staIP), __IPAddressFromString__ (staGateway), __IPAddressFromString__ (staSubnetMask), *staDns1 ? __IPAddressFromString__ (staDns1) : IPAddress (255, 255, 255, 255), *staDns2 ? __IPAddressFromString__ (staDns2) : IPAddress (255, 255, 255, 255)); // INADDR_NONE == 255.255.255.255
            } else { 
                #ifdef __DMESG__
                    dmesgQueue << "[network][STA] is using DHCP";
                #endif
                cout << "[network][STA] is using DHCP\n";
            }
  
            WiFi.begin (staSSID, staPassword);
        }


        // set up AP
        if (*apSSID) {
            #ifdef __DMESG__
                dmesgQueue << "[network][AP] settint up access point: " << apSSID;
            #endif
            cout << "[network][AP] settint up access point: " << apSSID << endl;

            if (WiFi.softAP (apSSID, apPassword)) { 
                // dmesg ("[network] [AP] initializing access point: ", (char *) (String (apSSID) + "/" + String (apPassword) + ", " + String (apIP) + ", " + String (apGateway) + ", " + String (apSubnetMask)).c_str ()); 
                WiFi.softAPConfig (__IPAddressFromString__ (apIP), __IPAddressFromString__ (apGateway), __IPAddressFromString__ (apSubnetMask));
                WiFi.begin ();
                #ifdef __DMESG__
                    dmesgQueue << "[network][AP] access point IP: " << WiFi.softAPIP ().toString ().c_str ();
                #endif
                cout << "[network][AP] access point IP: " << WiFi.softAPIP ().toString ().c_str () << endl;
            } else {
                // ESP.restart ();
                #ifdef __DMESG__
                    dmesgQueue << "[network][AP] failed to initialize access point"; 
                #endif
                cout << "[network][AP] failed to initialize access point\n";
            }
        } 

        // set WiFi mode
        if (*staSSID)
            if (*apSSID)
                WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
            else
                WiFi.mode (WIFI_STA); // only STA mode
        else
            if (*apSSID)
                WiFi.mode (WIFI_AP); // only AP mode

        arp (-1); // call arp immediatelly after network is set up to obtain the pointer to ARP table    

        // rename hostname for all adapters
        esp_netif_t *netif = esp_netif_next_unsafe (NULL);
        while (netif) {
            if (esp_netif_set_hostname (netif, HOSTNAME) != ESP_OK) {
                #ifdef __DMESG__
                    dmesgQueue << "[network] couldn't change adapter's hostname";
                #endif
                cout << "[network] couldn't change adapter's hostname" << endl;
            }
            netif = esp_netif_next_unsafe (netif);
        }

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

    struct etharp_entry *arpTable = NULL;
  
    // returns output of arp (telnet) command
    cstring arp (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that arp can display intermediate results over TCP connection
        // get pointer to arp table the first time function is called
        if (!arpTable) {
            // check if the first entry is stable so we can obtain pointer to it, then we can calculate pointer to ARP table from it
            ip4_addr_t *ipaddr;
            struct netif *netif;
            struct eth_addr *mac;
            if (etharp_get_entry (0, &ipaddr, &netif, &mac)) {
                byte offset = (byte *) &arpTable->ipaddr - (byte *) arpTable;
                arpTable = (struct etharp_entry *) ((byte *) ipaddr - offset); // success
            }
        }
        if (!arpTable) return "ARP table is not accessible right now.";
    
        // scan through ARP table
        struct netif *netif;
        // ip4_addr_t *ipaddr;
        // scan ARP table it for each netif  
        cstring s;
        for (netif = netif_list; netif; netif = netif->next) {
            struct etharp_entry *p = arpTable; // start scan of ARP table from the beginning with the next netif
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
                if (arpTable) { // we have got a valid pointer to ARP table
                    for (int i = 0; i < ARP_TABLE_SIZE; i++) { // scan through ARP table
                        if (p->state != ETHARP_STATE_EMPTY) {
                            struct netif *arp_table_netif = p->netif; // make a copy of a pointer to netif in case arp_table entry is just beeing deleted
                            if (arp_table_netif && arp_table_netif->num == netif->num) { // if ARP entry is for the same as netif we are displaying
                                if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                                    sendAll (telnetSocket, s, 1000); 
                                    s = "";
                                }
                                s += "\r\n  ";
                                cstring ip_addr; inet_ntoa_r (p->ipaddr, ip_addr, sizeof (ip_addr));
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

    // returns IP address from ARP table for a given MAC address 
    cstring arpGetIpForMac (eth_addr ethaddr) {
        if (arpTable) {
            // scan through ARP table
            struct etharp_entry *p = arpTable; 
            for (int i = 0; i < ARP_TABLE_SIZE; i++) {
                if (p->state != ETHARP_STATE_EMPTY) {
                  if (*(unsigned long *) &ethaddr == *(unsigned long *) &p->ethaddr) {
                        char ip_addr [46]; 
                        inet_ntoa_r (p->ipaddr, ip_addr, sizeof (ip_addr));
                        return ip_addr;
                    }
                }
                p ++;
            } 
        }
        return "";
    }  

  
    //------ IW -----  
    cstring iw (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that iw can display intermediate results over TCP connection
        cstring s;
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
                s += netif->hostname; // no need to check if NULL, string will do this itself
                s += "\r\n        hwaddr: ";
                s += __mac_ntos__ (netif->hwaddr, netif->hwaddr_len);
                s += "\r\n        inet addr: ";
                s += ip_addr;
                s += "\r\n";
        
                // display the following information for STA interface
                if (!strcmp (ip_addr, WiFi.localIP ().toString ().c_str ())) {
                    if (WiFi.status () == WL_CONNECTED) {
                        if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                            sendAll (telnetSocket, s, 1000); // display intermediate result                
                            s = "";
                        }
                        int rssi = WiFi.RSSI ();
                        const char *rssiDescription = ""; if (rssi == 0) rssiDescription = "not available"; else if (rssi >= -30) rssiDescription = "excelent"; else if (rssi >= -67) rssiDescription = "very good"; else if (rssi >= -70) rssiDescription = "okay"; else if (rssi >= -80) rssiDescription = "not good"; else if (rssi >= -90) rssiDescription = "bad"; else /* if (rssi < -90) */ rssiDescription = "unusable";
                        s += "           STAtion is connected to router:\r\n\n"
                             "              inet addr: ";
                        s += WiFi.gatewayIP ().toString ().c_str ();
                        s += "     RSSI: ";
                        s += rssi;
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
                    esp_wifi_ap_get_sta_list (&wifi_sta_list);
                    if (wifi_sta_list.num) {
                        s += "           stations connected to Access Point (";
                        s += wifi_sta_list.num;
                        s += "):\r\n\n";

                        for (int i = 0; i < wifi_sta_list.num; i++) {
                            if (telnetSocket >= 0) { // display intermediate result if called from telnetServer for 350 string characters may not be enough to return the whole result
                                sendAll (telnetSocket, s, 1000); // display intermediate result           
                                s = "";
                            }
                            // IP
                            s += "              inet addr: ";
                            s += arpGetIpForMac (*(eth_addr *) wifi_sta_list.sta [i].mac); // TO DO: find a better way, scanning ARP table is not the best option since ARP entries only last for 20 minutes
                            // RSSI
                            int rssi = wifi_sta_list.sta [i].rssi;
                            const char *rssiDescription = ""; if (rssi == 0) rssiDescription = "not available"; else if (rssi >= -30) rssiDescription = "excelent"; else if (rssi >= -67) rssiDescription = "very good"; else if (rssi >= -70) rssiDescription = "okay"; else if (rssi >= -80) rssiDescription = "not good"; else if (rssi >= -90) rssiDescription = "bad"; else /* if (rssi < -90) */ rssiDescription = "unusable";
                            s += "     RSSI: ";
                            s += rssi;
                            s += " dBm (";
                            s += rssiDescription;
                            // MAC
                            s += ")     hwaddr: ";
                            s += __mac_ntos__ ((byte *) &(wifi_sta_list.sta [i].mac), NETIF_MAX_HWADDR_LEN);
                            s += "\r\n";
                        }
                    
                    } else {
                        s += "           there are no stations connected to Access Point\r\n";
                    } // ap
                } // adapter: ap, st or lo
            } // f netif is up
        } // for netif
        return s;
    }
  
  
    //------ IFCONFIG -----
  
    cstring ifconfig (int telnetSocket = -1) { // if called from telnetServer telnetSocket is > 0 so that ifconfig can display intermediate results over TCP connection
        cstring s;

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
                s += netif->hostname; // no need to check if NULL, string will do this itself
                s += "\r\n        hwaddr: ";
                s += __mac_ntos__ (netif->hwaddr, netif->hwaddr_len);
                s += "\r\n        inet addr: ";
                s += ip_addr;
                s += "\r\n        mtu: ";
                s += netif->mtu;
            }
        }
        return s;
    }

#endif
