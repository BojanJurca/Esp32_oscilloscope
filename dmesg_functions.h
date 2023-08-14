/*
 
    dmesg_functions.h

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

      - Use dmesg functions to put system messages in circular message queue.

      - Use dmesg telnet command to display messages in message queue.

    March 12, 2023, Bojan Jurca
    
*/


    // ----- includes, definitions and supporting functions -----
    
    #include <WiFi.h>
    #include <rom/rtc.h>
    // fixed size strings
    #include "fsString.h"


#ifndef __DMESG__
    #define __DMESG__


    // ----- TUNNING PARAMETERS -----

    #define DMESG_CIRCULAR_QUEUE_LENGTH 128 // how may massages we keep on circular queue
    #define DMESG_MAX_MESSAGE_LENGTH 88


    // ----- functions and variables in this modul -----

    void dmesg (char *message1, char *message2);
    void dmesg (char *message1, const char *message2);
    void dmesg (const char *message1, char *message2);
    void dmesg (const char *message1, const char *message2);
    void dmesg (char *message1);
    void dmesg (const char *message1);
    void dmesg (char *message1, int i);
    void dmesg (const char *message1, int i);
    void dmesg (const char *message1, int i, char *message2);
    const char *resetReason (RESET_REASON reason);
    const char *resetReason (RESET_REASON reason);
    fsString<DMESG_MAX_MESSAGE_LENGTH> wakeupReason ();


    // ----- CODE ----- 


    // find and report reset reason (this may help with debugging)
    const char *resetReason (RESET_REASON reason) {
        switch (reason) {
            case 1:  return "POWERON_RESET - 1, Vbat power on reset";
            case 3:  return "SW_RESET - 3, Software reset digital core";
            case 4:  return "OWDT_RESET - 4, Legacy watch dog reset digital core";
            case 5:  return "DEEPSLEEP_RESET - 5, Deep Sleep reset digital core";
            case 6:  return "SDIO_RESET - 6, Reset by SLC module, reset digital core";
            case 7:  return "TG0WDT_SYS_RESET - 7, Timer Group0 Watch dog reset digital core";
            case 8:  return "TG1WDT_SYS_RESET - 8, Timer Group1 Watch dog reset digital core";
            case 9:  return "RTCWDT_SYS_RESET - 9, RTC Watch dog Reset digital core";
            case 10: return "INTRUSION_RESET - 10, Instrusion tested to reset CPU";
            case 11: return "TGWDT_CPU_RESET - 11, Time Group reset CPU";
            case 12: return "SW_CPU_RESET - 12, Software reset CPU";
            case 13: return "RTCWDT_CPU_RESET - 13, RTC Watch dog Reset CPU";
            case 14: return "EXT_CPU_RESET - 14, for APP CPU, reseted by PRO CPU";
            case 15: return "RTCWDT_BROWN_OUT_RESET - 15, Reset when the vdd voltage is not stable";
            case 16: return "RTCWDT_RTC_RESET - 16, RTC Watch dog reset digital core and rtc module";
            default: return "RESET REASON UNKNOWN";
        }
    } 

    // find and report reset reason (this may help with debugging)
    fsString<DMESG_MAX_MESSAGE_LENGTH> wakeupReason () {
        esp_sleep_wakeup_cause_t wakeup_reason;
        wakeup_reason = esp_sleep_get_wakeup_cause ();
        switch (wakeup_reason){
            case ESP_SLEEP_WAKEUP_EXT0:     return "ESP_SLEEP_WAKEUP_EXT0 - wakeup caused by external signal using RTC_IO";
            case ESP_SLEEP_WAKEUP_EXT1:     return "ESP_SLEEP_WAKEUP_EXT1 - wakeup caused by external signal using RTC_CNTL";
            case ESP_SLEEP_WAKEUP_TIMER:    return "ESP_SLEEP_WAKEUP_TIMER - wakeup caused by timer";
            case ESP_SLEEP_WAKEUP_TOUCHPAD: return "ESP_SLEEP_WAKEUP_TOUCHPAD - wakeup caused by touchpad";
            case ESP_SLEEP_WAKEUP_ULP:      return "ESP_SLEEP_WAKEUP_ULP - wakeup caused by ULP program";
            default:                        return fsString<DMESG_MAX_MESSAGE_LENGTH> ("WAKEUP REASON UNKNOWN - wakeup was not caused by deep sleep: ") + fsString<DMESG_MAX_MESSAGE_LENGTH> ((int) wakeup_reason);
        }   
    }
    
    struct __dmesgType__ {
        unsigned long milliseconds;
        time_t        time;
        fsString<DMESG_MAX_MESSAGE_LENGTH> message;
    };
  
    __dmesgType__ __dmesgCircularQueue__ [DMESG_CIRCULAR_QUEUE_LENGTH];

    RTC_DATA_ATTR unsigned int bootCount = 0;

    bool __initializeDmesgCircularQueue__ () {
        __dmesgCircularQueue__ [0].message = fsString<DMESG_MAX_MESSAGE_LENGTH> ("[ESP32] CPU0 reset reason: ") + resetReason (rtc_get_reset_reason (0));
        __dmesgCircularQueue__ [1].message = fsString<DMESG_MAX_MESSAGE_LENGTH> ("[ESP32] CPU1 reset reason: ") + resetReason (rtc_get_reset_reason (1));
        __dmesgCircularQueue__ [2].message = fsString<DMESG_MAX_MESSAGE_LENGTH> ("[ESP32] wakeup reason: ") + wakeupReason ();
        __dmesgCircularQueue__ [3].message = fsString<DMESG_MAX_MESSAGE_LENGTH> ("[ESP32] (re)started ") + fsString<DMESG_MAX_MESSAGE_LENGTH> (++ bootCount) + (char *) " times";
        return true;      
    }
    bool __initializedDmesgCircularQueue__ = __initializeDmesgCircularQueue__ ();
  
    byte __dmesgBeginning__ = 0; // first used location
    byte __dmesgEnd__ = 4;       // the location next to be used
    static SemaphoreHandle_t __dmesgSemaphore__= xSemaphoreCreateMutex (); 
    
    // adds message to dmesg circular queue
    void dmesg (char *message1, char *message2) {
        // Serial.printf ("dmesg [%10lu] %s%s\n", millis (), message1, message2);
        xSemaphoreTake (__dmesgSemaphore__, portMAX_DELAY);
            __dmesgCircularQueue__ [__dmesgEnd__].milliseconds = millis ();
            struct timeval now; gettimeofday (&now, NULL); __dmesgCircularQueue__ [__dmesgEnd__].time = now.tv_sec >= 946681200 ? now.tv_sec : 0; // only if time is >= 1.1.2000
            __dmesgCircularQueue__ [__dmesgEnd__].message = "";
            __dmesgCircularQueue__ [__dmesgEnd__].message += message1;
            __dmesgCircularQueue__ [__dmesgEnd__].message += message2;
            if ((__dmesgEnd__ = (__dmesgEnd__ + 1) % DMESG_CIRCULAR_QUEUE_LENGTH) == __dmesgBeginning__) __dmesgBeginning__ = (__dmesgBeginning__ + 1) % DMESG_CIRCULAR_QUEUE_LENGTH;
        xSemaphoreGive (__dmesgSemaphore__);
    }
  
    // different versions of the same function
    void dmesg (char *message1, const char *message2)         { dmesg (message1, (char *) message2); }
    void dmesg (const char *message1, char *message2)         { dmesg ((char *) message1, message2); }
    void dmesg (const char *message1, const char *message2)   { dmesg ((char *) message1, (char *) message2); }
    void dmesg (char *message1)                               { dmesg (message1, (char *) ""); }
    void dmesg (const char *message1)                         { dmesg ((char *) message1, (char *) ""); }
    void dmesg (char *message1, int i)                        { dmesg (message1, fsString<DMESG_MAX_MESSAGE_LENGTH> (i)); }
    void dmesg (const char *message1, int i)                  { dmesg ((char *) message1, i); }
    void dmesg (const char *message1, int i, char *message2)  { dmesg (message1, fsString<DMESG_MAX_MESSAGE_LENGTH> (i) + (char *) " " + message2); }

#endif
