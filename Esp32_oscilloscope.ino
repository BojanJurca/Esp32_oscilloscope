
// time-out? bbb




/*

    Esp32_web_ftp_telnet_server_template.ino

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
 
    File contains a working template for some operating system functionalities that can support your projects.
 
    Copy all files in the package to Esp32_web_ftp_telnet_server_template directory, compile them with Arduino and run on ESP32.

    May 22, 2024, Bojan Jurca

 */

// Compile this code with Arduino for one of ESP32 boards (Tools | Board) and one of FAT partition schemes (Tools | Partition scheme)!

#include <WiFi.h>

#include "servers/std/Cstring.hpp"
#include "servers/std/Map.hpp"
#include "servers/std/console.hpp"

#include "Esp32_servers_config.h"

#include "soc/rtc_wdt.h"

 
// ----- HTTP request handler example - if you don't want to handle HTTP requests just delete this function and pass NULL to httpSrv instead of its address -----
//       normally httpRequest is HTTP request, function returns a reply in HTML, json, ... formats or "" if request is unhandeled by httpRequestHandler
//       httpRequestHandler is supposed to be used with smaller replies,
//       if you want to reply with larger pages you may consider FTP-ing .html files onto the file system (into /var/www/html/ directory)


#ifndef FILE_SYSTEM
    #include "oscilloscope_amber.h" // use RAM version
#endif

String httpRequestHandler (char *httpRequest, httpConnection *hcn) { 

    #define httpRequestStartsWith(X) (strstr(httpRequest,X)==httpRequest)


    #ifdef __OSCILLOSCOPE__

        #ifdef __FILE_SYSTEM__

            // if HTTP request is GET /oscilloscope.html HTTP server will fetch the file but let us redirect GET / and GET /index.html to it as well
            if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ")) {
                hcn->setHttpReplyHeaderField ("Location", "/oscilloscope.html");
                hcn->setHttpReplyStatus ((char *) "307 temporary redirect"); // change reply status to 307 and set Location so client browser will know where to go next
                return "Redirecting ..."; // whatever
            }

        #else

            if (httpRequestStartsWith ("GET / ") || httpRequestStartsWith ("GET /index.html ") || httpRequestStartsWith ("GET /oscilloscope.html ")) {
                return F (oscilloscope_amber);  
            }

        #endif

    #endif
                                                                    

    return ""; // httpRequestHandler did not handle the request - tell httpServer to handle it internally by returning "" reply
}


void wsRequestHandler (char *wsRequest, WebSocket *webSocket) {
  
    #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)
    
    #ifdef __OSCILLOSCOPE__
          if (wsRequestStartsWith ("GET /runOscilloscope"))      runOscilloscope (webSocket);      // used by oscilloscope.html
    #endif

}

// ----- telnet command handler example - if you don't want to handle telnet commands yourself just delete this function and pass NULL to telnetSrv instead of its address -----

