/*
 * 
 * network.h
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 *  Network.h reads network configuration from file system and sets both ESP32 network interfaces accordingly
 *
 *  It is a little awkward why UNIX, LINUX, Raspbian are using so many network configuration files and how they are used):
 * 
 *    /network/interfaces                       - modify the code below with your IP addresses
 *    /etc/wpa_supplicant/wpa_supplicant.conf   - modify the code below with your WiFi SSID and password
 *    /etc/dhcpcd.conf                          - modify the code below with your access point IP addresses 
 *    /etc/hostapd/hostapd.conf                 - modify the code below with your access point SSID and password
 * 
 * History:
 *          - first release, 
 *            November 16, 2018, Bojan Jurca
 *          - added ifconfig, arp 
 *            December 9, 2018, Bojan Jurca
 *          - added iw
 *            December 11, 2018, Bojan Jurca
 *          - added fileSystemSemaphore to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876), 
 *            simplified installation
 *            April 13, 2019, Bojan Jurca
 *          - arp command improvement - now a pointer to arp table is obtained during initialization - more likely to be successful
 *            April 21, 2019, Bojan Jurca
 *          - added network event logging, 
 *            the use of dmesg 
 *            September 14, 2019, Bojan Jurca
 *          - putting wlan numbers in order,  
 *            automatic reconnection to router,
 *            bug fixes
 *            October 13, Bojan Jurca
 *          - simplifyed entry of default parameters,
 *            simplifyed format of configuration files 
 *            December 1, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 10, 2020, Bojan Jurca
 *          - added DNS servers to static STA IP configuration
 *            March 5, 2021, Bojan Jurca
 *            
 */


