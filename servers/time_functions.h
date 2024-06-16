  /*

    time_functions.h
 
    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino

    Cron daemon synchronizes the time with NTP server accessible from internet once a day.

    May 22, 2024, Bojan Jurca

    Nomenclature used in time_functions.h - for easier understaning of the code:

      - "buffer size" is the number of bytes that can be placed in a buffer. 

                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".      

      - "max length" is the number of characters that can be placed in a variable.

                  In case of C 0-terminated strings the terminating 0 character is not included so the buffer should be at least 1 byte larger.    
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <lwip/netdb.h>
    #include <time.h>
    #include <lwip/netdb.h>
    #include "std/Cstring.hpp"
    #include "std/console.hpp"
    #include "netwk.h"


#ifndef __TIME_FUNCTIONS__
    #define __TIME_FUNCTIONS__

    #ifndef TZ
        #ifdef TIMEZONE        
            #define TZ TIMEZONE
        #else
            #ifdef SHOW_COMPILE_TIME_INFORMATION
                #pragma message "TZ was not defined previously, #defining the default CET-1CEST,M3.5.0,M10.5.0/3 in time_functions.h"
            #endif
            #define TZ "CET-1CEST,M3.5.0,M10.5.0/3" // default: Europe/Ljubljana, or select another (POSIX) time zones: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
        #endif
    #endif

    #ifndef __FILE_SYSTEM__
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "Compiling time_functions.h without file system (fileSystem.hpp), time_functions.h will not use configuration files"
        #endif
    #endif


    // ----- functions and variables in this modul -----

    time_t time ();
    void setTimeOfDay (time_t);
    struct tm gmTime (time_t);
    struct tm localTime (time_t);
    char *ascTime (const struct tm, char *);
    time_t getUptime ();
    char *ntpDate (const char *);
    char *ntpDate ();
    bool cronTabAdd (uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, const char *, bool);
    bool cronTabAdd (const char *, bool);
    int cronTabDel (const char *);
    void startCronDaemon (void (* cronHandler) (const char *), size_t);


    // TUNNING PARAMETERS

    // NTP servers that ESP32 server is going to sinchronize its time with
    #ifndef DEFAULT_NTP_SERVER_1
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_NTP_SERVER_1 was not defined previously, #defining the default 1.si.pool.ntp.org in time_finctions.h"
        #endif
        #define DEFAULT_NTP_SERVER_1    "1.si.pool.ntp.org"
    #endif
    #ifndef DEFAULT_NTP_SERVER_2
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_NTP_SERVER_2 was not defined previously, #defining the default 2.si.pool.ntp.org in time_finctions.h"
        #endif
      #define DEFAULT_NTP_SERVER_2      "2.si.pool.ntp.org"
    #endif
    #ifndef DEFAULT_NTP_SERVER_3
        #ifdef SHOW_COMPILE_TIME_INFORMATION
            #pragma message "DEFAULT_NTP_SERVER_3 was not defined previously, #defining the default 3.si.pool.ntp.org in time_finctions.h"
        #endif
        #define DEFAULT_NTP_SERVER_3    "3.si.pool.ntp.org"
    #endif

    #define MAX_ETC_NTP_CONF_SIZE 1 * 1024            // 1 KB will usually do - initialization reads the whole /etc/ntp.conf file in the memory 
                                                      
    // crontab definitions
    #define CRON_DAEMON_STACK_SIZE 8 * 1024
    #define CRONTAB_MAX_ENTRIES 32                    // this defines the size of cron table - increase this number if you going to handle more cron events
    #define CRON_COMMAND_MAX_LENGTH 48                // the number of characters in the longest cron command
    #define ANY 255                                   // byte value that corresponds to * in cron table entries (should be >= 60 so it can be distinguished from seconds, minutes, ...)
    #define MAX_TZ_ETC_NTP_CONF_SIZE CRONTAB_MAX_ENTRIES * (20 + CRON_COMMAND_MAX_LENGTH) // this should do
  

    // ----- CODE -----

    bool __timeHasBeenSet__ = false;
    RTC_DATA_ATTR time_t __startupTime__;
  
    static SemaphoreHandle_t __cronSemaphore__ = xSemaphoreCreateRecursiveMutex (); // controls access to cronDaemon's variables

    char __ntpServer1__ [255] = DEFAULT_NTP_SERVER_1; // DNS host name may have max 253 characters
    char __ntpServer2__ [255] = DEFAULT_NTP_SERVER_2; // DNS host name may have max 253 characters
    char __ntpServer3__ [255] = DEFAULT_NTP_SERVER_3; // DNS host name may have max 253 characters 
  
    // synchronizes time with NTP server, returns error message
    char *ntpDate (const char *ntpServer) {
        if (WiFi.localIP ().toString () == "0.0.0.0") {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] not connected to WiFi";
            #endif
            return (char *) "Not connected to WiFi";
        }

        // Based on Let's make a NTP Client in C: https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
        // which I'm keeping here as close to the original as possible due to its comprehensive explanation.

        // »Network Time Protocol«

        typedef struct {
            uint8_t li_vn_mode;       // Eight bits. li, vn, and mode.
                                      // li.   Two bits.   Leap indicator.
                                      // vn.   Three bits. Version number of the protocol.
                                      // mode. Three bits. Client will pick mode 3 for client.
            uint8_t stratum;          // Eight bits. Stratum level of the local clock.
            uint8_t poll;             // Eight bits. Maximum interval between successive messages.
            uint8_t precision;        // Eight bits. Precision of the local clock.
            uint32_t rootDelay;       // 32 bits. Total round trip delay time.
            uint32_t rootDispersion;  // 32 bits. Max error aloud from primary clock source.
            uint32_t refId;           // 32 bits. Reference clock identifier.
            uint32_t refTm_s;         // 32 bits. Reference time-stamp seconds.
            uint32_t refTm_f;         // 32 bits. Reference time-stamp fraction of a second.
            uint32_t origTm_s;        // 32 bits. Originate time-stamp seconds.
            uint32_t origTm_f;        // 32 bits. Originate time-stamp fraction of a second.
            uint32_t rxTm_s;          // 32 bits. Received time-stamp seconds.
            uint32_t rxTm_f;          // 32 bits. Received time-stamp fraction of a second.
            uint32_t txTm_s;          // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
            uint32_t txTm_f;          // 32 bits. Transmit time-stamp fraction of a second.
        } ntp_packet;                 // Total: 384 bits or 48 bytes.

        // »The NTP message consists of a 384 bit or 48 byte data structure containing 17 fields. Note that the order of li, vn, and mode 
        //  is important. We could use three bit fields but instead we’ll combine them into a single byte to avoid any implementation-defined 
        //  issues involving endianness, LSB, and/or MSB.«

        // »Populate our Message«

        // Create and zero out the packet. All 48 bytes worth.
        ntp_packet packet = {};
        // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
        *((char *) &packet + 0) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

        // »First we zero-out or clear out the memory of our structure and then fill it in with leap indicator zero, version number three, and mode 3. 
        // The rest we can leave blank and still get back the time from the server.«

        // »Setup our Socket and Server Data Structure«

        // Create a UDP socket, convert the host-name to an IP address, set the port number,
        // connect to the server, send the packet, and then read in the return packet.

        struct hostent *server;       // Server data structure.
        struct sockaddr_in serv_addr; // Server address data structure.

        int sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Create a UDP socket.
        if (sockfd < 0) {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] socket() error: " << errno << " " << strerror (errno);
            #endif
            return (char *) "Could not open a socket";
        }

        // remember some information that netstat telnet command would use
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET] = { __NTP_SOCKET__, 0, 0, millis (), millis () };

        // remember some information that netstat telnet command would use
        // additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET] = { __NTP_SOCKET__, 0, 0, millis (), millis () };

        // Setup socket timeout to 1s, this will limit the time we wait for NTP reply
        struct timeval tout = {1, 0};
        if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof (tout)) < 0) {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] setsockopt() error: " << errno << strerror (errno);
            #endif
            close (sockfd);
            return (char *) "Could not set socket time-out";
        }

        server = gethostbyname (ntpServer); // Convert URL to IP.
        if (server == NULL) {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] gethostbyname() error: " << errno << strerror (errno);
            #endif
            return (char *) "NTP server not found";
        }

        // Zero out the server address structure.
        bzero ((char *) &serv_addr, sizeof (serv_addr));

        serv_addr.sin_family = AF_INET;
        // Copy the server's IP address to the server address structure.
        bcopy ((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);

        // Convert the port number integer to network big-endian style and save it to the server address structure.
        serv_addr.sin_port = htons (123);

        // »Before we can start communicating we have to setup our socket, server and server address structures. We will be using the 
        //  User Datagram Protocol (versus TCP) for our socket since the server we are sending our message to is listening on port 
        //  number 123 using UDP.«

        // »Send our Message to the Server«

        // Call up the server using its IP address and port number.

        if (connect (sockfd, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] connect() error: " << errno << " " << strerror (errno);
            #endif          
            close (sockfd);
            return (char *) "Could not connect to NTP server";
        }

        // Send it the NTP packet it wants. If n == -1, it failed.
        int n;
        n = sendto (sockfd, (char *) &packet, sizeof (ntp_packet), 0, (struct sockaddr *) &serv_addr, sizeof (serv_addr));
        if (n < 0) {
            #ifdef __DMESG__
                dmesgQueue << "[NTP] sendto() error: " << errno << " " << strerror (errno);
            #endif          
            close (sockfd);
            return (char *) "Could not sent NTP packet";
        }

        additionalNetworkInformation.bytesSent += n; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].bytesSent += n; // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis ();

        // remember some information that netstat telnet command would use
        // additionalNetworkInformation.bytesSent += n;
        // additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].bytesSent += n;
        // additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis (); 
  
        // »With our message payload, socket, server and address setup, we can now send our message to the server. 
        //  To do this, we write our 48 byte struct to the socket.«

        // »Read in the Return Message«

        // Wait and receive the packet back from the server. If n == -1, it failed.

        int fromlen;
        struct sockaddr_in from;
        n = recvfrom (sockfd, (char *) &packet, sizeof (ntp_packet), 0, (struct sockaddr *) &from, (socklen_t*) &fromlen);
        if (n < 0) {
          #ifdef __DMESG__
                dmesgQueue << "[NTP] recvfrom() error: " <<  errno << " " << strerror (errno);
            #endif          
            close (sockfd);
            return (char *) "Could not receive NTP packet";
        }

        // remember some information that netstat telnet command would use
        additionalNetworkInformation.bytesReceived += n;
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].bytesReceived += n;
        additionalSocketInformation [sockfd - LWIP_SOCKET_OFFSET].lastActiveMillis = millis (); 

        // »Now that our message is sent, we block or wait for the response by reading from the socket. The message we get back should be the same
        //  size as the message we sent. We will store the incoming message in packet just like we stored our outgoing message.«

        // »Parse the Return Message«

        // These two fields contain the time-stamp seconds as the packet left the NTP server.
        // The number of seconds correspond to the seconds passed since 1900.
        // ntohl() converts the bit/byte order from the network's to host's "endianness".

        packet.txTm_s = ntohl (packet.txTm_s); // Time-stamp seconds.
        packet.txTm_f = ntohl (packet.txTm_f); // Time-stamp fraction of a second.

        // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
        // Subtract 70 years worth of seconds from the seconds since 1900.
        // This leaves the seconds since the UNIX epoch of 1970.
        // (1900)------------------(1970)**************************************(Time Packet Left the Server)

        #define NTP_TIMESTAMP_DELTA 2208988800
        time_t txTm = (time_t) (packet.txTm_s - NTP_TIMESTAMP_DELTA);

        // »The message we get back is in network order or big-endian form. Depending on the machine you run this on, ntohl will transform the bits 
        //  from either big to little or big to big-endian. You can think of big or little-endian as reading from left to right or tfel ot thgir respectively.
        //
        //  With the data in the order we need it, we can now subtract the delta and cast the resulting number to a time-stamp number. 
        //  Note that NTP_TIMESTAMP_DELTA = 2208988800ull which is the NTP time-stamp of 1 Jan 1970 or put another way 2,208,988,800 unsigned long long seconds.«

        setTimeOfDay (txTm);
        #ifdef __DMESG__
            dmesgQueue << "[time] synchronized with " << ntpServer;
        #endif
        close (sockfd);
        cout << "[time] synchronized with " << ntpServer << endl;

        return (char *) ""; // OK
    }
  
    char *ntpDate () { // synchronizes time with NTP servers, returns error message
        char *s;
        if (!*(s = ntpDate (__ntpServer1__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesgQueue << "[time] " << s; 
        #endif
        cout << "[time] " << s << endl;
        delay (1);
        if (!*(s = ntpDate (__ntpServer2__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesgQueue << "[time] " << s; 
        #endif
        cout << "[time] " << s << endl;
        delay (1);
        if (!*(s = ntpDate (__ntpServer3__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesgQueue << "[time] " << s; 
        #endif
        cout << "[time] " << s << endl;
        return (char *) "NTP servers are not available.";
    }
  
    int __cronTabEntries__ = 0;
  
    struct cronEntryType {
        bool readFromFile;                              // true, if this entry is read from /etc/crontab file, false if it is inserted by program code
        bool markForExecution;                          // falg if the command is to be executed
        bool executed;                                  // flag if the command is beeing executed
        uint8_t second;                                 // 0-59, ANY (255) means *
        uint8_t minute;                                 // 0-59, ANY (255) means *
        uint8_t hour;                                   // 0-23, ANY (255) means *
        uint8_t day;                                    // 1-31, ANY (255) means *
        uint8_t month;                                  // 1-12, ANY (255) means *
        uint8_t day_of_week;                            // 0-6 and 7, Sunday to Saturday, 7 is also Sunday, ANY (255) means *
        char cronCommand [CRON_COMMAND_MAX_LENGTH + 1]; // cronCommand to be passed to cronHandler when time condition is met - it is reponsibility of cronHandler to do with it what is needed
        time_t lastExecuted;                            // the time cronCommand has been executed
    } __cronEntry__ [CRONTAB_MAX_ENTRIES];
  
    // adds new entry into crontab table
    bool cronTabAdd (uint8_t second, uint8_t minute, uint8_t hour, uint8_t day, uint8_t month, uint8_t day_of_week, const char *cronCommand, bool readFromFile = false) {
        bool b = false;    
        xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
            if (__cronTabEntries__ < CRONTAB_MAX_ENTRIES - 1) {
                __cronEntry__ [__cronTabEntries__] = {readFromFile, false, false, second, minute, hour, day, month, day_of_week, {}, 0};
                strncpy (__cronEntry__ [__cronTabEntries__].cronCommand, cronCommand, CRON_COMMAND_MAX_LENGTH);
                __cronEntry__ [__cronTabEntries__].cronCommand [CRON_COMMAND_MAX_LENGTH] = 0;
                __cronTabEntries__ ++;
            }
            b = true;
        xSemaphoreGiveRecursive (__cronSemaphore__);
        if (b) return true;
        #ifdef __DMESG__
            dmesgQueue << "[time][cronDaemon][cronTabAdd] can't add cronCommand, cron table is full: " << cronCommand;
        #endif
        cout << "[time][cronDaemon][cronTabAdd] can't add cronCommand, cron table is full: " << cronCommand << endl;
        return false;
    }
    
    // adds new entry into crontab table
    bool cronTabAdd (const char *cronTabLine, bool readFromFile = false) { // parse cronTabLine and then call the function above
        char second [3]; char minute [3]; char hour [3]; char day [3]; char month [3]; char day_of_week [3]; char cronCommand [65];
        if (sscanf (cronTabLine, "%2s %2s %2s %2s %2s %2s %64s", second, minute, hour, day, month, day_of_week, cronCommand) == 7) {
            int8_t se = strcmp (second, "*")      ? atoi (second)      : ANY; if ((!se && *second != '0')      || se > 59)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid second: " << second; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid second: " << second << endl;  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t mi = strcmp (minute, "*")      ? atoi (minute)      : ANY; if ((!mi && *minute != '0')      || mi > 59)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid minute: " << minute; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid minute: " << minute << endl;  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t hr = strcmp (hour, "*")        ? atoi (hour)        : ANY; if ((!hr && *hour != '0')        || hr > 23)  {
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid hour: " << hour; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid hour: " << hour << endl;  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t dm = strcmp (day, "*")         ? atoi (day)         : ANY; if (!dm                          || dm > 31)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid day: " << day; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid day: " << day << endl;
                                                                                                                                return false; 
                                                                                                                            }
            int8_t mn = strcmp (month, "*")       ? atoi (month)       : ANY; if (!mn                          || mn > 12)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid month: " << month; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid month: " << month << endl;  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t dw = strcmp (day_of_week, "*") ? atoi (day_of_week) : ANY; if ((!dw && *day_of_week != '0') || dw > 7)   {
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesgQueue << "[time][cronDaemon][cronAdd] invalid day of week: " << day_of_week; 
                                                                                                                                #endif
                                                                                                                                cout << "[time][cronDaemon][cronAdd] invalid day of week: " << day_of_week << endl;
                                                                                                                                return false; 
                                                                                                                            }
            if (!*cronCommand) { 
                #ifdef __DMESG__
                    dmesgQueue << "[time][cronDaemon][cronAdd] missing cron command";
                #endif
                cout << "[time][cronDaemon][cronAdd] missing cron command\n";  
                return false;
            }
            return cronTabAdd (se, mi, hr, dm, mn, dw, cronCommand, readFromFile);
        } else {
            #ifdef __DMESG__
                dmesgQueue << "[time][cronDaemon][cronAdd] invalid cronTabLine: " << cronTabLine;
            #endif
            cout << "[time][cronDaemon][cronAdd] invalid cronTabLine: " << cronTabLine << endl;
            return false;
        }
    }
    
    // deletes entry(es) from crontam table
    int cronTabDel (const char *cronCommand) { // returns the number of cron commands being deleted
        int cnt = 0;
        xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
            int i = 0;
            while (i < __cronTabEntries__) {
                if (!strcmp (__cronEntry__ [i].cronCommand, cronCommand)) {        
                    for (int j = i; j < __cronTabEntries__ - 1; j ++) { __cronEntry__ [j] = __cronEntry__ [j + 1]; }
                    __cronTabEntries__ --;
                    cnt ++;
                } else {
                    i ++;
                }
            }
        xSemaphoreGiveRecursive (__cronSemaphore__);
        if (!cnt) {
            #ifdef __DMESG__
                dmesgQueue << "[time][cronDaemon][cronTabDel] cronCommand not found: " << cronCommand;
            #endif
            cout << "[time][cronDaemon][cronTabDel] cronCommand not found: " << cronCommand << endl;
        }
        return cnt;
    }

    // startCronDaemon functions runs __cronDaemon__ as a separate task. It does two things: 
    // 1. - it executes cron commands from cron table when the time is right
    // 2. - it synchronizes time with NTP servers once a day
    void __cronDaemon__ (void *ptrCronHandler) { 
        #ifdef __DMESG__
            dmesgQueue << "[time][cronDaemon] is running on core " << xPortGetCoreID ();
        #endif
        cout << "[time][cronDaemon] started\n";
        do {     // try to set/synchronize the time, retry after 1 minute if unsuccessfull 
            delay (15000);
            if (!*ntpDate ()) break; // success
            delay (45000);
        } while (!time ());

        void (* cronHandler) (const char *) = (void (*) (const char *)) ptrCronHandler;  
        unsigned long lastMinuteCheckMillis = millis ();
        unsigned long lastHourCheckMillis = millis ();
        unsigned long lastDayCheckMillis = millis ();

        while (true) {
            delay (10);

            // trigger "EVERY_MINUTE" built in event? (triggers regardles wether real time is known or not)
            if (millis () - lastMinuteCheckMillis >= 60000) { 
                lastMinuteCheckMillis = millis ();
                if (cronHandler != NULL) cronHandler ("ONCE_A_MINUTE");
            }

            // trigger "EVERY_HOUR" built in event? (triggers regardles wether real time is known or not)
            if (millis () - lastHourCheckMillis >= 3600000) { 
                lastHourCheckMillis = millis ();
                if (cronHandler != NULL) cronHandler ("ONCE_AN_HOUR");
            }

            // trigger "EVERY_DAY" built in event? (triggers regardles wether real time is known or not)
            if (millis () - lastDayCheckMillis >= 86400000) { 
                lastDayCheckMillis = millis ();
                if (cronHandler != NULL) cronHandler ("ONCE_A_DAY");

                // 2. synchronize time with NTP servers
                ntpDate ();  
            } 

            // 3. execute cron commands from cron table
            time_t now = time (NULL);
            if (!now) continue; // if the time is not known cronDaemon can't do anythig
            static time_t previous = now;
            for (time_t l = previous + 1; l <= now; l++) {
                delay (1);
                struct tm slt; localtime_r (&l, &slt); 
                //scan through cron entries and find commands that needs to be executed (at time l)
                xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
                    // mark cron commands for execution
                    for (int i = 0; i < __cronTabEntries__; i ++) {
                        if ((__cronEntry__ [i].second == ANY      || __cronEntry__ [i].second == slt.tm_sec) && 
                            (__cronEntry__ [i].minute == ANY      || __cronEntry__ [i].minute == slt.tm_min) &&
                            (__cronEntry__ [i].hour == ANY        || __cronEntry__ [i].hour == slt.tm_hour) &&
                            (__cronEntry__ [i].day == ANY         || __cronEntry__ [i].day == slt.tm_mday) &&
                            (__cronEntry__ [i].month == ANY       || __cronEntry__ [i].month == slt.tm_mon + 1) &&
                            (__cronEntry__ [i].day_of_week == ANY || __cronEntry__ [i].day_of_week == slt.tm_wday || __cronEntry__ [i].day_of_week == slt.tm_wday + 7) ) {

                                if (!__cronEntry__ [i].executed) {
                                  __cronEntry__ [i].markForExecution = true;
                                }
                                                
                        } else {

                                __cronEntry__ [i].executed = false;
              
                        }
                    }          
                    // execute marked cron commands
                    int execCnt = 1;
                    while (execCnt) { // if a command during execution deletes other commands we have to repeat this step
                        execCnt = 0;
                        for (int i = 0; i < __cronTabEntries__; i ++) {
                            if ( __cronEntry__ [i].markForExecution ) {

                                if (__cronEntry__ [i].markForExecution) {
                                    __cronEntry__ [i].executed = true;
                                    __cronEntry__ [i].lastExecuted = now;
                                    __cronEntry__ [i].markForExecution = false;
                                    if (cronHandler != NULL) cronHandler (__cronEntry__ [i].cronCommand); // this should be called last in case cronHandler calls croTabAdd or cronTabDel itself                        
                                    execCnt ++; 
                                    delay (1);
                                }

                                __cronEntry__ [i].markForExecution = false;
                            }
                        }
                    }
                xSemaphoreGiveRecursive (__cronSemaphore__);
            }
            previous = now;
        }
    }

    // Runs __cronDaemon__ as a separate task. The first time it is called it also creates default configuration files /etc/ntp.conf and /etc/crontab
    void startCronDaemon (void (* cronHandler) (const char *)) { // synchronizes time with NTP servers from /etc/ntp.conf, reads /etc/crontab, returns error message. If /etc/ntp.conf or /etc/crontab don't exist (yet) creates the default one

        #ifdef __FILE_SYSTEM__
            if (fileSystem.mounted ()) {
              // read TZ (timezone) configuration from /usr/share/zoneinfo, create a new one if it doesn't exist
              if (!fileSystem.isFile ("/usr/share/zoneinfo")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ("/usr/share")) { fileSystem.makeDirectory ("/usr"); fileSystem.makeDirectory ("/usr/share"); }
                  cout << "[time][cronDaemon] /usr/share/zoneinfo does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ("/usr/share/zoneinfo", "w");
                  if (f) {
                      String defaultContent = F ( "# timezone configuration - reboot for changes to take effect\r\n"
                                                  "# choose one of available (POSIX) timezones: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv\r\n\r\n"
                                                  "TZ " TZ "\r\n");
                    created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());
                    f.close ();

                    diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  }
                  cout << (created ? "created\n" : "error\n");
              
              }
              {
                  cout << "[time][cronDaemon] reading /usr/share/zoneinfo ... ";
                  // parse configuration file if it exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/usr/share/zoneinfo")) {
                      strcat (buffer, "\n");
                      // cout << buffer << "\n";

                      char __TZ__ [100] = TZ;
                      char *p;
                      if ((p = stristr (buffer, "\nTZ"))) sscanf (p + 3, "%*[ =]%98[!-~]", __TZ__);                      

                      setenv ("TZ", __TZ__, 1);
                      tzset ();
                      #ifdef __DMESG__
                          dmesgQueue << "[time][cronDaemon] TZ environment variable set to " << __TZ__;
                      #endif
                      cout << "OK, TZ environment variable set to " << __TZ__ << endl;
                  } else {
                      cout << "error\n";
                  }
              }

              // read NTP configuration from /etc/ntp.conf, create a new one if it doesn't exist
              if (!fileSystem.isFile ((char *) "/etc/ntp.conf")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ((char *) "/etc")) { fileSystem.makeDirectory ((char *) "/etc"); }
                  cout << "[time][cronDaemon] /etc/ntp.conf does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ((char *) "/etc/ntp.conf", "w");
                  if (f) {
                      String defaultContent = F ( "# configuration for NTP - reboot for changes to take effect\r\n\r\n"
                                                  "server1 " DEFAULT_NTP_SERVER_1 "\r\n"
                                                  "server2 " DEFAULT_NTP_SERVER_2 "\r\n"
                                                  "server3 " DEFAULT_NTP_SERVER_3 "\r\n");
                      created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());
                      f.close ();

                      diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  }
                  cout << (created ? "created\n" : "error\n");
              }
              {
                  cout << "[time][cronDaemon] reading /etc/ntp.conf ... ";
                  // parse configuration file if it exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/etc/ntp.conf")) {
                      strcat (buffer, "\n");
                      // cout << buffer;

                      *__ntpServer1__ = *__ntpServer2__ = *__ntpServer3__ = 0; 
                      char *p;                    
                      if ((p = stristr (buffer, "\nserver1"))) sscanf (p + 8, "%*[ =]%253[0-9A-Za-z.-]", __ntpServer1__);
                      if ((p = stristr (buffer, "\nserver2"))) sscanf (p + 8, "%*[ =]%253[0-9A-Za-z.-]", __ntpServer2__);
                      if ((p = stristr (buffer, "\nserver3"))) sscanf (p + 8, "%*[ =]%253[0-9A-Za-z.-]", __ntpServer3__);

                      // cout << __ntpServer1__ << ", " << __ntpServer1__ << ", " << __ntpServer1__ << endl;
                      cout << "OK\n";
                  } else {
                      cout << "error\n";
                  }
              }

              // read scheduled tasks from /etc/crontab
              if (!fileSystem.isFile ((char *) "/etc/crontab")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ((char *) "/etc")) { fileSystem.makeDirectory ((char *) "/etc"); }          
                  cout << "[time][cronDaemon] /etc/crontab does not exist, creating default one ... ";
                  bool created = false;
                  File f = fileSystem.open ((char *) "/etc/crontab", "w");
                  if (f) {
                      String defaultContent = F("# scheduled tasks (in local time) - reboot for changes to take effect\r\n"
                                                "#\r\n"
                                                "# .------------------- second (0 - 59 or *)\r\n"
                                                "# |  .---------------- minute (0 - 59 or *)\r\n"
                                                "# |  |  .------------- hour (0 - 23 or *)\r\n"
                                                "# |  |  |  .---------- day of month (1 - 31 or *)\r\n"
                                                "# |  |  |  |  .------- month (1 - 12 or *)\r\n"
                                                "# |  |  |  |  |  .---- day of week (0 - 7 or *; Sunday=0 and also 7)\r\n"
                                                "# |  |  |  |  |  |\r\n"
                                                "# *  *  *  *  *  * cronCommand to be executed\r\n"
                                                "# * 15  *  *  *  * exampleThatRunsQuaterPastEachHour\r\n");

                    created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());
                    f.close ();

                    diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  }
                  cout << (created ? "created\n" : "error\n");
              }
              {
                  cout << "[time][cronDaemon] reading /etc/crontab ... ";
                  // parse scheduled tasks if /etc/crontab exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE] = "\n";
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, "/etc/crontab")) {
                      strcat (buffer, "\n");
                      // cout << buffer;

                      char *p, *q;
                      p = buffer + 1;
                      while (*p) {
                          if ((q = strstr (p, "\n"))) {
                              *q = 0;
                              if (*p) cronTabAdd (p, true);
                              p = q + 1;
                          }
                        }

                        cout << "OK, " << __cronTabEntries__ << " entries\n";
                    } else {
                        cout << "error\n";
                    }
              }
            } else {
                cout << "[time][cronDaemon] file system not mounted, can't read or write configuration files\n";
            }
        #else

            // set the default timezone
            setenv ("TZ", (char *) TZ, 1);
            tzset ();
            #ifdef __DMESG__
                dmesgQueue << "[time][cronDaemon] TZ environment variable set to " << TZ;
            #endif
            cout << "[time][cronDaemon] TZ environment variable set to " << TZ << endl;

            cout << "[time][cronDaemon] is starting without a file system\n";
        #endif    
        
        // run periodic synchronization with NTP servers and execute cron commands in a separate thread
        #define tskNORMAL_PRIORITY 1
        if (pdPASS != xTaskCreate (__cronDaemon__, "cronDaemon", CRON_DAEMON_STACK_SIZE, (void *) cronHandler, tskNORMAL_PRIORITY, NULL)) {
            #ifdef __DMESG__
                dmesgQueue << "[time][cronDaemon] xTaskCreate error, could not start cronDaemon";
            #endif
            cout << "[time][cronDaemon] xTaskCreate error, could not start cronDaemon\n";
        }
    }
  
    // a more convinient version of time function: returns GMT or 0 if the time is not set
    time_t time () {
        time_t t = time (NULL);
        if (t < 1687461154) return 0; // 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this
        return t;
    }

    // returns the number of seconfs ESP32 has been running
    time_t getUptime () {
        time_t t = time (NULL);
        return __timeHasBeenSet__ ? t - __startupTime__ :  millis () / 1000; // if the time has already been set, 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this
    }
  
    // sets internal clock, also sets/corrects __startupTime__ internal variable
    void setTimeOfDay (time_t t) {         
        time_t oldTime = time (NULL);
        struct timeval newTime = {t, 0};
        settimeofday (&newTime, NULL); 

        char buf [100];
        if (__timeHasBeenSet__) {
            ascTime (localTime (oldTime), buf);
            strcat (buf, " to ");
            ascTime (localTime (t), buf + strlen (buf));
            #ifdef __DMESG__
                dmesgQueue << "[time] time corrected from " << buf;
            #endif
            cout << "[time] time corrected from " << buf << endl;
        } else {
            __startupTime__ = t - getUptime (); // recalculate            
            __timeHasBeenSet__ = true;
            ascTime (localTime (t), buf);
            #ifdef __DMESG__
                dmesgQueue << "[time] time set to " << buf; 
            #endif
            cout << "[time] time set to " << buf << endl;
        }
    }

    // a more convinient version of thread safe gmtime_r function: converts GMT in time_t to GMT in struct tm
    struct tm gmTime (const time_t t) {
        struct tm st;
        return *gmtime_r (&t, &st);
    }

    // a more convinient version of thread safe localtime_r function: converts GMT in time_t to local time in struct tm
    struct tm localTime (const time_t t) {
        struct tm st;
        return *localtime_r (&t, &st);
    }

    // a more convinient version of thread safe asctime_r function: converts struct tm to array of chars, buff should have at least 26 bytes
    char *ascTime (const struct tm st, char *buf) {
        // asctime_r (&st, buf);
        // char *p; for (p = buf; *p >= ' '; p ++); *p = 0; // rtrim ending \n
        // return buf;
        
        sprintf (buf, "%04i/%02i/%02i %02i:%02i:%02i", 1900 + st.tm_year, 1 + st.tm_mon, st.tm_mday, st.tm_hour, st.tm_min, st.tm_sec);
        return buf; 
    }


    // ----- backward compatibility -----

    [[deprecated("Use time (void) instead.")]]  
    time_t getGmt () { 
      return time ();
    }

    [[deprecated("Use setTimeOfDay (time_t) instead.")]]  
    void setGmt (time_t t) {
        setTimeOfDay (t);
    }

    [[deprecated("Use setTimeOfDay (time_t) instead.")]]  
    void setLocalTime (time_t t) { 
        setTimeOfDay (t);
    }
  
    [[deprecated("Use gmTime (time_t) instead.")]]  
    struct tm timeToStructTime (time_t t) { return gmTime (t); }

    [[deprecated("Use ascTime (struct tm, char*) instead.")]]  
    bool timeToString (char *buf, size_t bufSize, time_t t) {
        if (bufSize >= 26) {
            ascTime (gmTime (t), buf);
            return true;    
        }
        return false;
    }  

    #define KAL_TIMEZONE                      "EET-2"
    #define MSK_TIMEZONE                      "MSK-3"
    #define SAM_TIMEZONE                      "<+04>-4"
    #define YEK_TIMEZONE                      "<+05>-5"
    #define OMS_TIMEZONE                      "<+06>-6"
    #define KRA_TIMEZONE                      "<+07>-7"
    #define IRK_TIMEZONE                      "<+08>-8"
    #define YAK_TIMEZONE                      "<+09>-9"
    #define VLA_TIMEZONE                      "<+10>-10"
    #define SRE_TIMEZONE                      "<+11>-11"
    #define PET_TIMEZONE                      "<+12>-12"
    #define JAPAN_TIMEZONE                    "<+09>-9"
    #define CHINA_TIMEZONE                    "<+08>-8"
    #define WET_TIMEZONE                      "GMT0BST,M3.5.0/1,M10.5.0"
    #define ICELAND_TIMEZONE                  "GMT0"
    #define CET_TIMEZONE                      "CET-1CEST,M3.5.0,M10.5.0/3"
    #define EET_TIMEZONE                      "EET-2EEST,M3.5.0/3,M10.5.0/4"
    #define FET_TIMEZONE                      "<+04>-4"
    #define NEWFOUNDLAND_TIMEZONE             "NST3:30NDT1:30"
    #define ATLANTIC_TIMEZONE                 "NZST-12NZDT,M9.5.0,M4.1.0/3"
    #define ATLANTIC_NO_DST_TIMEZONE          "AST4"
    #define EASTERN_TIMEZONE                  "EST5EDT,M3.2.0,M11.1.0"
    #define EASTERN_NO_DST_TIMEZONE           "<+05>-5"
    #define CNTRAL_TIMEZONE                   "CST6CDT,M3.2.0,M11.1.0"
    #define CNTRAL_NO_DST_TIMEZONE            "CST6"
    #define MOUNTAIN_TIMEZONE                 "MST7MDT,M3.2.0,M11.1.0"
    #define MOUNTAIN_NO_DST_TIMEZONE          "MST"
    #define PACIFIC_TIMEZONE                  "PST8PDT,M3.2.0,M11.1.0"
    #define ALASKA_TIMEZNE                    "AKST9AKDT,M3.2.0,M11.1.0"
    #define HAWAII_ALEUTIAN_TIMEZONE          "HST10"
    #define HAWAII_ALEUTIAN_NO_DST_TIMEZONE   "<-10>10"
    #define AMERICAN_SAMOA_TIMEZONE           "<-11>11"
    #define BAKER_HOWLAND_ISLANDS_TIMEZONE    "<-12>12"
    #define WAKE_ISLAND_TIMEZONE              "<+12>-12"
    #define CHAMORRO_TIMEZONE                 "<+10>-10"    

#endif
