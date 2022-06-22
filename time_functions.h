  /*

    time_functions.h
 
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    Real_time_clock synchronizes its time with NTP server accessible from internet once a day. Internet connection is 
    necessary for real_time_clock to work.

    October 4, 2020, Bojan Jurca

    Nomenclature used in time_functions.h - for easier understaning of the code:

      - "buffer size" is the number of bytes that can be placed in a buffer. 

                  In case of C 0-terminated strings the terminating 0 character should be included in "buffer size".      

      - "max length" is the number of characters that can be placed in a variable.

                  In case of C 0-terminated strings the terminating 0 character is not included so the buffer should be at least 1 byte larger.    
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>


#ifndef __TIME_FUNCTIONS__
  #define __TIME_FUNCTIONS__


    // ----- functions and variables in this modul -----

    void setGmt (time_t);
    time_t getGmt ();
    void setLocalTime (time_t);
    time_t getLocalTime ();
    time_t timeToLocalTime (time_t t);
    struct tm timeToStructTime (time_t t);
    bool timeToString (char *, size_t, time_t);
    String timeToString (time_t);
    time_t getUptime ();

    char *ntpDate (char *);
    char *ntpDate ();

    bool cronTabAdd (uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, char *, bool);
    bool cronTabAdd (char *, bool);
    int cronTabDel (char *);
    String cronTab ();
    void startCronDaemon (void (* cronHandler) (char *), size_t);

  
    // TUNNING PARAMETERS

    // time_functions tuning #definitions, change if needed but keep in mind that all this structures reside the stack - don't waste its space too much:

    // choose your time zone
          #define KAL_TIMEZONE 10                     // GMT + 2 ----- Russia time zones ----- // https://en.wikipedia.org/wiki/Time_in_Russia
          #define MSK_TIMEZONE 11                     // GMT + 3
          #define SAM_TIMEZONE 12                     // GMT + 4
          #define YEK_TIMEZONE 13                     // GMT + 5
          #define OMS_TIMEZONE 14                     // GMT + 6
          #define KRA_TIMEZONE 15                     // GMT + 7
          #define IRK_TIMEZONE 16                     // GMT + 8
          #define YAK_TIMEZONE 17                     // GMT + 9
          #define VLA_TIMEZONE 18                     // GMT + 10
          #define SRE_TIMEZONE 19                     // GMT + 11
          #define PET_TIMEZONE 20                     // GMT + 12
          #define JAPAN_TIMEZONE 21                   // GMT + 9  ----- Japan time zone -----
          #define CHINA_TIMEZONE 22                   // GMT + 8  ----- China time zone -----
          #define WET_TIMEZONE 23                     // Western European Time (GMT + DST from March to October) ----- Europe time zones -----  https://www.timeanddate.com/time/europe/
          #define ICELAND_TIMEZONE 24                 // same as WET_TIMEZONE but without DST (GMT, no DST)
          #define CET_TIMEZONE 25                     // Central European Time (GMT + 1 + DST from March to October)
          #define EET_TIMEZONE 26                     // Eastern European Time (GMT + 2 + DST from March to October)
          #define FET_TIMEZONE 27                     // Further-Eastern European Time (GMT + 3, no DST)
          #define NEWFOUNDLAND_TIMEZONE 28            // GMT - 3.5 + DST from March to November ----- USA & Canada time zones ---- https://en.wikipedia.org/wiki/Time_in_the_United_States
          #define ATLANTIC_TIMEZONE 29                // GMT - 4 + DST from March to November
          #define ATLANTIC_NO_DST_TIMEZONE 30         // GMT - 4, no DST
          #define EASTERN_TIMEZONE 31                 // GMT - 5 + DST from March to November
          #define EASTERN_NO_DST_TIMEZONE 32          // GMT - 5
          #define CNTRAL_TIMEZONE 33                  // GMT - 6 + DST from March to November
          #define CNTRAL_NO_DST_TIMEZONE 34           // GMT - 6 
          #define MOUNTAIN_TIMEZONE 35                // GMT - 7 + DST from March to November
          #define MOUNTAIN_NO_DST_TIMEZONE 36         // GMT - 7
          #define PACIFIC_TIMEZONE 37                 // GMT - 8 + DST from March to November
          #define ALASKA_TIMEZNE 38                   // GMT - 9 + DST from March to November
          #define HAWAII_ALEUTIAN_TIMEZONE 39         // GMT - 10 + DST from March to November
          #define HAWAII_ALEUTIAN_NO_DST_TIMEZONE 40  // GMT - 10
          #define AMERICAN_SAMOA_TIMEZONE 41          // GMT - 11
          #define BAKER_HOWLAND_ISLANDS_TIMEZONE 42   // GMT - 12
          #define WAKE_ISLAND_TIMEZONE 43             // GMT + 12
          #define CHAMORRO_TIMEZONE 44                // GMT + 10               
          // ... add more time zones or change timeToLocalTime function yourself
    #ifndef TIMEZONE
      #define TIMEZONE                CET_TIMEZONE    // one of the above
    #endif

    // NTP servers that ESP32 server is going to sinchronize its time with
    #ifndef DEFAULT_NTP_SERVER_1
      #define DEFAULT_NTP_SERVER_1    "1.si.pool.ntp.org"
    #endif
    #ifndef DEFAULT_NTP_SERVER_2
      #define DEFAULT_NTP_SERVER_2    "2.si.pool.ntp.org"
    #endif
    #ifndef DEFAULT_NTP_SERVER_3
      #define DEFAULT_NTP_SERVER_3    "3.si.pool.ntp.org"
    #endif
    #define MAX_ETC_NTP_CONF_SIZE 1 * 1024            // 1 KB will usually do - initialization reads the whole /etc/ntp.conf file in the memory 
                                                      
    // crontab definitions
    #define CRON_DAEMON_STACK_SIZE 8 * 1024
    #define CRONTAB_MAX_ENTRIES 32                    // this defines the size of cron table - increase this number if you going to handle more cron events
    #define CRON_COMMAND_MAX_LENGTH 48                // the number of characters in the longest cron command
    #define ANY 255                                   // byte value that corresponds to * in cron table entries (should be >= 60 so it can be distinguished from seconds, minutes, ...)
    #define MAX_ETC_CRONTAB_SIZE CRONTAB_MAX_ENTRIES * (20 + CRON_COMMAND_MAX_LENGTH) // this should do
  

    // ----- CODE -----

    #include "dmesg_functions.h"
    
    #ifdef __FILE_SYSTEM__
      #ifndef __STR_BETWEEN__  
        #define __STR_BETWEEN__
        String strBetween (String input, String openingBracket, String closingBracket) { // returns content inside of opening and closing brackets
          int i = input.indexOf (openingBracket);
          if (i >= 0) {
            input = input.substring (i +  openingBracket.length ());
            i = input.indexOf (closingBracket);
            if (i >= 0) {
              input = input.substring (0, i);
              input.trim ();
              return input;
            }
          }
          return "";
        }
  
        char *strBetween (char *buffer, size_t bufferSize, char *src, const char *left, const char *right) { // copies substring of src between left and right to buffer or "" if not found or buffer too small, return bufffer
          *buffer = 0;
          char *l, *r;
          if ((l = strstr (src, left))) {  
            l += strlen (left);
            if ((r = strstr (l, right))) { 
              int len = r - l;
              if (len < bufferSize - 1) { 
                strncpy (buffer, l, len); 
                buffer [len] = 0; 
              }
            }
          }    
          return buffer;                                                     
        }
      #endif      
    #endif

    time_t __readBuiltInClock__ () {
      struct timeval now;
      gettimeofday (&now, NULL);
      return now.tv_sec;    
    }
  
    RTC_DATA_ATTR bool __timeHasBeenSet__ = false;
    RTC_DATA_ATTR time_t __startupTime__ = 0;
  
    static SemaphoreHandle_t __cronSemaphore__ = xSemaphoreCreateRecursiveMutex (); // controls access to cronDaemon's variables
    static SemaphoreHandle_t __timeSemaphore__ = xSemaphoreCreateMutex (); // controls access to real_time_clock critical section

    char __ntpServer1__ [254] = DEFAULT_NTP_SERVER_1; // host name may have max 253 characters
    char __ntpServer2__ [254] = DEFAULT_NTP_SERVER_2; // host name may have max 253 characters
    char __ntpServer3__ [254] = DEFAULT_NTP_SERVER_3; // host name may have max 253 characters 
  
    char *ntpDate (char *ntpServer) { // synchronizes time with NTP server, returns error message
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
        if (currentTime < 946684800) { udp.stop (); return (char *) "Wrong NTP reply."; }
        udp.stop ();
        dmesg ("[time] synchronized with ", ntpServer);
        setGmt (currentTime);
        break;
      }
      return (char *) ""; // OK  
    }
  
    char *ntpDate () { // synchronizes time with NTP servers, returns error message
      char *s;
      s = ntpDate (__ntpServer1__); if (!*s) return (char *) ""; else dmesg (s); delay (1);
      s = ntpDate (__ntpServer2__); if (!*s) return (char *) ""; else dmesg (s); delay (1);
      s = ntpDate (__ntpServer3__); if (!*s) return (char *) ""; else dmesg (s); delay (1);
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
    dmesg ("[cronDaemon][cronTabAdd] can't add cronCommand, cron table is full: ", cronCommand);
    return false;
  }
  
  bool cronTabAdd (char *cronTabLine, bool readFromFile = false) { // parse cronTabLine and then call the function above
    char second [3]; char minute [3]; char hour [3]; char day [3]; char month [3]; char day_of_week [3]; char cronCommand [65];
    if (sscanf (cronTabLine, "%2s %2s %2s %2s %2s %2s %64s", second, minute, hour, day, month, day_of_week, cronCommand) == 7) {
      int8_t se = strcmp (second, "*") ? atoi (second) : ANY; if ((!se && *second != '0') || se > 59) { dmesg ("[cronDaemon][cronAdd] invalid second: ", second); return false; }
      int8_t mi = strcmp (minute, "*") ? atoi (minute) : ANY; if ((!mi && *minute != '0') || mi > 59) { dmesg ("[cronDaemon][cronAdd] invalid minute: ",  minute); return false; }
      int8_t hr = strcmp (hour, "*") ? atoi (hour) : ANY; if ((!hr && *hour != '0') || hr > 23) { dmesg ("[cronDaemon][cronAdd] invalid hour: ",  hour); return false; }
      int8_t dm = strcmp (day, "*") ? atoi (day) : ANY; if (!dm || dm > 31) { dmesg ("[cronDaemon][cronAdd] invalid day: ", day); return false; }
      int8_t mn = strcmp (month, "*") ? atoi (month) : ANY; if (!mn || mn > 12) { dmesg ("[cronDaemon][cronAdd] invalid month: ", month); return false; }
      int8_t dw = strcmp (day_of_week, "*") ? atoi (day_of_week) : ANY; if ((!dw && *day_of_week != '0') || dw > 7) { dmesg ("[cronDaemon][cronAdd] invalid day of week: ", day_of_week); return false; }
      if (!*cronCommand) { dmesg ("[cronDaemon] [cronAdd] missing cron command"); return false; }
      return cronTabAdd (se, mi, hr, dm, mn, dw, cronCommand, readFromFile);
    } else {
      dmesg ("[cronDaemon][cronAdd] invalid cronTabLine: ", cronTabLine);
      return false;
    }
  }
    
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
    if (!cnt) dmesg ("[cronDaemon][cronTabDel] cronCommand not found: ", cronCommand);
    return cnt;
  }

  // TO DO: get rod of Strings
  String cronTab () { // returns the whole crontab content as a string
    String s = "";
    xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
      if (!__cronTabEntries__) {
        s = "crontab is empty.";
      } else {
        for (int i = 0; i < __cronTabEntries__; i ++) {
          if (s != "") s += "\r\n";
          char c [4]; 
          if (__cronEntry__ [i].second == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].second); s += String (c); }
          if (__cronEntry__ [i].minute == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].minute); s += String (c); }
          if (__cronEntry__ [i].hour == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].hour); s += String (c); }
          if (__cronEntry__ [i].day == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].day); s += String (c); }
          if (__cronEntry__ [i].month == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].month); s += String (c); }
          if (__cronEntry__ [i].day_of_week == ANY) s += " * "; else { sprintf (c, "%2i ", __cronEntry__ [i].day_of_week); s += String (c); }
          if (__cronEntry__ [i].lastExecuted) s += " " + timeToString (__cronEntry__ [i].lastExecuted) + " "; else s += " (not executed yet)  ";
          if (__cronEntry__ [i].readFromFile) s += " from /etc/crontab  "; else s += " entered from code  ";
          s += __cronEntry__ [i].cronCommand;
        }
      }
    xSemaphoreGiveRecursive (__cronSemaphore__);
    return s;    
  }

  void __cronDaemon__ (void *ptrCronHandler) { // it does two things: it synchronizes time with NTP servers once a day and executes cron commands from cron table when the time is right
    dmesg ("[cronDaemon] started");
    do {     // try to set/synchronize the time, retry after 1 minute if unsuccessfull 
       delay (15000);
       if (!*ntpDate ()) break; // success
       delay (45000);
    } while (!getGmt ());

    void (* cronHandler) (char *) = (void (*) (char *)) ptrCronHandler;  
    unsigned long lastSyncMillis = millis (); 

    while (true) {
      delay (10);

      // 1. synchronize time with NTP servers
      if (millis () - lastSyncMillis >= 86400000) { ntpDate (); lastSyncMillis = millis (); } 

      // 2. execute cron commands from cron table
      time_t now = getLocalTime ();
      if (!now) continue; // if the time is not known cronDaemon can't do anythig
      static time_t previous = now;
      for (time_t l = previous + 1; l <= now; l++) {
        delay (1);
        struct tm slt = timeToStructTime (l);
        //scan through cron entries and find commands that needs to be executed (at time l)
        xSemaphoreTakeRecursive (__cronSemaphore__, portMAX_DELAY);
          // mark cron commands for execution
          for (int i = 0; i < __cronTabEntries__; i ++) {
            if ( (__cronEntry__ [i].second == ANY || __cronEntry__ [i].second == slt.tm_sec) && 
                 (__cronEntry__ [i].minute == ANY || __cronEntry__ [i].minute == slt.tm_min) &&
                 (__cronEntry__ [i].hour == ANY || __cronEntry__ [i].hour == slt.tm_hour) &&
                 (__cronEntry__ [i].day == ANY || __cronEntry__ [i].day == slt.tm_mday) &&
                 (__cronEntry__ [i].month == ANY || __cronEntry__ [i].month == slt.tm_mon + 1) &&
                 (__cronEntry__ [i].day_of_week == ANY || __cronEntry__ [i].day_of_week == slt.tm_wday || __cronEntry__ [i].day_of_week == slt.tm_wday + 7) ) {

                      if (!__cronEntry__ [i].executed) {
                        __cronEntry__ [i].markForExecution = true;
                        // DEBUG: Serial.printf ("[cronDaemon] %s marked for execution\n", __cronEntry__ [i].cronCommand);
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
                        // DEBUG: Serial.printf ("[cronDaemon] %s executed\n", __cronEntry__ [i].cronCommand);
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

  void startCronDaemon (void (* cronHandler) (char *)) { // synchronizes time with NTP servers from /etc/ntp.conf, reads /etc/crontab, returns error message. If /etc/ntp.conf or /etc/crontab don't exist (yet) creates the default one
    Serial.printf ("[%10lu] [cronDaemon] starting ...\n", millis ());
    #ifdef __FILE_SYSTEM__
      if (__fileSystemMounted__) {
        // create directory structure
        if (!isDirectory ("/etc")) { fileSystem.mkdir ("/etc"); }
        // read NTP configuration from /etc/ntp.conf, create a new one if it doesn't exist
        if (!isFile ("/etc/ntp.conf")) {
          Serial.printf ("[%10lu] [cronDaemon] /etc/ntp.conf does not exist, creating default one ... ", millis ());
          bool created = false;
          File f = fileSystem.open ("/etc/ntp.conf", FILE_WRITE);
          if (f) {
            char *defaultContent = (char *) "# configuration for NTP - reboot for changes to take effect\r\n\r\n"
                                            "server1 " DEFAULT_NTP_SERVER_1 "\r\n"
                                            "server2 " DEFAULT_NTP_SERVER_2 "\r\n"
                                            "server3 " DEFAULT_NTP_SERVER_3 "\r\n";
            created = (f.printf (defaultContent) == strlen (defaultContent));
            f.close ();
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (defaultContent); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif            
          }
          Serial.printf (created ? "created\n" : "error\n");

        } else {
          Serial.printf ("[%10lu] [cronDaemon] reading /etc/ntp.conf\n", millis ());
          // parse configuration file if it exists
          char buffer [MAX_ETC_NTP_CONF_SIZE];
          strcpy (buffer, "\n");
          if (readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/ntp.conf")) {
            strcat (buffer, "\n");
            strBetween (__ntpServer1__, sizeof (__ntpServer1__), buffer, "\nserver1 ", "\n");
            strBetween (__ntpServer2__, sizeof (__ntpServer2__), buffer, "\nserver2 ", "\n");
            strBetween (__ntpServer3__, sizeof (__ntpServer3__), buffer, "\nserver3 ", "\n");
          } 
        }
        // read scheduled tasks from /etc/crontab
        if (!isFile ("/etc/crontab")) {
          Serial.printf ("[%10lu] [cronDaemon] /etc/crontab does not exist, creating default one ... ", millis ());
          bool created = false;
          File f = fileSystem.open ("/etc/crontab", FILE_WRITE);          
          if (f) {
            char *defaultContent = (char *) "# scheduled tasks (in local time) - reboot for changes to take effect\r\n"
                                            "#\r\n"
                                            "# .------------------- second (0 - 59 or *)\r\n"
                                            "# |  .---------------- minute (0 - 59 or *)\r\n"
                                            "# |  |  .------------- hour (0 - 23 or *)\r\n"
                                            "# |  |  |  .---------- day of month (1 - 31 or *)\r\n"
                                            "# |  |  |  |  .------- month (1 - 12 or *)\r\n"
                                            "# |  |  |  |  |  .---- day of week (0 - 7 or *; Sunday=0 and also 7)\r\n"
                                            "# |  |  |  |  |  |\r\n"
                                            "# *  *  *  *  *  * cronCommand to be executed\r\n"
                                            "# * 15  *  *  *  * exampleThatRunsQuaterPastEachHour\r\n";
            created = (f.printf (defaultContent) == strlen (defaultContent));                                
            f.close ();
            #ifdef __PERFMON__
              __perfFSBytesWritten__ += strlen (defaultContent); // update performance counter without semaphore - values may not be perfectly exact but we won't loose time this way
            #endif                        
          }
          Serial.printf (created ? "created\n" : "error\n");

        } else {
          Serial.printf ("[%10lu] [cronDaemon] reading /etc/crontab\n", millis ());
          // parse scheduled tasks if /etc/crontab exists
          char buffer [MAX_ETC_CRONTAB_SIZE];
          strcpy (buffer, "\n");
          if (readConfigurationFile (buffer + 1, sizeof (buffer) - 3, (char *) "/etc/crontab")) {
            strcat (buffer, "\n");
            char *p, *q;
            p = buffer + 1;
            while (*p) {
              if ((q = strstr (p, "\n"))) {
                *q = 0;
                cronTabAdd (p, true);
                p = q + 1;
              }
            }
          }
        }
        
      } else {
        Serial.printf ("[%10lu] [cronDaemon] file system not mounted, can't read or write configuration files\n", millis ());
      }
    #else
      Serial.printf ("[%10lu] [cronDaemon] is starting without a file system\n", millis ());
    #endif    
    
    // run periodic synchronization with NTP servers and execute cron commands in a separate thread
    #define tskNORMAL_PRIORITY 1
    if (pdPASS != xTaskCreate (__cronDaemon__, "cronDaemon", CRON_DAEMON_STACK_SIZE, (void *) cronHandler, tskNORMAL_PRIORITY, NULL)) dmesg ("[cronDaemon] xTaskCreate error, could not start cronDaemon");
  }
 
  time_t getGmt () { // returns current GMT or 0 it the time has not been set yet 
    return __timeHasBeenSet__ ? __readBuiltInClock__() : 0;
  }

  time_t getUptime () {
    return __timeHasBeenSet__ ? __readBuiltInClock__ () - __startupTime__ :  millis () / 1000;
  }
  
  void setGmt (time_t t) { // also sets/corrects __startupTime__
    __startupTime__ = t - getUptime (); // recalculate
    
    struct timeval oldTime;
    gettimeofday (&oldTime, NULL);
    struct timeval newTime = {t, 0};
    settimeofday (&newTime, NULL); 

    char buffer [100];
    if (__timeHasBeenSet__) {
      timeToString (buffer, sizeof (buffer), timeToLocalTime (oldTime.tv_sec));
      strcat (buffer, " to ");
      timeToString (buffer + strlen (buffer), sizeof (buffer) - strlen (buffer), timeToLocalTime (t));
      dmesg ("[time] time corrected from ", buffer);
    } else {
      __timeHasBeenSet__ = true;
      timeToString (buffer, sizeof (buffer), timeToLocalTime (t));
      dmesg ("[time] time set to ", buffer); 
    }
  }

  time_t getLocalTime () { // returns current local time or 0 it the time has not been set yet 
    return __timeHasBeenSet__ ? timeToLocalTime (__readBuiltInClock__ ()) : 0;
  }

  void setLocalTime (time_t t) { 
    setGmt (t -= timeToLocalTime (t) - t); // convert local time into GMT, then set GMT
  }

  #if TIMEZONE == KAL_TIMEZONE // GMT + 2
    time_t timeToLocalTime (time_t t) { return t + 2 * 3600; } 
  #endif
  #if TIMEZONE == MSK_TIMEZONE // GMT + 3
    time_t timeToLocalTime (time_t t) { return t + 3 * 3600; } 
  #endif
  #if TIMEZONE == SAM_TIMEZONE // GMT + 4
    time_t timeToLocalTime (time_t t) { return t + 4 * 3600; } 
  #endif
  #if TIMEZONE == YEK_TIMEZONE // GMT + 5
    time_t timeToLocalTime (time_t t) { return t + 5 * 3600; } 
  #endif
  #if TIMEZONE == OMS_TIMEZONE // GMT + 6
    time_t timeToLocalTime (time_t t) { return t + 6 * 3600; } 
  #endif
  #if TIMEZONE == KRA_TIMEZONE // GMT + 7
    time_t timeToLocalTime (time_t t) { return t + 7 * 3600; } 
  #endif
  #if TIMEZONE == IRK_TIMEZONE // GMT + 8
    time_t timeToLocalTime (time_t t) { return t + 8 * 3600; } 
  #endif
  #if TIMEZONE == YAK_TIMEZONE // GMT + 9
    time_t timeToLocalTime (time_t t) { return t + 9 * 3600; } 
  #endif
  #if TIMEZONE == VLA_TIMEZONE // GMT + 10
    time_t timeToLocalTime (time_t t) { return t + 10 * 3600; } 
  #endif
  #if TIMEZONE == SRE_TIMEZONE // GMT + 11
    time_t timeToLocalTime (time_t t) { return t + 11 * 3600; } 
  #endif
  #if TIMEZONE == PET_TIMEZONE // GMT + 12
    time_t timeToLocalTime (time_t t) { return t + 12 * 3600; } 
  #endif

  #if TIMEZONE == JAPAN_TIMEZONE // GMT + 9
    time_t timeToLocalTime (time_t t) { return t + 9 * 3600; } 
  #endif
  
  #if TIMEZONE == CHINA_TIMEZONE  // GMT + 8
    time_t timeToLocalTime (time_t t) { return t + 8 * 3600; } 
  #endif

  // in Europe, time change is defined according to GMT:
  #define PAST_EUROPE_SPRING_TIME_CHANGE (tStr.tm_mon > 2 /* Apr-Dec */ || (tStr.tm_mon == 2 /* Mar */ && tStr.tm_mday - tStr.tm_wday >= 23 /* last Sun or latter */ && (tStr.tm_wday > 0 /* Mon-Sat */ || (tStr.tm_wday == 0 /* Sun */ && tStr.tm_hour >= 1 /* >= 1:00 GMT */))))
  #define PAST_EUROPE_AUTUMN_TIME_CHANGE (tStr.tm_mon > 9 /* Nov-Dec */ || (tStr.tm_mon == 9 /* Oct */ && tStr.tm_mday - tStr.tm_wday >= 23 /* last Sun or latter */ && (tStr.tm_wday > 0 /* Mon-Sat */ || (tStr.tm_wday == 0 /* Sun */ && tStr.tm_hour >= 1 /* >= 1:00 GMT */))))

  #if TIMEZONE == WET_TIMEZONE // GMT + DST 
    time_t timeToLocalTime (time_t t) {
      time_t l = t;
      struct tm tStr = timeToStructTime (t);
      if (PAST_EUROPE_SPRING_TIME_CHANGE && !PAST_EUROPE_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == ICELAND_TIMEZONE // GMT
    time_t timeToLocalTime (time_t t) { return t; } 
  #endif
  #if TIMEZONE == CET_TIMEZONE // GMT + 1 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t;
      struct tm tStr = timeToStructTime (t);
      if (PAST_EUROPE_SPRING_TIME_CHANGE && !PAST_EUROPE_AUTUMN_TIME_CHANGE) l += 3600;   
      return l + 3600;
    }
  #endif
  #if TIMEZONE == EET_TIMEZONE // GMT + 2 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t;
      struct tm tStr = timeToStructTime (t);
      if (PAST_EUROPE_SPRING_TIME_CHANGE && !PAST_EUROPE_AUTUMN_TIME_CHANGE) l += 3600;   
      return l + 2 * 3600;
    }
  #endif
  #if TIMEZONE == FET_TIMEZONE // GMT + 3 
    time_t timeToLocalTime (time_t t) { return t + 3 * 3600; } 
  #endif

  // in North America, time change is defined according to local time:
  #define PAST_NORTH_AMERICA_SPRING_TIME_CHANGE (lStr.tm_mon > 2 /* Apr-Dec */ || (lStr.tm_mon == 2 /* Mar */ && lStr.tm_mday - lStr.tm_wday >= 8  /* 2 nd Sun or latter */ && (lStr.tm_wday > 0 /* Mon-Sat */ || (lStr.tm_wday == 0 /* Sun */ && lStr.tm_hour >= 2 /* >= 2:00 */))))  
  #define PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE (lStr.tm_mon > 10 /* Dec */    || (lStr.tm_mon == 10 /* Nov */ && lStr.tm_mday - lStr.tm_wday >= 1 /* 1 st Sun or latter */ && (lStr.tm_wday > 0 /* Mon-Sat */ || (lStr.tm_wday == 0 /* Sun */ && lStr.tm_hour >= 1 /* >= 1:00 (without considering DST) */))))

  #if TIMEZONE == NEWFOUNDLAND_TIMEZONE // GMT - 3.5 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 12600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == ATLANTIC_TIMEZONE // GMT -4 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 4 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == ATLANTIC_NO_DST_TIMEZONE // GMT - 4
    time_t timeToLocalTime (time_t t) { return t - 4 * 3600; }
  #endif
  #if TIMEZONE == EASTERN_TIMEZONE // GMT - 5 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 5 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == EASTERN_NO_DST_TIMEZONE // GMT - 5
    time_t timeToLocalTime (time_t t) { return t - 5 * 3600; }
  #endif
  #if TIMEZONE == CENTRAL_TIMEZONE // GMT - 6 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 6 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == CENTRAL_NO_DST_TIMEZONE // GMT - 6
    time_t timeToLocalTime (time_t t) { return t - 6 * 3600; }
  #endif
  #if TIMEZONE == MOUNTAIN_TIMEZONE // GMT - 7 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 7 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }
  #endif
  #if TIMEZONE == MOUNTAIN_NO_DST_TIMEZONE // GMT - 7
    time_t timeToLocalTime (time_t t) { return t - 7 * 3600; }
  #endif  
  #if TIMEZONE == PACIFIC_TIMEZONE // GMT - 8 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 8 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }  
  #endif
  #if TIMEZONE == ALASKA_TIMEZONE // GMT - 9 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 9 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }  
  #endif
  #if TIMEZONE == HAWAII_ALEUTIAN_TIMEZONE // GMT - 10 + DST
    time_t timeToLocalTime (time_t t) {
      time_t l = t - 10 * 3600;
      struct tm lStr = timeToStructTime (l);
      if (PAST_NORTH_AMERICA_SPRING_TIME_CHANGE && !PAST_NORTH_AMERICA_AUTUMN_TIME_CHANGE) l += 3600;   
      return l;
    }  
  #endif
  #if TIMEZONE == HAWAII_ALEUTIAN_NO_DST_TIMEZONE // GMT - 10
    time_t timeToLocalTime (time_t t) { return t - 10 * 3600; }
  #endif  
  #if TIMEZONE == AMERICAN_SAMOA_TIMEZONE // GMT - 11
    time_t timeToLocalTime (time_t t) { return t - 11 * 3600; }
  #endif
  #if TIMEZONE == BAKER_HOWLAND_ISLANDS_TIMEZONE // GMT - 12
    time_t timeToLocalTime (time_t t) { return t - 12 * 3600; }
  #endif
  #if TIMEZONE == WAKE_ISLAND_TIMEZONE // GMT + 12
    time_t timeToLocalTime (time_t t) { return t + 12 * 3600; } 
  #endif
  #if TIMEZONE == CHAMORRO_TIMEZONE // GMT + 10
    time_t timeToLocalTime (time_t t) { return t + 10 * 3600; }
  #endif

  struct tm timeToStructTime (time_t t) { 
    xSemaphoreTake (__timeSemaphore__, portMAX_DELAY); // gmtime (&time_t) returns pointer to static structure which may be a problem in multi-threaded environment
      struct tm timeStr = *gmtime (&t);
    xSemaphoreGive (__timeSemaphore__);
    return timeStr;                                         
  }

  bool timeToString (char *buffer, size_t bufferSize, time_t t) {
    *buffer = 0;
    struct tm timeStr = timeToStructTime (t);
    char s [25];
    sprintf (s, "%04i/%02i/%02i %02i:%02i:%02i", 1900 + timeStr.tm_year, 1 + timeStr.tm_mon, timeStr.tm_mday, timeStr.tm_hour, timeStr.tm_min, timeStr.tm_sec);
    if (strlen (s) < bufferSize - 1) {
      strcpy (buffer, s);
      return true;    
    }
    return false;
  }  

  String timeToString (time_t t) {
    struct tm timeStr = timeToStructTime (t);
    char s [25];
    sprintf (s, "%04i/%02i/%02i %02i:%02i:%02i", 1900 + timeStr.tm_year, 1 + timeStr.tm_mon, timeStr.tm_mday, timeStr.tm_hour, timeStr.tm_min, timeStr.tm_sec);
    return String (s);
  }  

  // TESTING CODE

  void __TEST_DST_TIME_CHANGE__ () {
    // FOR CET_TIMEZONE
    Serial.printf ("\n      Testing end of DST interval in CET_TIMEZONE\n");
    Serial.printf ("      GMT (UNIX) | GMT               | Local             | Local - GMT [s]\n");
    Serial.printf ("      ----------------------------------------------------------------\n");
    for (time_t testTime = 1616893200 - 2; testTime < 1616893200 + 2; testTime ++) { 
      setGmt (testTime);
      time_t localTestTime = getLocalTime ();
      Serial.printf ("      %llu | ", (unsigned long long) testTime);
      char s [50];
      strftime (s, 50, "%Y/%m/%d %H:%M:%S", gmtime (&testTime));
      Serial.printf ("%s | ", s);
      strftime (s, 50, "%Y/%m/%d %H:%M:%S", gmtime (&localTestTime));
      Serial.printf ("%s | ", s);
      Serial.printf ("%i\n", (int) (localTestTime - testTime));
    }
    for (time_t testTime = 1635642000 - 2; testTime < 1635642000 + 2; testTime ++) {
      setGmt (testTime);
      time_t localTestTime = getLocalTime ();
      Serial.printf ("      %llu | ", (unsigned long long) testTime);
      char s [50];
      strftime (s, 50, "%Y/%m/%d %H:%M:%S", gmtime (&testTime));
      Serial.printf ("%s | ", s);
      strftime (s, 50, "%Y/%m/%d %H:%M:%S", gmtime (&localTestTime));
      Serial.printf ("%s | ", s);
      Serial.printf ("%i\n", (int) (localTestTime - testTime));
    }
  }  
  
#endif