String pulseViewCommandHandlerCallback (int argc, char *argv [], telnetConnection *tcn) {

    #define argv0is(X) (argc > 0 && !strcmp (argv[0], X))  
    #define argv1is(X) (argc > 1 && !strcmp (argv[1], X))
    #define argv2is(X) (argc > 2 && !strcmp (argv[2], X)) 


    // process special commands to manage session private memory
    oscSharedMemory *pOscSharedMemory = (oscSharedMemory *) tcn->privateMemory;
    if (argv0is ("SESSION START")) {
        tcn->privateMemory = malloc (sizeof (oscSharedMemory));
        pOscSharedMemory = (oscSharedMemory *) tcn->privateMemory;
        if (pOscSharedMemory) {
            // fill in the default values
            pOscSharedMemory = (oscSharedMemory *) tcn->privateMemory;
            memset (pOscSharedMemory, 0, sizeof (oscSharedMemory));
            strcpy (pOscSharedMemory->readType, "analog");    
            pOscSharedMemory->analog = true;
            pOscSharedMemory->gpio1 = 255; // invalid, has to be set later with CHAN command
            pOscSharedMemory->gpio2 = 255; // invalid, has to be set later with CHAN command, if used
            pOscSharedMemory->samplingTime = 100; 
            strcpy (pOscSharedMemory->samplingTimeUnit, "us");
        }
    } else if (argv0is ("SESSION END")) {
        if (tcn->privateMemory) {
            if (pOscSharedMemory->oscReaderState == STARTED) {
                // send oscReader STOP signal
                pOscSharedMemory->oscReaderState = STOP; 

                // wait until oscReader STOPPED or error
                while (pOscSharedMemory->oscReaderState != STOPPED) delay (1); 
            }
            free (tcn->privateMemory);
        }
    }


    // process SCPI commands
    if (argv0is ("*IDN?")) {
        if (argc == 1) {
            #ifdef DEFAULT_AP_SSID
                return String ("*IDN " HOSTNAME " " VERSION_OF_SERVERS ", SN: ") + WiFi.softAPmacAddress ().c_str ();
            #else
                return String ("*IDN " HOSTNAME " " VERSION_OF_SERVERS ", SN: ") + WiFi.macAddress ().c_str ();
            #endif
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("SARA")) {
        if (argc == 2) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            double sara;
            if (sscanf (argv [1], "%lf", &sara) == 1 && sara >= 1 && sara <= 1000) {
                ///  bbb int samplingTime = 1.0 / sara * 1000000; // in us
                pOscSharedMemory->samplingTime = 1000000.0 / sara; // in us
                return "OK"; // or  "\r" if you wish, but not "" - this would mean that che command has not been handeled so the Telnet server will try to handle it itself
            } else {
                return "Parameter out of range";
            }
        } else {
            return "Wrong number of parameters";  
        }
    }

    else if (argv0is ("SARA?")) {
        if (argc == 1) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            char buf [25];
            sprintf (buf, "%-24lf", 1000000.0 / pOscSharedMemory->samplingTime);
            return buf;
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("CHAN")) {
        if (!pOscSharedMemory)    
            return "Out of memory";

        int gpio1, gpio2;
        pOscSharedMemory->gpio1 = 255;
        pOscSharedMemory->gpio2 = 255;
        switch (argc) {
            case 3:   gpio2 = atoi (argv [2]);
                      if (gpio2 < 0 || gpio2 > 39) {
                          return String ("Wrong channel ") + argv [2];
                      }
                      pOscSharedMemory->gpio2 = gpio2;
            case 2:   gpio1 = atoi (argv [1]);
                      if (gpio1 < 0 || gpio1 > 39) {
                          return String ("Wrong channel ") + argv [1];
                      }
                      pOscSharedMemory->gpio1 = gpio1;
                      return "OK"; // or  "\r" if you wish, but not "" - this would mean that che command has not been handeled so the Telnet server will try to handle it itself
            default:  return "Wrong number of parameters";
        }
    }

    else if (argv0is ("NS")) {
        if (argc == 2) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            int noOfSamples = atoi (argv [1]); 
            if (noOfSamples > 0 ) {
                pOscSharedMemory->noOfSamples = noOfSamples;
                return "OK"; // or  "\r" if you wish, but not "" - this would mean that che command has not been handeled so the Telnet server will try to handle it itself
            } else {
                return "Parameter out of range";
            }

        } else {
            return "Wrong number of parameters";  
        }
    }

    else if (argv0is ("TRSE")) { 
        if (argc == 3) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            int value = atoi (argv [2]);
            if (value <= 0 || value >= 4055) {
                return "Parameter out of range";
            }
            if (!strcmp (argv [1], "RISING")) {
                pOscSharedMemory->positiveTrigger = true;
                pOscSharedMemory->positiveTriggerTreshold = value;
            } else if (!strcmp (argv [1], "FALLING")) {
                pOscSharedMemory->negativeTrigger = true;
                pOscSharedMemory->negativeTriggerTreshold = value;
            } else {
                return "Parameter invalid";
            }

        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("ARM")) {
        if (argc == 1) {

            if (!pOscSharedMemory)
                return "Out of memory";

            // did we collect all the necessary information so far?
            if (pOscSharedMemory->gpio1 < 0 || pOscSharedMemory->gpio1 > 39)
                return "Channel 1 not set"; // at least cahnnel 1 should be set

            if (pOscSharedMemory->noOfSamples == 0)
                return "Number of samples not set";

            if (pOscSharedMemory->samplingTime == 0)
                return "Sample rate not set";

            pOscSharedMemory->screenWidthTime = pOscSharedMemory->samplingTime * pOscSharedMemory->noOfSamples;

            /*
            cout << "   ARM samplingTime:    " << pOscSharedMemory->samplingTime << " us" << endl;
            cout << "   ARM noOfSamples:     " << pOscSharedMemory->noOfSamples << endl;
            cout << "   ARM screenWidthTime: " << pOscSharedMemory->screenWidthTime << " us" << endl;
            */

            if (pOscSharedMemory->screenWidthTime == 0)
                return "BUG";

            // ----- run oscilloscope reader as a separate task -----

            // choose the corect oscReader
            void (*oscReader) (void *sharedMemory);
            oscReader = oscReader_analog; // us sampling interval, 1-2 signals, analog reader
            #ifdef USE_I2S_INTERFACE
              if (pOscSharedMemory->gpio2 > 39) // 1 signal only
                  oscReader = oscReader_analog_1_signal_i2s; // us sampling interval, 1 signal, (fast, DMA) I2S analog reader
            #endif
            if (!strcmp (pOscSharedMemory->samplingTimeUnit, "ms"))
                oscReader = oscReader_millis; // ms sampling intervl, 1-2 signals, digital or analog reader with 'sample at a time' or 'screen at a time' options

            pOscSharedMemory->oscReaderState = INITIAL;

            #ifdef OSCILLOSCOPE_READER_CORE
                BaseType_t taskCreated = xTaskCreatePinnedToCore (oscReader, "oscReader", 4 * 1024, (void *) pOscSharedMemory, OSCILLOSCOPE_READER_PRIORITY, NULL, OSCILLOSCOPE_READER_CORE);
            #else
                BaseType_t taskCreated = xTaskCreate (oscReader, "oscReader", 4 * 1024, (void *) pOscSharedMemory, OSCILLOSCOPE_READER_PRIORITY, NULL);
            #endif
            if (pdPASS != taskCreated) {
                  #ifdef __DMESG__
                      dmesgQueue << "[oscilloscope] could not start oscReader";
                  #endif
                  cout << "[oscilloscope] could not start oscReader";
            } else {

                // send oscReader START signal and wait until STARTED
                pOscSharedMemory->oscReaderState = START; 
                while (pOscSharedMemory->oscReaderState == START) delay (1); 

            }

            return "OK"; // or  "\r" if you wish, but not "" - this would mean that che command has not been handeled so the Telnet server will try to handle it itself

        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("STOP")) {
        if (!pOscSharedMemory)
            return "Out of memory";

        if (argc == 1) {

            // ----- stop oscilloscope task (if it has been started) -----

            if (pOscSharedMemory->oscReaderState == STARTED) {
                // send oscReader STOP signal
                pOscSharedMemory->oscReaderState = STOP; 

                // wait until oscReader STOPPED or error
                while (pOscSharedMemory->oscReaderState != STOPPED) delay (1); 
            }

            return "OK"; // or  "\r" if you wish, but not "" - this would mean that che command has not been handeled so the Telnet server will try to handle it itself
        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("SAST?")) {
        if (argc == 1) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            // ----- check oscilloscope task state -----

            switch (pOscSharedMemory->oscReaderState) {
                case INITIAL:                   // if not started yet
                              return "SAST Ready";
                case STOPPED:                   // it already stopped
                              return "SAST Stopped";
                case STARTED:                   // oscillocsope task is running, but check if send buffer is ready
                              if (pOscSharedMemory->sendBuffer.samplesAreReady && pOscSharedMemory->sendBuffer.sampleCount)
                                  return "SAST Ready";
                              else
                                  return "SAST Trig'd";
            }
            return "BUG"; // shouldn't happen

        } else {
            return "Wrong number of parameters";
        }
    }

    else if (argv0is ("DATA")) {
        if (argc == 1) {

            if (!pOscSharedMemory)    
                return "Out of memory";

            // ----- return the samples if they are ready -----

            if (pOscSharedMemory->sendBuffer.samplesAreReady) {

                String buf = " ";

                // check the format of output data - from the first, dummy sample:
                switch (pOscSharedMemory->sendBuffer.samples1Signal [0].signal1) {
                    case -2:  // 1 signal with deltaTime

                              // TO DO: improove signal quality with interpolation - regarding to pOscSharedMemory->sendBuffer.samples1Signal [i].deltaTime
                              cout << "-2: 1 analog signal with deltaTime, sampleCount: " << pOscSharedMemory->sendBuffer.sampleCount << endl;

                              for (int i = 1; i < pOscSharedMemory->sendBuffer.sampleCount; i++)
                                  buf += String (pOscSharedMemory->sendBuffer.samples1Signal [i].signal1) + " ";

                              pOscSharedMemory->sendBuffer.samplesAreReady = false; // enable next reading
                              return buf;

                    case -3:  // 2 signals with deltaTime

                              // TO DO: improove signal qualitywith interpolation - regarding pOscSharedMemory->sendBuffer.samples2Signal [i].deltaTime
                              cout << "-3: 2 analog signals with deltaTime, sampleCount: " << pOscSharedMemory->sendBuffer.sampleCount << endl;

                              for (int i = 1; i < pOscSharedMemory->sendBuffer.sampleCount; i++)
                                  buf += String (pOscSharedMemory->sendBuffer.samples2Signals [i].signal1) + " " + String (pOscSharedMemory->sendBuffer.samples2Signals [i].signal2) + " ";
                                  
                              pOscSharedMemory->sendBuffer.samplesAreReady = false; // enable next reading
                              return buf;

                    default:  // 1 signal I2S sampling with constant time, sampling time = -pOscSharedMemory->sendBuffer.samplesI2sSignal [0].signal1

                              cout << pOscSharedMemory->sendBuffer.samples1Signal [0].signal1 << ": 1 analog I2S analog signal, fixed time, sampleCount: " << pOscSharedMemory->sendBuffer.sampleCount << endl;

                              for (int i = 1; i < pOscSharedMemory->sendBuffer.sampleCount; i++)
                                  buf += String (pOscSharedMemory->sendBuffer.samplesI2sSignal [i].signal1) + " ";
                                  
                              pOscSharedMemory->sendBuffer.samplesAreReady = false; // enable next reading
                              return buf;
                }
                pOscSharedMemory->sendBuffer.samplesAreReady = false; // enable next reading
                return "BUG"; // shouldn't happen

            }
            return "No data";

        } else {
            return "Wrong number of parameters";
        }
    }


    return "Command is not supported";
}


