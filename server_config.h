// Configure the servers you want to use in server_config.h before including their header files.


#ifndef __SERVER_CONFIG__
    #define __SERVER_CONFIG__


    /*
     * ----- CONFIGURATION PART -----
     *
     * Change the definitions to suit your project
     *
     */


    // ----- host name -----

    #define HOSTNAME "Oscilloscope"


    // ----- locale -----

    // #define LOCALE "en_150.UTF-8" // English with European dates and numbers; comment-out if not needed


    // ----- WiFi -----

        // ----- STA - define how ESP32 will connecto to WiFi router -----

            // ----- STA credentials -----

            // this information goes to /etc/wpa_supplicant.conf if file system is included
            // if DEFAULT_STA_SSID is left undefined STA will not be set up
            #define DEFAULT_STA_SSID                          "YOUR_STA_SSID"
            #define DEFAULT_STA_PASSWORD                      "YOUR_STA_PASSWORD"

            // ----- DHCP or static IP address -----

            // this information goes to /network/interfaces
            // if DEFAULT_STA_IP is left undefined DHCP will be used

            //                                     example:
            // #define DEFAULT_STA_IP              "172.16.0.6"
            #define DEFAULT_STA_SUBNET_MASK     "255.255.255.0"
            #define DEFAULT_STA_GATEWAY         "172.16.0.1"
            #define DEFAULT_STA_DNS_1           "193.189.160.13"
            #define DEFAULT_STA_DNS_2           "193.189.160.23"

        // ----- AP - define how ESP32 will serve as access point -----

            // ----- AP credentials -----

            // this information goes to /etc/hostapd.conf if file system is included
            // if DEFAULT_AP_SSID is left undefined AP will not be set up

            // #define DEFAULT_AP_SSID         HOSTNAME
            #define DEFAULT_AP_PASSWORD     "YOUR_AP_PASSWORD"

            // ----- AP network configuration -----

            // this information goes to /etc/dhcpcd.conf if file system is included

            //                              example:
            #define DEFAULT_AP_IP           "192.168.1.1"
            #define DEFAULT_AP_SUBNET_MASK  "255.255.255.0"


    #define MAX_WIFI_CONF_FILE_SIZE 512 // how much memory is needed to temporary store each WiFi configuration file 


    // ----- time -----

    // Define three NTP servers that will be queried for the current time.
    // This information will be written into /etc/ntp.conf (if file system is included).
    #define DEFAULT_NTP_SERVER_1 "1.si.pool.ntp.org"
    #define DEFAULT_NTP_SERVER_2 "2.si.pool.ntp.org"
    #define DEFAULT_NTP_SERVER_3 "3.si.pool.ntp.org"


    // ----- power saving -----

    // Define the default power‑saving mode, or leave undefined.
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html
    //
    // Power saving can significantly reduce the ESP32's operating temperature
    // (and overall power consumption), but it may cause the ESP32 to become
    // unresponsive to UDP packets (for example, during OTA updates).
    //
    // The workaround is to use Telnet to disable power saving before uploading
    // a new sketch via OTA.
    // #define POWER_SAVING WIFI_PS_MAX_MODEM // or WIFI_PS_MIN_MODEM; WIFI_PS_NONE makes no sense here


    // ----- mDNS -----

    // use mDNS so the servers can be accessed in local network by HOSTNAME, but uses some additional memory
    // mDNS may not work well with power saving mode
    // #define USE_mDNS // leave undefined to not use mDNS


