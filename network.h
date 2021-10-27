/*
 
   network.h
  
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
  
    network.h reads network configuration from file system and sets both ESP32 network interfaces accordingly
 
    It is a little awkward why Unix, Linux are using so many network configuration files and how they are used):
  
      /network/interfaces                       - modify the code below with your IP addresses
      /etc/wpa_supplicant/wpa_supplicant.conf   - modify the code below with your WiFi SSID and password
      /etc/dhcpcd.conf                          - modify the code below with your access point IP addresses 
      /etc/hostapd/hostapd.conf                 - modify the code below with your access point SSID and password
  
    September, 9, 2021, Bojan Jurca
    
*/


#ifndef __NETWORK__
  #define __NETWORK__


  #include <WiFi.h>
  #include <lwip/netif.h>
  #include <netif/etharp.h>
  #include <lwip/sockets.h>
  #include "common_functions.h" // between, pad, inet_ntos, mac_ntos
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


  // converts doted (text) IP address into IPAddress structure
  IPAddress IPAddressFromString (String ipAddress) { 
    int ip1, ip2, ip3, ip4; 
    if (4 == sscanf (ipAddress.c_str (), "%i.%i.%i.%i", &ip1, &ip2, &ip3, &ip4)) {
      return IPAddress (ip1, ip2, ip3, ip4);
    } else {
      Serial.printf ("[network] invalid IP address %s\n", ipAddress.c_str ());
      return IPAddress (0, 42, 42, 42); // == 1073421048 - invalid address - first byte of class A can not be 0
    }
  }


  // returns output of arp (telnet) command
  String arp ();


  /*

     Support for telnet dmesg command. If telnetServer.hpp is included in the project __networkDmesg__ function will be redirected
     to message queue defined there and dmesg command will display its contetn. Otherwise it will just display message on the
     Serial console.
     
  */ 

  void __networkDmesg__ (String message) { 
    #ifdef __TELNET_SERVER__ 
      dmesg (message);                                              // use dmesg from telnet server if possible (if telnet server is in use)
    #else
      Serial.printf ("[%10lu] %s\n", millis (), message.c_str ());  // just print the message to serial console window otherwise
    #endif
  }
  void (* networkDmesg) (String) = __networkDmesg__;                // use this pointer to display / record system messages - it will be redirected to telnet dmesg function if telnet server will be included later

  #ifndef dmesg
    #define dmesg networkDmesg
  #endif


  void startWiFi () {                  // starts WiFi according to configuration files, creates configuration files if they don't exist
    // WiFi.disconnect (true);
    WiFi.mode (WIFI_OFF);

    // these parameters are needed to start ESP32 WiFi in different modes
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
    
        // /network/interfaces STA(tion) configuration
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
    
        // /etc/wpa_supplicant/wpa_supplicant.conf STA(tion) credentials
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
            dmesg ("[network] file system not mounted, can't read or write configuration files.");

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
                                                  // dmesg ("[network] WiFi interface ready.");
                                                  break;
          case SYSTEM_EVENT_SCAN_DONE:            dmesg ("[network] [STA] completed scan for access points.");
                                                  break;
          case SYSTEM_EVENT_STA_START:            if (!staStarted) {
                                                    staStarted = true;
                                                    dmesg ("[network] [STA] WiFi client started.");
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_STOP:             dmesg ("[network] [STA] WiFi clients stopped.");
                                                  break;
          case SYSTEM_EVENT_STA_CONNECTED:        dmesg ("[network] [STA] connected to WiFi " + WiFi.SSID () + ".");
                                                  break;
          case SYSTEM_EVENT_STA_DISCONNECTED:     if (staStarted) {
                                                    staStarted = false;
                                                    dmesg ("[network] [STA] disconnected from WiFi.");
                                                  }
                                                  break;
          case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:  dmesg ("[network] [STA] authentication mode has changed.");
                                                  break;
          case SYSTEM_EVENT_STA_GOT_IP:           dmesg ("[network] [STA] got IP address: " + WiFi.localIP ().toString ());
                                                  break;
          case SYSTEM_EVENT_STA_LOST_IP:          dmesg ("[network] [STA] lost IP address. IP address reset to 0");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:   dmesg ("[network] [STA] WiFi Protected Setup (WPS): succeeded in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_FAILED:    dmesg ("[network] [STA] WiFi Protected Setup (WPS): failed in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:   dmesg ("[network] [STA] WiFi Protected Setup (WPS): timeout in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_STA_WPS_ER_PIN:       dmesg ("[network] [STA] WiFi Protected Setup (WPS): pin code in enrollee mode.");
                                                  break;
          case SYSTEM_EVENT_AP_START:             dmesg ("[network] [AP] WiFi access point started.");
                                                  // AP hostname can't be set until AP interface is mounted 
                                                  { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) dmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
                                                  break;
          case SYSTEM_EVENT_AP_STOP:              dmesg ("[network] [AP] WiFi access point stopped.");
                                                  break;
          case SYSTEM_EVENT_AP_STACONNECTED:      // do not report this event - it is too frequent
                                                  // dmesg ("[network] [AP] client connected.");
                                                  break;
          case SYSTEM_EVENT_AP_STADISCONNECTED:   // do not report this event - it is too frequent
                                                  // dmesg ("[network] [AP] client disconnected.");
                                                  break;
          case SYSTEM_EVENT_AP_STAIPASSIGNED:     // do not report this event - it is too frequent
                                                  // dmesg ("[network] [AP] assigned IP address to client.");
                                                  break;
          case SYSTEM_EVENT_AP_PROBEREQRECVED:    dmesg ("[network] [AP] received probe request.");
                                                  break;
          case SYSTEM_EVENT_GOT_IP6:              dmesg ("[network] IPv6 is preferred.");
                                                  break;
          /*
          case SYSTEM_EVENT_ETH_START:            dmesg ("[network] ethernet started.");
                                                  break;
          case SYSTEM_EVENT_ETH_STOP:             dmesg ("[network] ethernet stopped.");
                                                  break;
          case SYSTEM_EVENT_ETH_CONNECTED:        dmesg ("[network] ethernet connected.");
                                                  break;
          case SYSTEM_EVENT_ETH_DISCONNECTED:     dmesg ("[network] ethernet disconnected.");
                                                  break;
          case SYSTEM_EVENT_ETH_GOT_IP:           dmesg ("[network] ethernet got IP address.");
                                                  break;        
          */
          default:                                dmesg ("[network] event: " + String (event)); // shouldn't happen
                                                  break;
      }
    });    

    dmesg ("[network] starting WiFi");
    // connect STA and AP 
    if (staSSID > "") { 

      if (staIP > "") { 
        dmesg ("[network] [STA] connecting STAtion to router with static IP: " + staIP + " GW: " + staGateway + " MSK: " + staSubnetMask + " DNS: " + staDns1 + ", " + staDns2);
        WiFi.config (IPAddressFromString (staIP), IPAddressFromString (staGateway), IPAddressFromString (staSubnetMask), staDns1 == "" ? IPAddress (255, 255, 255, 255) : IPAddressFromString (staDns1), staDns2 == "" ? IPAddress (255, 255, 255, 255) : IPAddressFromString (staDns2)); // INADDR_NONE == 255.255.255.255
      } else { 
        dmesg ("[network] [STA] connecting STAtion to router using DHCP.");
      }
      WiFi.begin (staSSID.c_str (), staPassword.c_str ());
      
    }    

    if (apSSID > "") { // setup AP

        if (WiFi.softAP (apSSID.c_str (), apPassword.c_str ())) { 
          dmesg ("[network] [AP] initializing access point: " + apSSID + "/" + apPassword + ", " + apIP + ", " + apGateway + ", " + apSubnetMask); 
          WiFi.softAPConfig (IPAddressFromString (apIP), IPAddressFromString (apGateway), IPAddressFromString (apSubnetMask));
          WiFi.begin ();
          dmesg ("[network] [AP] access point IP: " + WiFi.softAPIP ().toString ());
        } else {
          // ESP.restart ();
          dmesg ("[network] [AP] failed to initialize access point mode."); 
        }

        arp (); // call arp immediatelly after network setup to obtain pointer to ARP table
    } 

    // set WiFi mode
    if (staSSID > "") { 
      if (apSSID > "") {
      
        { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME); if (e != ESP_OK) dmesg ("[network] couldn't change STA adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // AP hostname can't be set until AP interface is mounted { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) dmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // WiFi.setHostname (HOSTNAME); // only for STA interface
        WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
      
      } else {
	
	      { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_STA, HOSTNAME); if (e != ESP_OK) dmesg ("[network] couldn't change STA adapter hostname."); } // outdated, use: esp_netif_set_hostname
        // WiFi.setHostname (HOSTNAME); // only for STA interface
        WiFi.mode (WIFI_STA); // only STA mode
        
      }
    } else {
      
      if (apSSID > "") {
        // AP hostname can't be set until AP interface is mounted { esp_err_t e = tcpip_adapter_set_hostname (TCPIP_ADAPTER_IF_AP, HOSTNAME); if (e != ESP_OK) dmesg ("[network] couldn't change AP adapter hostname."); } // outdated, use: esp_netif_set_hostname
        WiFi.mode (WIFI_AP); // only AP mode
      }
      
    }      
  }

  [[deprecated("Replaced by void startWiFi ();")]]
  void startNetworkAndInitializeItAtFirstCall () { startWiFi (); }

  wifi_mode_t getWiFiMode () {
    wifi_mode_t retVal = WIFI_OFF;
    if (esp_wifi_get_mode (&retVal) != ESP_OK) {;} // dmesg ("[network] couldn't get WiFi mode.");
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
  String arp () {
    // get pointer to arp table the first time function is called
    struct etharp_entry *arpTablePointer = NULL;
    if (!arpTablePointer) {
      // check if the first entry is stable so we can obtain pointer to it, then we can calculate pointer to ARP table from it
      ip4_addr_t *ipaddr;
      struct netif *netif;
      struct eth_addr *mac;
      if (etharp_get_entry (0, &ipaddr, &netif, &mac)) {
        // dmesg ("[network] [ARP] got ARP table address.");
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
                     mac_ntos ((byte *) &p->ethaddr, 6) +  
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

  //------ IW -----

  #include "TcpServer.hpp" // TcpConnection needed for displaying intermediate results over telnet sessions

  static SemaphoreHandle_t __WiFiSnifferSemaphore__ = xSemaphoreCreateMutex (); // to prevent two threads to start sniffing simultaneously
  static String __macToSniffRssiFor__;  // input to __sniffWiFiForRssi__ function
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

  int __sniffWiFiForRssi__ (String stationMac) { // sniff WiFi trafic for station RSSI - since we are sniffing connected stations we can stay on AP WiFi channel
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
                                                                                          if (__macToSniffRssiFor__ == mac_ntos ((byte *) hdr->addr2, 6)) __rssiSniffedForMac__ = ppkt->rx_ctrl.rssi;
                                                                                          return;
                                                                                        });
        unsigned long startTime = millis ();
        while (__rssiSniffedForMac__ == 0 && millis () - startTime < 5000) delay (1); // sniff max 5 second, it should be enough
        // Serial.printf ("RSSI obtained in %lu milliseconds\n", millis () - startTime);
        rssi = __rssiSniffedForMac__;
      esp_wifi_set_promiscuous (false);

    xSemaphoreGive (__WiFiSnifferSemaphore__);
    return rssi;
  }

  String iw (TcpConnection *connection = NULL) { // if called from telnetServer connection != NULL so that iw can dispely intermediate results over TCP connection
    String s = "";
    struct netif *netif;
    for (netif = netif_list; netif; netif = netif->next) {
      if (netif_is_up (netif)) {
        if (s != "") s += "\r\n";
        // display the following information for STA and AP interface (similar to ifconfig)
        s += String (netif->name [0]) + String (netif->name [1]) + "      hostname: " + (netif->hostname ? String (netif->hostname) : "") + "\r\n" +
             "        hwaddr: " + mac_ntos (netif->hwaddr, netif->hwaddr_len) + "\r\n" +
             "        inet addr: " + inet_ntos (netif->ip_addr) + "\r\n";

                // display the following information for STA interface
                if (inet_ntos (netif->ip_addr) == WiFi.localIP ().toString ()) {
                  if (WiFi.status () == WL_CONNECTED) {
                    int rssi = WiFi.RSSI ();
                    String rssiDescription = ""; if (rssi == 0) rssiDescription = "not available"; else if (rssi >= -30) rssiDescription = "excelent"; else if (rssi >= -67) rssiDescription = "very good"; else if (rssi >= -70) rssiDescription = "okay"; else if (rssi >= -80) rssiDescription = "not good"; else if (rssi >= -90) rssiDescription = "bad"; else /* if (rssi >= -90) */ rssiDescription = "unusable";
                    s += String ("           STAtion is connected to router:\r\n\r\n") + 
                                 "              inet addr: " + WiFi.gatewayIP ().toString () + "\r\n" +
                                 "              RSSI: " + String (rssi) + " dBm (" + rssiDescription + ")\r\n";
                  } else {
                    s += "           STAtion is disconnected from router\r\n";
                  }
                // display the following information for local loopback interface
                } else if (inet_ntos (netif->ip_addr) == "127.0.0.1") {
                    s += "           local loopback\r\n";
                // display the following information for AP interface
                } else {
                  wifi_sta_list_t wifi_sta_list = {};
                  tcpip_adapter_sta_list_t adapter_sta_list = {};
                  esp_wifi_ap_get_sta_list (&wifi_sta_list);
                  tcpip_adapter_get_sta_list (&wifi_sta_list, &adapter_sta_list);
                  if (adapter_sta_list.num) {
                    s += "           stations connected to Access Point (" + String (adapter_sta_list.num) + "):\r\n";
                    for (int i = 0; i < adapter_sta_list.num; i++) {
                      tcpip_adapter_sta_info_t station = adapter_sta_list.sta [i];
                      s += String ("\r\n") + 
                                   "              hwaddr: " + mac_ntos ((byte *) &station.mac, 6) + "\r\n" + 
                                   "              inet addr: " + inet_ntos (station.ip) + "\r\n";
                                   if (connection) { connection->sendData (s); s = ""; } // display intermediate result if called from telnetServer
                                   int rssi = __sniffWiFiForRssi__ (mac_ntos ((byte *) &station.mac, 6));
                                   String rssiDescription = ""; if (rssi == 0) rssiDescription = "not available"; else if (rssi >= -30) rssiDescription = "excelent"; else if (rssi >= -67) rssiDescription = "very good"; else if (rssi >= -70) rssiDescription = "okay"; else if (rssi >= -80) rssiDescription = "not good"; else if (rssi >= -90) rssiDescription = "bad"; else /* if (rssi >= -90) */ rssiDescription = "unusable";
                                   s = "              RSSI: " + String (rssi) + " dBm (" + rssiDescription + ")\r\n";
                    }
                  } else {
                    s += "           there are no stations connected to Access Point\r\n";
                  }
                }

      }
    }
    if (connection) { connection->sendData (s); s = ""; } // display intermediate result if called from telnetServer
    return s;
  }

  //------ IFCONFIG -----

  String ifconfig () {
    String s = "";
    struct netif *netif;
    for (netif = netif_list; netif; netif = netif->next) {
      if (netif_is_up (netif)) {
        if (s != "") s += "\r\n";
        s += String (netif->name [0]) + String (netif->name [1]) + "      hostname: " + (netif->hostname ? String (netif->hostname) : "") + "\r\n" + 
                 "        hwaddr: " + mac_ntos (netif->hwaddr, netif->hwaddr_len) + "\r\n" +
                 "        inet addr: " + inet_ntos (netif->ip_addr) + "\r\n" + 
                 "        mtu: " + String (netif->mtu) + "\r\n";
      }
    }
    return s;    
  }

  //------ PING: https://github.com/pbecchi/ESP32_ping -----

  #include "TcpServer.hpp" // TcpConnection needed for displaying intermediate results over telnet sessions

  #include "lwip/inet_chksum.h"
  #include "lwip/ip.h"
  #include "lwip/ip4.h"
  #include "lwip/err.h"
  #include "lwip/icmp.h"
  #include "lwip/sockets.h"
  #include "lwip/sys.h"
  #include "lwip/netdb.h"
  #include "lwip/dns.h"

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

  static bool __pingRecv__ (__pingDataStructure__ *pds, TcpConnection *telnetConnection, int s) {
    char buf [64];
    int fromlen, len;
    struct sockaddr_in from;
    struct ip_hdr *iphdr;
    struct icmp_echo_hdr *iecho = NULL;
    char ipa[16];
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
      if (len >= (int)(sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) {
        // Register end time
        gettimeofday (&end, NULL);
  
        /// Get from IP address
        ip4_addr_t fromaddr;
        fromaddr = *(ip4_addr_t *) &from.sin_addr; // inet_addr_to_ipaddr (&fromaddr, &from.sin_addr);
        strcpy (ipa, inet_ntos (fromaddr).c_str ()); 
  
        // Get echo
        iphdr = (struct ip_hdr *) buf;
        iecho = (struct icmp_echo_hdr *) (buf + (IPH_HL(iphdr) * 4));
  
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
          if (telnetConnection && !telnetConnection->sendData (cstr)) return false;
          
          return true;
        }
        else {
          // TODO: 
        }
      }
    }
  
    if (len < 0) {
      sprintf (cstr, "Request timeout for icmp_seq %d\r\n", pds->pingSeqNum);
      if (telnetConnection && !telnetConnection->sendData (cstr)) return false;
    }
    return false;
  }  

  uint32_t ping (String targetName, int pingCount = PING_DEFAULT_COUNT, int pingInterval = PING_DEFAULT_INTERVAL, int pingSize = PING_DEFAULT_SIZE, int timeOut = PING_DEFAULT_TIMEOUT, TcpConnection *telnetConnection = NULL) {
    // struct sockaddr_in address;
    ip4_addr_t pingTarget;
    int s;
    char cstr [256];

    // get target IP
    IPAddress targetIP;
    if (!WiFi.hostByName (targetName.c_str (), targetIP)) {
      if (telnetConnection) telnetConnection->sendData ("Could not find host " + targetName); 
      return 0;
    }
    
    // Create socket
    if ((s = socket (AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0) {
      if (telnetConnection) telnetConnection->sendData ( (char *) "Error creating socket."); 
      return 0;
    }
    
    pingTarget.addr = inet_addr (targetIP.toString ().c_str ()); 
    
    // Setup socket
    struct timeval tOut;
    
    // Timeout
    tOut.tv_sec = timeOut;
    tOut.tv_usec = 0;
    
    if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tOut, sizeof (tOut)) < 0) {
      closesocket (s);
      if (telnetConnection) telnetConnection->sendData ((char *) "Error setting socket options."); 
      return 0;
    }
    
    __pingDataStructure__ pds = {};
    pds.ID = random (0, 0xFFFF); // each consequently running ping command should have its own unique ID otherwise we won't be able to distinguish packets 
    pds.minTime = 1.E+9; // FLT_MAX;
    
    // Begin ping ...
    
    sprintf (cstr, "ping %s: %d data bytes\r\n",  targetIP.toString ().c_str (), pingSize);
    if (telnetConnection && !telnetConnection->sendData (cstr)) return 0;
    
    while ((pds.pingSeqNum < pingCount) && (!pds.stopped)) {
      if (__pingSend__ (&pds, s, &pingTarget, pingSize) == ERR_OK) if (!__pingRecv__ (&pds, telnetConnection, s)) return pds.received;
      delay (pingInterval * 1000L);
    }
    
    closesocket (s);
    
    sprintf (cstr, "\r\n%u packets transmitted, %u packets received, %.1f%% packet loss\r\n", pds.transmitted, pds.received, ((((float) pds.transmitted - (float) pds.received) / (float) pds.transmitted) * 100.0));
    if (pds.received) { // ok
      sprintf (cstr + strlen (cstr), "round-trip min / avg / max / stddev = %.3f / %.3f / %.3f / %.3f ms\r\n", pds.minTime, pds.meanTime, pds.maxTime, sqrt (pds.varTime / pds.received));
    } // else errd
    if (telnetConnection) telnetConnection->sendData (cstr);
    
    return pds.received;
  }

#endif
