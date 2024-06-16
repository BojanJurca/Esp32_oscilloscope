/*
 
    dmesg.hpp

    This file is part of Multitasking Esp32 HTTP FTP Telnet servers for Arduino project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino

      - Use dmesgQueue.push_back (...) functions to put system messages in circular message queue.

      - Use dmesg telnet command to display messages in the dmesg message queue.

    May 22, 2024, Bojan Jurca
    
*/


    // ----- includes, definitions and supporting functions -----
    
    #include <rom/rtc.h>
    // Cstrings
    #include "std/Cstring.hpp"
    #include "threadSafeCircularQueue.hpp"


#ifndef __DMESG__
    #define __DMESG__

    // ----- TUNNING PARAMETERS -----

    #define DMESG_MAX_MESSAGE_LENGTH 82     // max length of each message
    #define DMESG_CIRCULAR_QUEUE_LENGTH 100 // how may massages we keep on circular queue


    // keep the following information for each entry 
    struct dmesgQueueEntry_t {
        unsigned long milliseconds;
        time_t        time;
        Cstring<DMESG_MAX_MESSAGE_LENGTH> message; 

        template<typename T>
        dmesgQueueEntry_t& operator << (const T& value) {
            this->message += value;
            return *this;
        }

        dmesgQueueEntry_t& operator << (const char *value) {
            this->message += value;
            return *this;
        }             

        dmesgQueueEntry_t& operator << (const String& value) {
            this->message += (const char *) value.c_str ();
            return *this;
        }             

        template<size_t N>
        dmesgQueueEntry_t& operator << (const Cstring<N>& value) {
            this->message += (char *) &value;
            return *this;
        }             
    };


    // define dmesg circular queue 
    template<size_t maxSize> class dmesgQueue_t : public threadSafeCircularQueue<dmesgQueueEntry_t, maxSize> {

        public:

            // constructor - insert the first entries
            dmesgQueue_t () : threadSafeCircularQueue<dmesgQueueEntry_t, maxSize> () { 

                #if CONFIG_FREERTOS_UNICORE // CONFIG_FREERTOS_UNICORE == 1 => 1 core ESP32
                    this->operator<< ("[ESP32] CPU0 reset reason: ") << __resetReason__ (rtc_get_reset_reason (0));
                #else // CONFIG_FREERTOS_UNICORE == 0 => 2 core ESP32
                    this->operator<< ("[ESP32] CPU0 reset reason: ") << __resetReason__ (rtc_get_reset_reason (0));
                    this->operator<< ("[ESP32] CPU0 reset reason: ") << __resetReason__ (rtc_get_reset_reason (1));
                #endif            
        
                this->operator<< ("[ESP32] wakeup reason: ") << __wakeupReason__ ();
            }

            template<typename T>
            dmesgQueueEntry_t& operator << (const T& value) {
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Lock ();
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::push_back ( { millis (), time (NULL), {} } );
                dmesgQueueEntry_t *p = threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::back ();
                p->message = value;
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Unlock ();
                return *p;
            }          

            dmesgQueueEntry_t& operator << (const char *value) {
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Lock ();
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::push_back ( { millis (), time (NULL), {} } );
                dmesgQueueEntry_t *p = threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::back ();
                p->message = value;
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Unlock ();
                return *p;
            }             

            dmesgQueueEntry_t& operator << (const String& value) {
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Lock ();
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::push_back ( { millis (), time (NULL), {} } );
                dmesgQueueEntry_t *p = threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::back ();
                p->message = (const char *) value.c_str ();
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Unlock ();
                return *p;
            }             

            template<size_t N>
            dmesgQueueEntry_t& operator << (const Cstring<N>& value) {
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Lock ();
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::push_back ( { millis (), time (NULL), {} } );
                dmesgQueueEntry_t *p = threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::back ();
                p->message = (char *) &value;
                threadSafeCircularQueue<dmesgQueueEntry_t, maxSize>::Unlock ();
                return *p;
            }             


        private:

            // returns reset reason (this may help with debugging)
            const char *__resetReason__ (RESET_REASON reason) {
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

            // returns wakeup reason (this may help with debugging)
            const char *__wakeupReason__ () {
                esp_sleep_wakeup_cause_t wakeup_reason;
                wakeup_reason = esp_sleep_get_wakeup_cause ();
                switch (wakeup_reason) {
                    case ESP_SLEEP_WAKEUP_EXT0:     return "ESP_SLEEP_WAKEUP_EXT0 - wakeup caused by external signal using RTC_IO";
                    case ESP_SLEEP_WAKEUP_EXT1:     return "ESP_SLEEP_WAKEUP_EXT1 - wakeup caused by external signal using RTC_CNTL";
                    case ESP_SLEEP_WAKEUP_TIMER:    return "ESP_SLEEP_WAKEUP_TIMER - wakeup caused by timer";
                    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "ESP_SLEEP_WAKEUP_TOUCHPAD - wakeup caused by touchpad";
                    case ESP_SLEEP_WAKEUP_ULP:      return "ESP_SLEEP_WAKEUP_ULP - wakeup caused by ULP program";
                    default:                        return "WAKEUP REASON UNKNOWN - wakeup was not caused by deep sleep";
                }   
            }

    };


    // create working instance
    dmesgQueue_t<DMESG_CIRCULAR_QUEUE_LENGTH> dmesgQueue; 


    // backwards compatibility
    [[deprecated("Use dmesgQueue << operator instead.")]]
    void dmesg (const char *message) { dmesgQueue << message; }

    [[deprecated("Use dmesgQueue << operator instead.")]]
    void dmesg (const char *message1, const char *message2) { dmesgQueue << message1 << message2; }

    [[deprecated("Use dmesgQueue << operator instead.")]]
    void dmesg (const char *message, int i) { dmesgQueue << message << i; };

    [[deprecated("Use dmesgQueue << operator instead.")]]
    void dmesg (const char *message1, int i, char *message2) { dmesgQueue << message1 << i << message2; }
    
#endif