#else


    /*
     * ----- IMPLEMENTATION PART -----
     */

    #include <lwip/inet.h>
    #include <esp_netif.h>


    // ----- WiFi network -----

    #ifndef DEFAULT_STA_SSID
        #define DEFAULT_STA_SSID ""
    #endif
    #ifndef DEFAULT_STA_PASSWORD
        #define DEFAULT_STA_PASSWORD ""
    #endif
    #ifndef DEFAULT_STA_IP
        #define DEFAULT_STA_IP ""
    #endif
    #ifndef DEFAULT_STA_SUBNET_MASK
        #define DEFAULT_STA_SUBNET_MASK ""
    #endif
    #ifndef DEFAULT_STA_GATEWAY
        #define DEFAULT_STA_GATEWAY ""
    #endif
    #ifndef DEFAULT_STA_DNS_1
        #define DEFAULT_STA_DNS_1 ""
    #endif
    #ifndef DEFAULT_STA_DNS_2
        #define DEFAULT_STA_DNS_2 ""
    #endif
    #ifndef DEFAULT_AP_SSID
        #define DEFAULT_AP_SSID ""
    #endif
    #ifndef DEFAULT_AP_PASSWORD
        #define DEFAULT_AP_PASSWORD ""
    #endif
    #ifndef DEFAULT_AP_IP
        #define DEFAULT_AP_IP ""
    #endif
    #ifndef DEFAULT_AP_SUBNET_MASK
        #define DEFAULT_AP_SUBNET_MASK ""
    #endif

    #ifdef __THREAD_SAFE_FS__

        void WiFi_start (threadSafeFS::FS& fileSystem) {
            // WiFi.disconnect (true);
            WiFi.mode (WIFI_OFF);

            // create default configuration files if they don't exist yet

            // ----- /etc/wpa_supplicant.conf -----
            if (!fileSystem.isFile ("/etc/wpa_supplicant.conf")) {
                if (!fileSystem.isDirectory ("/etc"))
                    fileSystem.mkdir ("/etc");
                #ifdef __DMESG__
                    dmesgQueue << "[WiFi] " "creating default /etc/wpa_supplicant.conf";
                #endif
                Serial.println ("[WiFi] " "creating default /etc/wpa_supplicant.conf");
                bool created = false;
                threadSafeFS::File f = fileSystem.open ("/etc/wpa_supplicant.conf", "w");
                if (f) {
                                created = f.print ("# WiFi STA credentials - reboot for changes to take effect\r\n\r\n"
                                                   "   ssid " DEFAULT_STA_SSID "\r\n"
                                                   "   psk  " DEFAULT_STA_PASSWORD "\r\n");
                    f.close ();
                }
                if (!created) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] " "default /etc/wpa_supplicant.conf could not be created";
                    #endif
                    Serial.println ("[WiFi] " "default /etc/wpa_supplicant.conf could not be created");
                }
            }

            // ----- /network/interfaces -----
            if (!fileSystem.isFile ("/network/interfaces")) {
                if (!fileSystem.isDirectory ("/network"))
                    fileSystem.mkdir ("/network");
                #ifdef __DMESG__
                    dmesgQueue << "[WiFi] " "creating default /network/interfaces";
                #endif
                Serial.println ("[WiFi] " "creating default /network/interfaces");
                bool created = false;
                threadSafeFS::File f = fileSystem.open ("/network/interfaces", "w");
                if (f) {
                                     created = f.print ("# WiFi STA configuration - reboot for changes to take effect\r\n\r\n"
                                                        "# get IPv4 address from DHCP\r\n");
                    constexpr const char* default_sta_ip = DEFAULT_STA_IP; 
                    if constexpr (default_sta_ip [0] != '\0') {
                        if (created) created = f.print ("#  iface STA inet dhcp\r\n\r\n"
                                                        "# use static IPv4 address (the example is below)\r\n"
                                                        "   iface STA inet static\r\n");
                    } else {
                        if (created) created = f.print ("  iface STA inet dhcp\r\n\r\n"
                                                        "# use static IPv4 address (the example is below)\r\n"
                                                        "#  iface STA inet static\r\n");
                    }
                        if (created) created = f.print ("   address " DEFAULT_STA_IP "\r\n"
                                                        "   netmask " DEFAULT_STA_SUBNET_MASK "\r\n"
                                                        "   gateway " DEFAULT_STA_GATEWAY "\r\n"
                                                        "   dns1    " DEFAULT_STA_DNS_1 "\r\n"
                                                        "   dns2    " DEFAULT_STA_DNS_2 "\r\n");
                    f.close ();
                }
                if (!created) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] " "default /network/interfaces could not be created";
                    #endif
                    Serial.println ("[WiFi] " "default /network/interfaces could not be created");
                }
            }

            // ----- /etc/hostapd.conf -----
            if (!fileSystem.isFile ("/etc/hostapd.conf")) {
                // if (!fileSystem.isDirectory ("/etc"))
                //     fileSystem.mkdir ("/etc");
                #ifdef __DMESG__
                    dmesgQueue << "[WiFi] " "creating default /etc/hostapd.conf";
                #endif
                Serial.println ("[WiFi] " "creating default /etc/hostapd.conf");
                bool created = false;
                threadSafeFS::File f = fileSystem.open ("/etc/hostapd.conf", "w");
                if (f) {
                                created = f.print ("# WiFi AP credentials - reboot for changes to take effect\r\n\r\n"
                                                   "   ssid " DEFAULT_AP_SSID "\r\n"
                                                   "# use at least 8 characters for wpa_passphrase\r\n"
                                                   "   wpa_passphrase " DEFAULT_AP_PASSWORD "\r\n");
                    f.close ();
                }
                if (!created) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] " "default /etc/hostapd.conf could not be created";
                    #endif
                    Serial.println ("[WiFi] " "default /etc/hostapd.conf could not be created");
                }
            }

            // ----- /etc/dhcpcd.conf -----
            if (!fileSystem.isFile ("/etc/dhcpcd.conf")) {
                // if (!fileSystem.isDirectory ("/etc"))
                //     fileSystem.mkdir ("/etc");
                #ifdef __DMESG__
                    dmesgQueue << "[WiFi] " "creating default /etc/dhcpcd.conf";
                #endif
                Serial.println ("[WiFi] " "creating default /etc/dhcpcd.conf");
                bool created = false;
                threadSafeFS::File f = fileSystem.open ("/etc/dhcpcd.conf", "w");
                if (f) {
                                created = f.print ("# WiFi AP configuration - reboot for changes to take effect\r\n\r\n"
                                                   "   static ip_address " DEFAULT_AP_IP "\r\n"
                                                   "   netmask " DEFAULT_AP_SUBNET_MASK "\r\n"
                                                   "   gateway " DEFAULT_AP_IP "\r\n");
                    f.close ();
                }
                if (!created) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] " "default /etc/dhcpcd.conf could not be created";
                    #endif
                    Serial.println ("[WiFi] " "default /etc/dhcpcd.conf could not be created");
                }
            }


            // read configuration files

            char STA_SSID [34] = ""; // enough for max 32 characters for SSID
            char STA_PASSWORD [65] = ""; // enough for max 63 characters for password
            bool STA_DHCP = false;
            char STA_IP [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters) 
            char STA_SUBNET_MASK [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters) 
            char STA_GATEWAY [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters) 
            char STA_DNS_1 [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters) 
            char STA_DNS_2 [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters) 
            char AP_SSID [34] = ""; // enough for max 32 characters for SSID
            char AP_PASSWORD [65] = ""; // enough for max 63 characters for password
            char AP_IP [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters)
            char AP_SUBNET_MASK [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters)
            char AP_GATEWAY [INET_ADDRSTRLEN] = ""; // enough for IPv4 (max 15 characters)
            char buffer [MAX_WIFI_CONF_FILE_SIZE] = "\n";


            // ----- /etc/wpa_supplicant.conf -----

            // #ifdef __DMESG__
            //     dmesgQueue << "[WiFi] reading /etc/wpa_supplicant.conf ... ";
            // #endif
            // Serial.println ("[WiFi] reading /etc/wpa_supplicant.conf ... ");
            if (fileSystem.readConfiguration (buffer + 1, sizeof (buffer) - 3, "/etc/wpa_supplicant.conf")) {
                strcat (buffer, "\n");
                char *p;                    
                if ((p = strstr (buffer, "\nssid"))) sscanf (p + 5, "%*[ =]%33[^\n]", STA_SSID);
                for (int i = strlen (STA_SSID) - 1; i && STA_SSID [i] <= ' '; i--) STA_SSID [i] = 0; // right-trim
                if ((p = strstr (buffer, "\npsk"))) sscanf (p + 4, "%*[ =]%63[^\n]", STA_PASSWORD);
                for (int i = strlen (STA_PASSWORD) - 1; i && STA_PASSWORD [i] <= ' '; i--) STA_PASSWORD [i] = 0; // right-trim
            } else {
                Serial.println ("[WiFi] error reading /etc/wpa_supplicant.conf");
            }


            // ----- /network/interfaces -----

            // #ifdef __DMESG__
            //     dmesgQueue << "[WiFi] reading /network/interfaces ... ";
            // #endif
            // Serial.println ("[WiFi] reading /network/interfaces ... ");            
            if (fileSystem.readConfiguration(buffer + 1, sizeof (buffer) - 3, "/network/interfaces")) {
                strcat (buffer, "\n");
                char *p4 = strstr (buffer, "\niface STA inet static");
                if (p4) {
                    // static IPv4 configuration
                    char *p;                    
                    if ((p = strstr (p4, "\naddress"))) sscanf (p + 8, "%*[ =]%16[0-9.]", STA_IP);
                    if ((p = strstr (p4, "\nnetmask"))) sscanf (p + 8, "%*[ =]%16[0-9.]", STA_SUBNET_MASK);
                    if ((p = strstr (p4, "\ngateway"))) sscanf (p + 8, "%*[ =]%16[0-9.]", STA_GATEWAY);
                    if ((p = strstr (p4, "\ndns1")))    sscanf (p + 5, "%*[ =]%16[0-9.]", STA_DNS_1);
                    if ((p = strstr (p4, "\ndns2")))    sscanf (p + 5, "%*[ =]%16[0-9.]", STA_DNS_2);
                } else {
                    // IPv4 configuration with DHCP
                    STA_DHCP = true;
                }
            } else {
                Serial.println ("[WiFi] error reading /network/interfaces");
            } 


            // ----- /etc/hostapd.conf -----

            // #ifdef __DMESG__
            //     dmesgQueue << "[WiFi] reading /etc/hostapd.conf ... ";
            // #endif
            // Serial.println ("[WiFi] reading /etc/hostapd.conf ... ");  
            if (fileSystem.readConfiguration (buffer + 1, sizeof (buffer) - 3, "/etc/hostapd.conf")) {
                strcat (buffer, "\n");
                char *p;
                if ((p = strstr (buffer, "\nssid"))) sscanf (p + 5, "%*[ =]%33[^\n]", AP_SSID);
                for (int i = strlen (AP_SSID) - 1; i && AP_SSID [i] <= ' '; i--) AP_SSID [i] = 0; // right-trim
                if ((p = strstr (buffer, "\nwpa_passphrase"))) sscanf (p + 15, "%*[ =]%63[^\n]", AP_PASSWORD);
                for (int i = strlen (AP_PASSWORD) - 1; i && AP_PASSWORD [i] <= ' '; i--) AP_PASSWORD [i] = 0; // right-trim
            } else {
                Serial.println ("[WiFi] error reading /etc/hostapd.conf");
            }


            // ----- /etc/dhcpcd.conf -----

            // #ifdef __DMESG__
            //     dmesgQueue << "[WiFi] reading /etc/dhcpcd.conf ... ";
            // #endif
            // Serial.println ("[WiFi] reading /etc/dhcpcd.conf ... ");  
            if (fileSystem.readConfiguration (buffer + 1, sizeof (buffer) - 3, "/etc/dhcpcd.conf")) {
                strcat (buffer, "\n");
                char *p4 = strstr (buffer, "\nstatic ip_address");
                if (p4) {
                    // static IPv4 configuration
                    char *p;                    
                    if ((p = strstr (p4, "\nnetmask"))) sscanf (p + 8, "%*[ =]%16[0-9.]", AP_SUBNET_MASK);
                    if ((p = strstr (p4, "\ngateway"))) sscanf (p + 8, "%*[ =]%16[0-9.]", AP_GATEWAY);
                }
            } else {
                Serial.println ("[WiFi] error reading /etc/dhcpcd.conf");
            }


            if (STA_SSID [0] != '\0') {
                // set up STA

                #ifdef __DMESG__
                    dmesgQueue << "[WiFi][STA] connecting to: " << STA_SSID;
                #endif
                Serial.printf ("[WiFi][STA] connecting to: %s\n", STA_SSID);

                if (!STA_DHCP) {
                    // set up static IPv4 address

                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][STA] using static IPv4 address: " << STA_IP;
                    #endif
                    Serial.printf ("[WiFi][STA] using static IPv4 address: %s\n",  STA_IP);

                    WiFi.config (IPAddress (STA_IP), IPAddress (STA_GATEWAY), IPAddress (STA_SUBNET_MASK), IPAddress (STA_DNS_1), IPAddress (STA_DNS_2));
                } else {
                    // get IPv4 address form router's DHCP

                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][STA] is using DHCP for IPv4";
                    #endif
                    Serial.println ("[WiFi][STA] is using DHCP for IPv4");
                }

                WiFi.begin (STA_SSID, STA_PASSWORD);
            }

            // set up AP
            if (AP_SSID [0] != '\0') {
                // set up AP

                #ifdef __DMESG__
                    dmesgQueue << "[WiFi][AP] setting up access point: " << AP_SSID;
                #endif
                Serial.printf ("[WiFi][AP] setting up access point: %s\n", AP_SSID);

                if (WiFi.softAP (AP_SSID, AP_PASSWORD)) { 
                    // configure IP addresses

                    WiFi.softAPConfig (IPAddress (AP_IP), IPAddress (AP_IP), IPAddress (AP_SUBNET_MASK));
                    WiFi.begin ();
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][AP] static IPv4 address: " << WiFi.softAPIP ();
                    #endif
                    Serial.printf ("[WiFi][AP] static IPv4 address: %s\n", WiFi.softAPIP ().toString ().c_str ());
                } else {
                    // ESP.restart ();
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][AP] failed to initialize access point"; 
                    #endif
                    Serial.println ("[WiFi][AP] failed to initialize access point");
                }
            } 

            // set WiFi mode
            if (STA_SSID [0] != '\0') {
                if (AP_SSID [0] != '\0')
                    WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
                else
                    WiFi.mode (WIFI_STA); // STA mode only
            } else {
                if (AP_SSID [0] != '\0')
                    WiFi.mode (WIFI_AP); // AP mode only
            }

            // rename hostname for all adapters
            esp_netif_t *netif = esp_netif_next_unsafe (NULL);
            while (netif) {
                if (esp_netif_set_hostname (netif, HOSTNAME) != ESP_OK) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] couldn't change adapter's hostname";
                    #endif
                    Serial.println ("[WiFi] couldn't change adapter's hostname");
                }
                netif = esp_netif_next_unsafe (netif);
            }

        }

    #endif

        void WiFi_start () {
            // WiFi.disconnect (true);
            WiFi.mode (WIFI_OFF);
        
            constexpr const char *default_sta_ssid = DEFAULT_STA_SSID; 
            constexpr const char *default_ap_ssid = DEFAULT_AP_SSID; 

            if constexpr (default_sta_ssid [0] != '\0') {
                // set up STA

                #ifdef __DMESG__
                    dmesgQueue << "[WiFi][STA] connecting to: " DEFAULT_STA_SSID;
                #endif
                Serial.println ("[WiFi][STA] connecting to: " DEFAULT_STA_SSID);

                if constexpr (DEFAULT_STA_IP[0] != '\0') {
                    // set up static IPv4 address

                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][STA] using static IPv4 address: " DEFAULT_STA_IP;
                    #endif
                    Serial.println ("[WiFi][STA] using static IPv4 address: " DEFAULT_STA_IP);

                    WiFi.config (IPAddress (DEFAULT_STA_IP), IPAddress (DEFAULT_STA_GATEWAY), IPAddress (DEFAULT_STA_SUBNET_MASK), IPAddress (DEFAULT_STA_DNS_1), IPAddress (DEFAULT_STA_DNS_2));
                } else {
                    // get IPv4 address form router's DHCP

                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][STA] is using DHCP for IPv4";
                    #endif
                    Serial.println ("[WiFi][STA] is using DHCP for IPv4");
                }

                WiFi.begin (DEFAULT_STA_SSID, DEFAULT_STA_PASSWORD);
            }

            // set up AP
            if (default_ap_ssid [0] != '\0') {
                // set up AP

                #ifdef __DMESG__
                    dmesgQueue << "[WiFi][AP] setting up access point: " DEFAULT_AP_SSID;
                #endif
                Serial.println ("[WiFi][AP] setting up access point: " DEFAULT_AP_SSID);

                if (WiFi.softAP (DEFAULT_AP_SSID, DEFAULT_AP_PASSWORD)) { 
                    // configure IP addresses

                    WiFi.softAPConfig (IPAddress (DEFAULT_AP_IP), IPAddress (DEFAULT_AP_IP), IPAddress (DEFAULT_AP_SUBNET_MASK));
                    WiFi.begin ();
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][AP] static IPv4 address: " << WiFi.softAPIP ();
                    #endif
                    Serial.printf ("[WiFi][AP] static IPv4 address: %s\n", WiFi.softAPIP ().toString ().c_str ());
                } else {
                    // ESP.restart ();
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi][AP] failed to initialize access point"; 
                    #endif
                    Serial.println ("[WiFi][AP] failed to initialize access point");
                }
            } 

            // set WiFi mode
            if (default_sta_ssid [0] != '\0') {
                if (default_ap_ssid [0] != '\0')
                    WiFi.mode (WIFI_AP_STA); // both, AP and STA modes
                else
                    WiFi.mode (WIFI_STA); // STA mode only
            } else {
                if (default_ap_ssid [0] != '\0')
                    WiFi.mode (WIFI_AP); // AP mode only
            }

            // rename hostname for all adapters
            esp_netif_t *netif = esp_netif_next_unsafe (NULL);
            while (netif) {
                if (esp_netif_set_hostname (netif, HOSTNAME) != ESP_OK) {
                    #ifdef __DMESG__
                        dmesgQueue << "[WiFi] couldn't change adapter's hostname";
                    #endif
                    Serial.println ("[WiFi] couldn't change adapter's hostname");
                }
                netif = esp_netif_next_unsafe (netif);
            }

        }


    // ----- zoneinfo -----

    #ifdef __THREAD_SAFE_FS__

        Cstring<300> zoneinfo (threadSafeFS::FS& fileSystem, const char *defaultTZ) {
            // create /usr/share/zoneinfo file if it doesn't exist
            if (!fileSystem.isFile ("/usr/share/zoneinfo")) {
                if (!fileSystem.isDirectory ("/usr"))
                    fileSystem.mkdir ("/usr");
                if (!fileSystem.isDirectory ("/usr/share"))
                    fileSystem.mkdir ("/usr/share");
                #ifdef __DMESG__
                    dmesgQueue << "[TZ] " "creating default /usr/share/zoneinfo";
                #endif
                Serial.println ("[TZ] " "creating default /usr/share/zoneinfo");
                bool created = false;
                threadSafeFS::File f = fileSystem.open ("/usr/share/zoneinfo", "w");
                if (f) {
                                created = f.print ("# timezone configuration - reboot for changes to take effect\r\n"
                                                    "# choose one of available (POSIX) timezones: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv\r\n\r\n");
                    if (created) created = f.print (defaultTZ ? defaultTZ : "");
                    if (created) created = f.print ("\r\n");
                    f.close ();
                }
                if (!created) {
                    #ifdef __DMESG__
                        dmesgQueue << "[TZ] " "default /usr/share/zoneinfo could not be created";
                    #endif
                    Serial.println ("[TZ] " "default /usr/share/zoneinfo could not be created");
                }
            }

            // read /usr/share/zoneinfo file
            char buffer [300] = "";
            if (fileSystem.readConfiguration (buffer, sizeof (buffer) - 3, "/usr/share/zoneinfo")) {
                strcat (buffer, "\n");
                *(strstr (buffer, "\n")) = 0;
                return buffer;
            } else {
                #ifdef __DMESG__
                    dmesgQueue << "[TZ] " "error reading /usr/share/zoneinfo";
                #endif
                Serial.println ("[TZ] " "error reading /usr/share/zoneinfo");
            }
            return "";
        }

    #endif

        Cstring<300> zoneinfo (const char *defaultTZ) {
            return defaultTZ;
        }

#endif