#ifndef __NETWORK__
  #define __NETWORK__

  #include <WiFi.h>
  #include <lwip/netif.h>
  #include <netif/etharp.h>
  #include <lwip/sockets.h>
  #include "common_functions.h" // between, pad
  #include "esp_wifi.h"
  #include "file_system.h"  // network.h needs file_system.h to read configurations files from


  // DEFINITIONS - change according to your needs

  // define host name
  #ifndef HOSTNAME
    #define HOSTNAME "MyESP32Server"   
  #endif
  // define default STAtion mode parameters to be written into /etc/wpa_supplicant/wpa_supplicant.conf if you want to use ESP as WiFi station
  #ifndef DEFAULT_STA_SSID
    #define DEFAULT_STA_SSID          "YOUR_STA_SSID"
  #endif
  #ifndef DEFAULT_STA_PASSWORD
    #define DEFAULT_STA_PASSWORD      "YOUR_STA_PASSWORD"
  #endif
  // define default static IP, subnet mask and gateway to be written into /network/interfaces if you want ESP to connect to WiFi with static IP instead of using DHCP
  /*
  #ifndef DEFAULT_STA_IP
    #define DEFAULT_STA_IP          "10.0.0.3" 
  #endif
  #ifndef DEFAULT_STA_SUBNET_MASK
    #define DEFAULT_STA_SUBNET_MASK "255.255.255.0"
  #endif
  #ifndef DEFAULT_STA_GATEWAY
    #define DEFAULT_STA_GATEWAY     "10.0.0.3"
  #endif
  #ifndef DEFAULT_STA_DNS_1
    #define DEFAULT_STA_DNS_1       "193.189.160.13" // or whatever your internet provider's DNS is
  #endif
  #ifndef DEFAULT_STA_DNS_2
    #define DEFAULT_STA_DNS_2       "193.189.160.23" // or whatever your internet provider's DNS is
  #endif  
  */
  // define default Access Point parameters to be written into /etc/hostapd/hostapd.conf if you want ESP to serve as an access point  
  #ifndef DEFAULT_AP_SSID
    #define DEFAULT_AP_SSID           HOSTNAME // change to what you want to name your AP SSID by default
  #endif
  #ifndef DEFAULT_AP_PASSWORD
    #define DEFAULT_AP_PASSWORD       "YOUR_AP_PASSWORD"
  #endif
  // define default access point IP and subnet mask to be written into /etc/dhcpcd.conf if you want to define them yourself
  #define DEFAULT_AP_IP           "10.0.1.3"
  #define DEFAULT_AP_SUBNET_MASK  "255.255.255.0"


  // FUNCTIONS OF THIS MODULE

  void startNetworkAndInitializeItAtFirstCall ();

  IPAddress IPAddressFromString (String ipAddress);
  
  String MacAddressAsString (byte *MacAddress, byte addressLength);
  
  String inet_ntos (ip_addr_t addr);
  
  String inet_ntos (ip4_addr_t addr); 
  
  String arp_a ();
  
  wifi_mode_t getWiFiMode ();
  

  // VARIABLES AND FUNCTIONS TO BE USED INSIDE THIS MODULE

  void __networkDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__ 
      dmesg (message);                                              // use dmesg from telnet server if possible (if telnet server is in use)
    #else
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ());  // just print the message to serial console window otherwise
    #endif
  }
  void (* networkDmesg) (String) = __networkDmesg__;                // use this pointer to display / record system messages - it will be redirected to telnet dmesg function if telnet server will be included later
  
  void startNetworkAndInitializeItAtFirstCall () {                  // starts WiFi according to configuration files, creates configuration files if they don't exist
    // WiFi.disconnect (true);
    WiFi.mode (WIFI_OFF);

    // these parameters are needed to start WiFi in different modes
    String staSSID = "";
    String staPassword = "";
    String staIP = "";
    String staSubnetMask = "";
    String staGateway = "";
    String staDns1 = "";
    String staDns2 = "";
    String apSSID = "";
    String apPassword = "";
    String apIP = "";
    String apSubnetMask = "";
    String apGateway = "";    

    #ifdef __FILE_SYSTEM__
      if (__fileSystemMounted__) { 
      
        String fileContent = "";
    
        // /network/interfaces contation STA(tion) configuration
        readFile (fileContent, "/network/interfaces");
        if (fileContent != "") { // parse configuration
    
          fileContent = compactConfigurationFileContent (fileContent) + "\n"; 
          int i = fileContent.indexOf ("iface STA inet static");
          if (i >= 0) {
            fileContent = fileContent.substring (i);
            staIP         = between (fileContent, "address ", "\n");
            staSubnetMask = between (fileContent, "netmask ", "\n");
            staGateway    = between (fileContent, "gateway ", "\n");
            staDns1       = between (fileContent, "dns1 ", "\n");
            staDns2       = between (fileContent, "dns2 ", "\n");
          }
          // Serial.printf ("[%10lu] [network] staIP         = %s\n", millis (), staIP.c_str ());
          // Serial.printf ("[%10lu] [network] staSubnetMask = %s\n", millis (), staSubnetMask.c_str ());
          // Serial.printf ("[%10lu] [network] staGateway    = %s\n", millis (), staGateway.c_str ());
          
        } else { // save default configuration
    
          Serial.printf ("[%10lu] [network] /network/interfaces does not exist or it is empty, creating a new one ... ", millis ());
          if (!isDirectory ("/network")) FFat.mkdir ("/network"); // location of this file
        
          fileContent = "# WiFi STA(tion) configuration - reboot for changes to take effect\r\n\r\n";
                         #if defined DEFAULT_STA_IP && defined DEFAULT_STA_SUBNET_MASK && defined DEFAULT_STA_GATEWAY && defined DEFAULT_STA_DNS_1 && defined DEFAULT_STA_DNS_2
                           fileContent += "# get IP address from DHCP\r\n"
                                          "#  iface STA inet dhcp\r\n"                  
                                          "\r\n"
                                          "# use static IP address\r\n"                   
                                          "   iface STA inet static\r\n"
                                          "      address " + (staIP = DEFAULT_STA_IP) + "\r\n"          
                                          "      netmask " + (staSubnetMask = DEFAULT_STA_SUBNET_MASK) + "\r\n" 
                                          "      gateway " + (staGateway = DEFAULT_STA_GATEWAY) + "\r\n"
                                          "      dns1 " + (staDns1 = DEFAULT_STA_DNS_1) + "\r\n"
                                          "      dns2 " + (staDns2 = DEFAULT_STA_DNS_2) + "\r\n";    
                         #else
                           fileContent += "# get IP address from DHCP\r\n"
                                          "   iface STA inet dhcp\r\n"                  
                                          "\r\n"
                                          "# use static IP address (example below)\r\n"   
                                          "#  iface STA inet static\r\n"
                                          "#     address 10.0.0.3\r\n"          
                                          "#     netmask 255.255.255.0\r\n" 
                                          "#     gateway 10.0.0.1\r\n"
                                          "#     dns1 \r\n"
                                          "#     dns2 \r\n";
                         #endif
          if (writeFile (fileContent, "/network/interfaces")) Serial.printf ("created.\n");
          else                                                Serial.printf ("error.\n");  
          
        }
    
        // /etc/wpa_supplicant/wpa_supplicant.conf contation STA(tion) credentials
        readFile (fileContent, "/etc/wpa_supplicant/wpa_supplicant.conf");
        if (fileContent != "") {
    
          // fileContent = between (compactConfigurationFileContent (fileContent), "network\n{", "}") + '\n'; 
          fileContent = compactConfigurationFileContent (fileContent); 
          fileContent = between (fileContent, "network", "}") + "}";
          fileContent = between (fileContent, "{", "}") + "\n";
          staSSID     = between (fileContent, "ssid ", "\n");
          staPassword = between (fileContent, "psk ", "\n");
          // Serial.printf ("[%10lu] [network] staSSID       = %s\n", millis (), staSSID.c_str ());      
          // Serial.printf ("[%10lu] [network] staPassword   = %s\n", millis (), staPassword.c_str ());      
      
        } else { // save default configuration
    
          Serial.printf ("[%10lu] [network] /etc/wpa_supplicant/wpa_supplicant.conf does not exist or it is empty, creating a new one ... ", millis ());
          if (!isDirectory ("/etc/wpa_supplicant")) { FFat.mkdir ("/etc"); FFat.mkdir ("/etc/wpa_supplicant"); } // location of this file
    
          fileContent = "# WiFi STA (station) credentials - reboot for changes to take effect\r\n\r\n"
                        "network = {\r\n"
                        "   ssid " 
                        #ifdef DEFAULT_STA_SSID
                           + (staSSID = DEFAULT_STA_SSID) +
                        #endif
                        "\r\n"               
                        "   psk "
                        #ifdef DEFAULT_STA_PASSWORD
                          + (staPassword = DEFAULT_STA_PASSWORD) +
                        #endif
                        "\r\n"           
                        "}\r\n";
    
          if (writeFile (fileContent, "/etc/wpa_supplicant/wpa_supplicant.conf")) Serial.printf ("created.\n");
          else                                                                    Serial.printf ("error.\n");  
          
        }
    
        // /etc/dhcpcd.conf contains A(ccess) P(oint) configuration
        readFile (fileContent, "/etc/dhcpcd.conf");
        if (fileContent != "") {
    
          fileContent = compactConfigurationFileContent (fileContent) + "\n"; 
          int i = fileContent.indexOf ("iface AP");
          if (i >= 0) {
            fileContent = fileContent.substring (i);
            apIP          = between (fileContent, "static ip_address ", "\n");
            apSubnetMask  = between (fileContent, "netmask ", "\n");
            apGateway     = between (fileContent, "gateway ", "\n");
          }
          // Serial.printf ("[%10lu] [network] apIP          = %s\n", millis (), apIP.c_str ());      
          // Serial.printf ("[%10lu] [network] apSubnetMask  = %s\n", millis (), apSubnetMask.c_str ());      
          // Serial.printf ("[%10lu] [network] apGateway     = %s\n", millis (), apGateway.c_str ());      
    
        } else { // save default configuration
          
          Serial.printf ("[%10lu] [network] /etc/dhcpcd.conf does not exist or it is empty, creating a new one ... ", millis ());
          
          fileContent =  "# WiFi AP configuration - reboot for changes to take effect\r\n\r\n"
                         "iface AP\r\n"
                         "   static ip_address "
                         #ifdef DEFAULT_AP_IP
                           + (apIP = DEFAULT_AP_IP) +
                         #endif
                         "\r\n"
                         "   netmask "
                         #ifdef DEFAULT_AP_SUBNET_MASK
                           + (apSubnetMask = DEFAULT_AP_SUBNET_MASK) +
                         #endif
                         "\r\n"
                         "   gateway "
                         #ifdef DEFAULT_AP_IP
                           + (apGateway = DEFAULT_AP_IP) +
                         #endif
                         "\r\n";
          
          if (writeFile (fileContent, "/etc/dhcpcd.conf")) Serial.printf ("created.\n");
          else                                             Serial.printf ("error.\n");  
          
        }
    
        // /etc/hostapd/hostapd.conf contains A(ccess) P(oint) credentials
        readFile (fileContent, "/etc/hostapd/hostapd.conf");
        if (fileContent != "") {
    
          fileContent = compactConfigurationFileContent (fileContent) + "\n"; 
          int i = fileContent.indexOf ("iface AP");
          if (i >= 0) {
            fileContent = fileContent.substring (i);
            apSSID        = between (fileContent, "ssid ", "\n");
            apPassword    = between (fileContent, "wpa_passphrase ", "\n");
          }
          // Serial.printf ("[%10lu] [network] apSSID       = %s\n", millis (), apSSID.c_str ());      
          // Serial.printf ("[%10lu] [network] apPassword   = %s\n", millis (), apPassword.c_str ());      
      
        } else { // save default configuration
    
          Serial.printf ("[%10lu] [network] /etc/hostapd/hostapd.conf does not exist or it is empty, creating a new one ... ", millis ());
          if (!isDirectory ("/etc/hostapd")) { FFat.mkdir ("/etc"); FFat.mkdir ("/etc/hostapd"); } // location of this file
          
          fileContent =  "# WiFi AP credentials - reboot for changes to take effect\r\n\r\n"
                         "iface AP\r\n"
                         "   ssid "                      
                         #ifdef DEFAULT_AP_SSID
                           + (apSSID = DEFAULT_AP_SSID) +
                         #endif
                         "\r\n"
                         "   # use at least 8 characters for wpa_passphrase\r\n"
                         "   wpa_passphrase "
                         #ifdef DEFAULT_AP_PASSWORD
                           + (apPassword = DEFAULT_AP_PASSWORD) +
                         #endif                
                         "\r\n";    
    
          if (writeFile (fileContent, "/etc/hostapd/hostapd.conf")) Serial.printf ("created.\n");
          else                                                      Serial.printf ("error.\n");  
          
        }
  
      } else 
    #endif    
          {
            networkDmesg ("[network] file system not mounted, can't read or write configuration files.");

            #ifdef DEFAULT_STA_SSID
              staSSID = DEFAULT_STA_SSID;
            #endif
            #ifdef DEFAULT_STA_PASSWORD
              staPassword = DEFAULT_STA_PASSWORD;
            #endif
            #ifdef DEFAULT_STA_IP
              staIP = DEFAULT_STA_IP;
            #endif
            #ifdef DEFAULT_STA_SUBNET_MASK
              staSubnetMask = DEFAULT_STA_SUBNET_MASK;
            #endif
            #ifdef DEFAULT_STA_GATEWAY
              staGateway = DEFAULT_STA_GATEWAY;
            #endif
            #ifdef DEFAULT_AP_SSID
              apSSID = DEFAULT_AP_SSID;
            #endif
            #ifdef DEFAULT_AP_PASSWORD
              apPassword = DEFAULT_AP_PASSWORD;
            #endif
            #ifdef DEFAULT_AP_IP
              apIP = apGateway = DEFAULT_AP_IP;
            #endif
            #ifdef DEFAULT_AP_SUBNET_MASK
              apSubnetMask = DEFAULT_AP_SUBNET_MASK;
            #endif
          }

    // network event logging - see: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEvents/WiFiClientEvents.ino
    WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {
      static bool staStarted = false; // to prevent unneccessary messages
      switch (event) {
          case SYSTEM_EVENT_WIFI_READY:           // do not report this event - it is too frequent
                                                  // networkDmesg ("[network] WiFi interface ready.");
                                                  break;
          case SYSTEM_EVENT_SCAN_DONE:            networkDmesg ("[network] [STA] completed scan for access points.");
                                                  break;
          case SYSTEM_EVENT_STA_START:            if (!staStarted) {
                                                    staStarted = true;
                                                    networkDmesg ("[network] [STA] WiFi client started.");
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_STOP:             networkDmesg ("[network] [STA] WiFi clients stopped.");
                                                  break;
          case SYSTEM_EVENT_STA_CONNECTED:        networkDmesg ("[network] [STA] connected to WiFi " + WiFi.SSID () + ".");
                                                  break;
          case SYSTEM_EVENT_STA_DISCONNECTED:     if (staStarted) {
                                                    staStarted = false;
                                                    networkDmesg ("[network] [STA] disconnected from WiFi.");
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  networkDmesg ("[network] [STA] authentication mode has changed.");
                                                  break;
          case SYSTEM_EVENT_STA_GOT_IP:           networkDmesg ("[network] [STA] got IP address: " + WiFi.localIP ().toString () + ".");
                                                  break;
          case SYSTEM_EVENT_STA_LOST_IP:          networkDmesg ("[network] [STA] lost IP address and IP address is reset to 0.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:   networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): succeeded in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_FAILED:    networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): failed in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:   networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): timeout in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_PIN:       networkDmesg ("[network] [STA] WiFi Protected Setup (WPS): pin code in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_AP_START:             networkDmesg ("[network] [AP] WiFi access point started.");
                                                  // AP hostname can't be set until AP interface is mounted 
                                                  { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) networkDmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
                                                  break;
          case SYSTEM_EVENT_AP_STOP:              networkDmesg ("[network] [AP] WiFi access point stopped.");
                                                  break;
          case SYSTEM_EVENT_AP_STACONNECTED:      // do not report this event - it is too frequent
                                                  // networkDmesg ("[network] [AP] client connected.");
                                                  break;
          case SYSTEM_EVENT_AP_STADISCONNECTED:   // do not report this event - it is too frequent
                                                  // networkDmesg ("[network] [AP] client disconnected.");
                                                  break;
          case SYSTEM_EVENT_AP_STAIPASSIGNED:     // do not report this event - it is too frequent
                                                  // networkDmesg ("[network] [AP] assigned IP address to client.");
                                                  break;
          case SYSTEM_EVENT_AP_PROBEREQRECVED:    networkDmesg ("[network] [AP] received probe request.");
                                                  break;
          case SYSTEM_EVENT_GOT_IP6:              networkDmesg ("[network] IPv6 is preferred.");
                                                  break;
          /*
          case SYSTEM_EVENT_ETH_START:            networkDmesg ("[network] ethernet started.");
                                                  break;
          case SYSTEM_EVENT_ETH_STOP:             networkDmesg ("[network] ethernet stopped.");
                                                  break;
          case SYSTEM_EVENT_ETH_CONNECTED:        networkDmesg ("[network] ethernet connected.");
                                                  break;
          case SYSTEM_EVENT_ETH_DISCONNECTED:     networkDmesg ("[network] ethernet disconnected.");
                                                  break;
          case SYSTEM_EVENT_ETH_GOT_IP:           networkDmesg ("[network] ethernet got IP address.");
                                                  break;        
          */
          default:                                networkDmesg ("[network] event: " + String (event)); // shouldn't happen
                                                  break;
      }
    });    

    networkDmesg ("[network] starting WiFi");
    // connect STA and AP 
    if (staSSID > "") { 

      if (staIP > "") { 
        networkDmesg ("[network] [STA] connecting STAtion to router with static IP: " + staIP + " GW: " + staGateway + " MSK: " + staSubnetMask + " DNS: " + staDns1 + ", " + staDns2);
        WiFi.config (IPAddressFromString (staIP), IPAddressFromString (staGateway), IPAddressFromString (staSubnetMask), staDns1 == "" ? IPAddress (255, 255, 255, 255) : IPAddressFromString (staDns1), staDns2 == "" ? IPAddress (255, 255, 255, 255) : IPAddressFromString (staDns2)); // INADDR_NONE == 255.255.255.255
      } else { 
        networkDmesg ("[network] [STA] connecting STAtion to router using DHCP.");
      }
      WiFi.begin (staSSID.c_str (), staPassword.c_str ());
      
    }    

    if (apSSID > "") { // setup AP

        if (WiFi.softAP (apSSID.c_str (), apPassword.c_str ())) { 
          networkDmesg ("[network] [AP] initializing access point: " + apSSID + "/" + apPassword + ", " + apIP + ", " + apGateway + ", " + apSubnetMask); 
          WiFi.softAPConfig (IPAddressFromString (apIP), IPAddressFromString (apGateway), IPAddressFromString (apSubnetMask));
          WiFi.begin ();
        } else {
          // ESP.restart ();
          networkDmesg ("[network] [AP] failed to initialize access point mode."); 
        }

        arp_a (); // call arp_a immediatelly after network setup to obtain pointer to ARP table
    } 

    // set WiFi mode
    if (staSSID > "") { 
      if (apSSID > "") {
      
        { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME); if (e != ESP_OK) networkDmesg ("[network] couldn't change STA adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // AP hostname can't be set until AP interface is mounted { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) networkDmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // WiFi.setHostname (HOSTNAME); // only for STA interface
        WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
      
      } else {
	
	      { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME); if (e != ESP_OK) networkDmesg ("[network] couldn't change STA adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // WiFi.setHostname (HOSTNAME); // only for STA interface
        WiFi.mode (WIFI_STA); // only STA mode
        
      }
    } else {
      
      if (apSSID > "") {
        // AP hostname can't be set until AP interface is mounted { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) networkDmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
        WiFi.mode (WIFI_AP); // only AP mode
      }
      
    }      
  }

  wifi_mode_t getWiFiMode () {
    wifi_mode_t retVal = WIFI_OFF;
    if (esp_wifi_get_mode (&retVal) != ESP_OK) {;} // networkDmesg ("[network] couldn't get WiFi mode.");
    return retVal;
  }
  
  String inet_ntos (ip_addr_t addr) { // equivalent of inet_ntoa (struct in_addr addr) 
                                      // inet_ntoa returns pointer to static string which may
                                      // be a problem in multi-threaded environment
    return String (*(((byte *) &addr) + 0)) + "." + 
           String (*(((byte *) &addr) + 1)) + "." + 
           String (*(((byte *) &addr) + 2)) + "." + 
           String (*(((byte *) &addr) + 3));
  }

  String inet_ntos (ip4_addr_t addr) { // equivalent of inet_ntoa (struct in_addr addr) 
                                       // inet_ntoa returns pointer to static string which may
                                       // be a problem in multi-threaded environment
    return String (*(((byte *) &addr) + 0)) + "." + 
           String (*(((byte *) &addr) + 1)) + "." + 
           String (*(((byte *) &addr) + 2)) + "." + 
           String (*(((byte *) &addr) + 3));
  }
  
  IPAddress IPAddressFromString (String ipAddress) { // converts dotted IP address into IPAddress structure
    int ip1, ip2, ip3, ip4; 
    if (4 == sscanf (ipAddress.c_str (), "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
      return IPAddress (ip1, ip2, ip3, ip4);
    } else {
      Serial.printf ("[network] invalid IP address %s\n", ipAddress.c_str ());
      return IPAddress (0, 42, 42, 42); // == 1073421048 - invalid address - first byte of class A can not be 0
    }
  }

  String MacAddressAsString (byte *MacAddress, byte addressLength) {
    String s = "";
    char c [3];
    for (byte i = 0; i < addressLength; i++) {
      sprintf (c, "%02x", *(MacAddress ++));
      s += String (c);
      if (i < 5) s += ":";
    }
    return s;
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

  String arp_a () {
    // get pointer to arp table the first time function is called
    struct etharp_entry *arpTablePointer = NULL;
    if (!arpTablePointer) {
      // check if the first entry is stable so we can obtain pointer to it, then we can calculate pointer to ARP table from it
      ip4_addr_t *ipaddr;
      struct netif *netif;
      struct eth_addr *mac;
      if (etharp_get_entry (0, &ipaddr, &netif, &mac)) {
        // networkDmesg ("[network] [ARP] got ARP table address.");
        byte offset = (byte *) &arpTablePointer->ipaddr - (byte *) arpTablePointer;
        arpTablePointer = (struct etharp_entry *) ((byte *) ipaddr - offset); // success
      }
    }
    if (!arpTablePointer) return "ARP table is not accessible right now.";

    // scan through ARP table
    struct netif *netif;
    // ip4_addr_t *ipaddr;
    // scan ARP table it for each netif  
    String s = "";
    for (netif = netif_list; netif; netif = netif->next) {
      struct etharp_entry *p = arpTablePointer; // start scan of ARP table from the beginning with the next netif
      if (netif_is_up (netif)) {
        if (s != "") s += "\r\n\r\n";
        s += String (netif->name [0]) + String (netif->name [1]) + ": " + inet_ntos (netif->ip_addr) + "\r\n  Internet Address      Physical Address      Type";
        if (arpTablePointer) { // we have got a valid pointer to ARP table
          for (int i = 0; i < ARP_TABLE_SIZE; i++) { // scan through ARP table
            if (p->state != ETHARP_STATE_EMPTY) {
              struct netif *arp_table_netif = p->netif; // make a copy of a pointer to netif in case arp_table entry is just beeing deleted
              if (arp_table_netif && arp_table_netif->num == netif->num) { // if ARP entry is for the same as netif we are displaying
                s += "\r\n  " + pad (inet_ntos (*(ip_addr_t *) &p->ipaddr), 22) +
                     MacAddressAsString ((byte *) &p->ethaddr, 6) +  
                     (p->state > ETHARP_STATE_STABLE_REREQUESTING_2 ? "     static" : "     dynamic");
              } 
            }
            p ++;
          } // scan through ARP table
        } // we have got a valid pointer to ARP table
      }
    }
    return s + "\r\n";
  }  

#endif