void setup () {

    // disable watchdog if you can afford it - watchdog gets occasionally triggered when loaded heavily
    #if CONFIG_FREERTOS_UNICORE // CONFIG_FREERTOS_UNICORE == 1 => 1 core ESP32
        disableCore0WDT ();     
    #else // CONFIG_FREERTOS_UNICORE == 0 => 2 core ESP32
        disableCore0WDT ();     
        disableCore1WDT (); 
    #endif


    cinit ();
  
    #ifdef FILE_SYSTEM
        fileSystem.mountLittleFs (true);                                // this is the first thing to do - all configuration files are on file system
        // fileSystem.formatLittleFs ();        
    #endif

    // deleteFile ("/etc/ntp.conf");                                    // contains ntp server names for time sync - deleting this file would cause creating default one
    // deleteFile ("/etc/crontab");                                     // contains cheduled tasks                 - deleting this file would cause creating empty one
    // startCronDaemon (cronHandler);                                      // creates /etc/ntp.conf with default NTP server names and syncronize ESP32 time with them once a day
                                                                        // creates empty /etc/crontab, reads it at startup and executes cronHandler when the time is right
                                                                        // 3 KB stack size is minimal requirement for NTP time synchronization, add more if your cronHandler requires more

    // deleteFile ("/etc/passwd");                                      // contains users' accounts information    - deleting this file would cause creating default one
    // deleteFile ("/etc/shadow");                                      // contains users' passwords               - deleting this file would cause creating default one
    // userManagement.initialize ();                                       // creates user management files with root, webadmin, webserver and telnetserver users, if they don't exist

    // deleteFile ("/network/interfaces");                              // contation STA(tion) configuration       - deleting this file would cause creating default one
    // deleteFile ("/etc/wpa_supplicant/wpa_supplicant.conf");          // contation STA(tion) credentials         - deleting this file would cause creating default one
    // deleteFile ("/etc/dhcpcd.conf");                                 // contains A(ccess) P(oint) configuration - deleting this file would cause creating default one
    // deleteFile ("/etc/hostapd/hostapd.conf");                        // contains A(ccess) P(oint) credentials   - deleting this file would cause creating default one
    startWiFi ();                                                       // starts WiFi according to configuration files, creates configuration files if they don't exist

    // start web server 
    httpServer *httpSrv = new httpServer (httpRequestHandler,           // a callback function that will handle HTTP requests that are not handled by webServer itself
                                          wsRequestHandler);            // a callback function that will handle WS requests, NULL to ignore WS requests
    if (!httpSrv || httpSrv->state () != httpServer::RUNNING) {
        #ifdef __DMESG__
            dmesgQueue << "[httpServer] did not start";
        #endif
        cout << "[httpServer] did not start";
    }

    // start FTP server
    ftpServer *ftpSrv = new ftpServer ();
    if (!ftpSrv || ftpSrv->state () != ftpServer::RUNNING) {
        #ifdef __DMESG__
            dmesgQueue << "[ftpServer] did not start";
        #endif
        cout << "[ftpServer] did not start";
    }

    // start telnet server on port 5025 for SCPI communication
    telnetServer *telnetSrv = new telnetServer (pulseViewCommandHandlerCallback, "0.0.0.0", 5025);
    if (!telnetSrv || telnetSrv->state () != telnetServer::RUNNING) {
        #ifdef __DMESG__
            dmesgQueue << "[telnetServer] did not start";
        #endif
        cout << "[telnetServer] did not start";
    }

}

void loop () {

}
