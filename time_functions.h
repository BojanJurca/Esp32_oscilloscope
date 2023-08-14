  /*

    time_functions.h
 
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    Real_time_clock synchronizes its time with NTP server accessible from internet once a day. Internet connection is 
    necessary for real_time_clock to get initialized.

    June 25, 2023, Bojan Jurca

    Nomenclature used in time_functions.h - for easier understaning of the code:

      - "buffer size" is the number of bytes that can be placed in a buffer. 

                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".      

      - "max length" is the number of characters that can be placed in a variable.

                  In case of C 0-terminated strings the terminating 0 character is not included so the buffer should be at least 1 byte larger.    
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include "time.h"


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
    time_t localTime ();
    struct tm gmTime (time_t);
    struct tm localTime (time_t);
    char *ascTime (const struct tm, char *);
    time_t getUptime ();
    char *ntpDate (char *);
    char *ntpDate ();
    bool cronTabAdd (uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, char *, bool);
    bool cronTabAdd (char *, bool);
    int cronTabDel (char *);
    void startCronDaemon (void (* cronHandler) (char *), size_t);


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

    char __ntpServer1__ [254] = DEFAULT_NTP_SERVER_1; // host name may have max 253 characters
    char __ntpServer2__ [254] = DEFAULT_NTP_SERVER_2; // host name may have max 253 characters
    char __ntpServer3__ [254] = DEFAULT_NTP_SERVER_3; // host name may have max 253 characters 
  
    // synchronizes time with NTP server, returns error message
    char *ntpDate (char *ntpServer) {

        // prepare NTP request
        byte ntpPacket [48];
        memset (ntpPacket, 0, sizeof (ntpPacket));
        ntpPacket [0] = 0b11100011;  
        ntpPacket [1] = 0;           
        ntpPacket [2] = 6;           
        ntpPacket [3] = 0xEC;  
        ntpPacket [12] = 49;
        ntpPacket [13] = 0x4E;
        ntpPacket [14] = 49;
        ntpPacket [15] = 52;  
        // send NTP request
        IPAddress ntpServerIp;
        WiFiUDP udp;
    
        if (!WiFi.hostByName (ntpServer, ntpServerIp)) return (char *) "Could not find NTP server by its name.";
        // open internal port
        #define INTERNAL_NTP_PORT 2390
        if (!udp.begin (INTERNAL_NTP_PORT)) return (char *) "Internal port 2390 is not available for NTP.";
        // start UDP
        #define NTP_PORT 123
        if (!udp.beginPacket (ntpServerIp, NTP_PORT)) { udp.stop (); return (char *) "NTP server is not available on default NTP port 123."; }
        // send UDP
        if (udp.write (ntpPacket, sizeof (ntpPacket)) != sizeof (ntpPacket)) { udp.stop (); return (char *) "Could't send NTP request."; }
        // check if UDP request has been sent
        if (!udp.endPacket ()) { udp.stop (); return (char *) "NTP request not sent."; }
      
        // wait for NTP reply or time-out
        unsigned long ntpRequestMillis = millis ();
        while (true) {
            if (millis () - ntpRequestMillis >= 500) { udp.stop (); return (char *) "NTP request time-out."; }
            if (udp.parsePacket () != sizeof (ntpPacket)) continue; // keep waiting for NTP reply
            // read NTP reply
            udp.read (ntpPacket, sizeof (ntpPacket));
            if (!ntpPacket [40] && !ntpPacket [41] && !ntpPacket [42] && !ntpPacket [43]) { udp.stop (); return (char *) "Invalid NTP reply."; }
            // convert reply into UNIX time
            unsigned long highWord;
            unsigned long lowWord;
            unsigned long secsSince1900;
            #define SEVENTY_YEARS 2208988800UL
            highWord = word (ntpPacket [40], ntpPacket [41]); 
            lowWord = word (ntpPacket [42], ntpPacket [43]);
            secsSince1900 = highWord << 16 | lowWord;
            time_t currentTime = secsSince1900 - SEVENTY_YEARS;
            if (currentTime < 1687461154) { udp.stop (); return (char *) "Wrong NTP reply."; } // 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this
            udp.stop ();
            setTimeOfDay (currentTime);
            #ifdef __DMESG__
                dmesg ("[time] synchronized with ", ntpServer);
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] synchronized with ", ntpServer);
            break;
        }
        return (char *) ""; // no error = OK  
    }
  
    char *ntpDate () { // synchronizes time with NTP servers, returns error message
        char *s;
        if (!*(s = ntpDate (__ntpServer1__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesg ("[time] ", s); 
        #endif
        Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] ", s);
        delay (1);
        if (!*(s = ntpDate (__ntpServer2__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesg ("[time] ", s); 
        #endif
        Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] ", s);
        delay (1);
        if (!*(s = ntpDate (__ntpServer3__))) return (char *) ""; 
        #ifdef __DMESG__
            dmesg ("[time] ", s); 
        #endif
        Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] ", s);
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
    bool cronTabAdd (uint8_t second, uint8_t minute, uint8_t hour, uint8_t day, uint8_t month, uint8_t day_of_week, char *cronCommand, bool readFromFile = false) {
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
            dmesg ("[time][cronDaemon][cronTabAdd] can't add cronCommand, cron table is full: ", cronCommand);
        #endif
        Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronTabAdd] can't add cronCommand, cron table is full: ", cronCommand);
        return false;
    }
    
    // adds new entry into crontab table
    bool cronTabAdd (char *cronTabLine, bool readFromFile = false) { // parse cronTabLine and then call the function above
        char second [3]; char minute [3]; char hour [3]; char day [3]; char month [3]; char day_of_week [3]; char cronCommand [65];
        if (sscanf (cronTabLine, "%2s %2s %2s %2s %2s %2s %64s", second, minute, hour, day, month, day_of_week, cronCommand) == 7) {
            int8_t se = strcmp (second, "*")      ? atoi (second)      : ANY; if ((!se && *second != '0')      || se > 59)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid second: ", second); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid second: ", second);  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t mi = strcmp (minute, "*")      ? atoi (minute)      : ANY; if ((!mi && *minute != '0')      || mi > 59)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid minute: ",  minute); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid minute: ", minute);  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t hr = strcmp (hour, "*")        ? atoi (hour)        : ANY; if ((!hr && *hour != '0')        || hr > 23)  {
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid hour: ",  hour); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid hour: ", hour);  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t dm = strcmp (day, "*")         ? atoi (day)         : ANY; if (!dm                          || dm > 31)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid day: ", day); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid day: ", day);  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t mn = strcmp (month, "*")       ? atoi (month)       : ANY; if (!mn                          || mn > 12)  { 
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid month: ", month); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid month: ", month);  
                                                                                                                                return false; 
                                                                                                                            }
            int8_t dw = strcmp (day_of_week, "*") ? atoi (day_of_week) : ANY; if ((!dw && *day_of_week != '0') || dw > 7)   {
                                                                                                                                #ifdef __DMESG__
                                                                                                                                    dmesg ("[time][cronDaemon][cronAdd] invalid day of week: ", day_of_week); 
                                                                                                                                #endif
                                                                                                                                Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid day of week: ", day_of_week);  
                                                                                                                                return false; 
                                                                                                                            }
            if (!*cronCommand) { 
                #ifdef __DMESG__
                    dmesg ("[time][cronDaemon][cronAdd] missing cron command");
                #endif
                Serial.printf ("[%10lu] %s\r\n", millis (), "[time][cronDaemon][cronAdd] missing cron command");  
                return false;
            }
            return cronTabAdd (se, mi, hr, dm, mn, dw, cronCommand, readFromFile);
        } else {
            #ifdef __DMESG__
                dmesg ("[time][cronDaemon][cronAdd] invalid cronTabLine: ", cronTabLine);
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronAdd] invalid cronTabLine: ", cronTabLine);
            return false;
        }
    }
    
    // deletes entry(es) from crontam table
    int cronTabDel (char *cronCommand) { // returns the number of cron commands being deleted
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
                dmesg ("[time][cronDaemon][cronTabDel] cronCommand not found: ", cronCommand);
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon][cronTabDel] cronCommand not found: ", cronCommand);
        }
        return cnt;
    }

    // startCronDaemon functions runs __cronDaemon__ as a separate task. It does two things: it synchronizes time with NTP servers once a day and executes cron commands from cron table when the time is right
    void __cronDaemon__ (void *ptrCronHandler) { 
        #ifdef __DMESG__
            dmesg ("[time][cronDaemon] is running on core ", xPortGetCoreID ());
        #endif
        Serial.printf ("[%10lu] %s\r\n", millis (), "[time][cronDaemon] started");
        do {     // try to set/synchronize the time, retry after 1 minute if unsuccessfull 
            delay (15000);
            if (!*ntpDate ()) break; // success
            delay (45000);
        } while (!time ());

        void (* cronHandler) (char *) = (void (*) (char *)) ptrCronHandler;  
        unsigned long lastSyncMillis = millis (); 

        while (true) {
            delay (10);

            // 1. synchronize time with NTP servers
            if (millis () - lastSyncMillis >= 86400000) { ntpDate (); lastSyncMillis = millis (); } 

            // 2. execute cron commands from cron table
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
                                  // DEBUG: Serial.printf ("[%10lu] [time][cronDaemon] %s marked for execution\n", millis (), __cronEntry__ [i].cronCommand);
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
                                    // DEBUG: Serial.printf ("[%10lu] [time][cronDaemon] %s executed\n", millis (), __cronEntry__ [i].cronCommand);
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
    void startCronDaemon (void (* cronHandler) (char *)) { // synchronizes time with NTP servers from /etc/ntp.conf, reads /etc/crontab, returns error message. If /etc/ntp.conf or /etc/crontab don't exist (yet) creates the default one
        Serial.printf ("[%10lu] [time][cronDaemon] starting ...\n", millis ());

        #ifdef __FILE_SYSTEM__
            if (fileSystem.mounted ()) {
              // read TZ (timezone) configuration from /usr/share/zoneinfo, create a new one if it doesn't exist
              if (!fileSystem.isFile ((char *) "/usr/share/zoneinfo")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ((char *) "/usr/share")) { fileSystem.makeDirectory ((char *) "/usr"); fileSystem.makeDirectory ((char *) "/usr/share"); }
                  Serial.printf ("[%10lu] [time][cronDaemon] /usr/share/zoneinfo does not exist, creating default one ... ", millis ());
                  bool created = false;
                  File f = fileSystem.open ((char *) "/usr/share/zoneinfo", "w", true);
                  if (f) {
                      String defaultContent = F ( "# timezone configuration - reboot for changes to take effect\r\n"
                                                  "# choose one of available (POSIX) timezones: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv\r\n\r\n"
                                                  "TZ " TZ "\r\n");
                    created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());
                    f.close ();

                    diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  }
                  Serial.printf (created ? "created\n" : "error\n");
              
              }
              {
                  Serial.printf ("[%10lu] [time][cronDaemon] reading /usr/share/zoneinfo\n", millis ());
                  // parse configuration file if it exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE];
                  strcpy (buffer, "\n");
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/usr/share/zoneinfo")) {
                      Serial.printf ("\n%s\n\n", buffer + 1);
                      strcat (buffer, "\n");
                      char __tz__ [100] = TZ;
                      strBetween (__tz__, sizeof (__ntpServer1__), buffer, "\nTZ ", "\n");

                      setenv ("TZ", __tz__, 1);
                      tzset ();
                      #ifdef __DMESG__
                          dmesg ("[time][cronDaemon] TZ environment variable set to ", __tz__);
                      #endif
                      Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon] TZ environment variable set to ", __tz__);

                  }
              }

              // read NTP configuration from /etc/ntp.conf, create a new one if it doesn't exist
              if (!fileSystem.isFile ((char *) "/etc/ntp.conf")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ((char *) "/etc")) { fileSystem.makeDirectory ((char *) "/etc"); }
                  Serial.printf ("[%10lu] [time][cronDaemon] /etc/ntp.conf does not exist, creating default one ... ", millis ());
                  bool created = false;
                  File f = fileSystem.open ((char *) "/etc/ntp.conf", "w", true);
                  if (f) {
                      String defaultContent = F ( "# configuration for NTP - reboot for changes to take effect\r\n\r\n"
                                                  "server1 " DEFAULT_NTP_SERVER_1 "\r\n"
                                                  "server2 " DEFAULT_NTP_SERVER_2 "\r\n"
                                                  "server3 " DEFAULT_NTP_SERVER_3 "\r\n");
                        created = (f.printf (defaultContent.c_str ()) == defaultContent.length ());
                      f.close ();

                      diskTrafficInformation.bytesWritten += defaultContent.length (); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way

                  }
                  Serial.printf (created ? "created\n" : "error\n");
              }
              {
                  Serial.printf ("[%10lu] [time][cronDaemon] reading /etc/ntp.conf\n", millis ());
                  // parse configuration file if it exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE];
                  strcpy (buffer, "\n");
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/ntp.conf")) {
                      Serial.printf ("\n%s\n\n", buffer + 1);
                      strcat (buffer, "\n");
                      strBetween (__ntpServer1__, sizeof (__ntpServer1__), buffer, "\nserver1 ", "\n");
                      strBetween (__ntpServer2__, sizeof (__ntpServer2__), buffer, "\nserver2 ", "\n");
                      strBetween (__ntpServer3__, sizeof (__ntpServer3__), buffer, "\nserver3 ", "\n");
                  }
              } 
              
              // read scheduled tasks from /etc/crontab
              if (!fileSystem.isFile ((char *) "/etc/crontab")) {
                  // create directory structure
                  if (!fileSystem.isDirectory ((char *) "/etc")) { fileSystem.makeDirectory ((char *) "/etc"); }          
                  Serial.printf ("[%10lu] [time][cronDaemon] /etc/crontab does not exist, creating default one ... ", millis ());
                  bool created = false;
                  File f = fileSystem.open ((char *) "/etc/crontab", "w", true);
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
                  Serial.printf (created ? "created\n" : "error\n");

              }
              {
                  Serial.printf ("[%10lu] [time][cronDaemon] reading /etc/crontab\n", millis ());
                  // parse scheduled tasks if /etc/crontab exists
                  char buffer [MAX_TZ_ETC_NTP_CONF_SIZE];
                  strcpy (buffer, "\n");
                  if (fileSystem.readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/crontab")) {
                      Serial.printf ("\n%s\n\n", buffer + 1);

                      strcat (buffer, "\n");
                      char *p, *q;
                      p = buffer + 1;
                      while (*p) {
                          if ((q = strstr (p, "\n"))) {
                              *q = 0;
                              if (*p) cronTabAdd (p, true);
                              p = q + 1;
                          }
                        }
                    }
                }
                
            } else {
                Serial.printf ("[%10lu] [time][cronDaemon] file system not mounted, can't read or write configuration files\n", millis ());
            }
        #else

            // set the default timezone
            setenv ("TZ", (char *) TZ, 1);
            tzset ();
            #ifdef __DMESG__
                dmesg ("[time][cronDaemon] TZ environment variable set to ", (char *) TZ);
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time][cronDaemon] TZ environment variable set to ", (char *) TZ);

            Serial.printf ("[%10lu] [time][cronDaemon] is starting without a file system\n", millis ());
        #endif    
        
        // run periodic synchronization with NTP servers and execute cron commands in a separate thread
        #define tskNORMAL_PRIORITY 1
        if (pdPASS != xTaskCreate (__cronDaemon__, "cronDaemon", CRON_DAEMON_STACK_SIZE, (void *) cronHandler, tskNORMAL_PRIORITY, NULL)) {
            #ifdef __DMESG__
                dmesg ("[time][cronDaemon] xTaskCreate error, could not start cronDaemon");
            #endif
            Serial.printf ("[%10lu] %s\r\n", millis (), "[time][cronDaemon] xTaskCreate error, could not start cronDaemon");
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
                dmesg ("[time] time corrected from ", buf);
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] time corrected from ", buf);
        } else {
            __startupTime__ = t - getUptime (); // recalculate            
            __timeHasBeenSet__ = true;
            ascTime (localTime (t), buf);
            #ifdef __DMESG__
                dmesg ("[time] time set to ", buf); 
            #endif
            Serial.printf ("[%10lu] %s%s\r\n", millis (), "[time] time set to ", buf);
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

    // conversion function: converts GMT in time_t to local time in time_t, returns 0 if internal clock is not set
    time_t timeToLocalTime (const time_t t) {
        if (t < 1687461154) return 0; // 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this

        // calculate local time in time_t and then the difference 
//        struct tm gst; gmtime_r (&t, &gst);    time_t gt = mktime (&gst);
        // struct tm lst; localtime_r (&t, &lst); time_t lt = mktime (&lst);

// Serial.printf ("        %lu %lu %lu         %02i:%02i:%02i    %02i:%02i:%02i\r\n", (unsigned long) t, (unsigned long) gt, (unsigned long) lt,  gst.tm_hour, gst.tm_min, gst.tm_sec,    lst.tm_hour, lst.tm_min, lst.tm_sec);

        //if (lt > t) return t - (lt - t);
        //if (lt < t) return t + (t - lt);
        return t;
    }

    // similar to time (void) but returns the local time instead of GMT
    time_t localTime () {
        time_t t = time (NULL);
        if (t < 1687461154) return 0; // 2023/06/22 21:12:34 is the time when I'm writing this code, any valid time should be greater than this
        return timeToLocalTime (t);
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
  
    [[deprecated("Use localTime (void) instead.")]]  
    time_t getLocalTime () { 
      return localTime ();
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
