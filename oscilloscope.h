/*
 
    oscilloscope.h
 
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
                
    Issues:
            - when WiFi is in WIFI_AP or WIFI_STA_AP mode is oscillospe causes WDT problem when working at higher frequenceses

    August 12, 2023, Bojan Jurca
             
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <soc/gpio_sig_map.h> // to digitalRead PWM and other GPIOs ...
    #include <soc/io_mux_reg.h>   // thanks to gin66: https://github.com/BojanJurca/Esp32_oscilloscope/issues/19    
    #include "driver/adc.h"       // to use adc1_get_raw instead of analogRead
    // fixed size strings    
    #include "fsString.h"


    // ----- TUNNING PARAMETERS -----

    // #define OSCILLOSCOPE_READER_CORE 1 // 1 or 0                   // #define OSCILLOSCOPE_READER_CORE if you want oscilloscope reader to run on specific core    
    #ifndef OSCILLOSCOPE_READER_PRIORITY
        #define OSCILLOSCOPE_READER_PRIORITY 1                        // normal priority if not define differently
    #endif


    // ----- CODE -----

    #include "httpServer.hpp"                 // oscilloscope uses websockets defined in webServer.hpp  


    // oscilloscope samples
    struct oscSample {                        // one sample
       int16_t signal1;                       // signal value of 1st GPIO read by analogRead or digialRead   
       int16_t signal2;                       // signal value of 2nd GPIO if requested   
       int16_t deltaTime;                     // sample time - offset from previous sample in ms or us  
    }; // = 6 bytes per sample
    
    struct oscSamples {                       // buffer with samples
       oscSample samples [128];               // sample buffer content will never exceed 41 samples, make it 128 - that will simplify the code and thus making it faster    
       int sampleCount;                       // number of samples in the buffer
       bool samplesAreReady;                  // is the buffer ready for sending
    }; // = max 128 samples or 768 bytes

    enum readerState { INITIAL = 0, START = 1, STARTED  = 2, STOP = 3, STOPPED = 4 };
    /* transitions:
          START   - set by osc main thread
          STARTED - set by oscReader
          STOP    - set by osc main thread
          STOPPED - set by oscReader
    */

    struct oscSharedMemory {         // data structure to be shared among oscilloscope tasks
      // basic data
      WebSocket *webSocket;                   // open webSocket for communication with javascript client
      bool clientIsBigEndian;                 // true if javascript client is big endian machine
      // sampling sharedMemory
      char readType [8];                      // analog or digital  
      bool analog;                            // true if readType is analog, false if digital (digitalRead)
      int gpio1;                              // gpio where ESP32 is taking samples from (digitalRead)
      adc1_channel_t adcchannel1;             // channel mapped from gpio ESP32 is taking samples from (adc1_get_raw instead of analogRead)
      adc1_channel_t adcchannel2;             // channel mapped from gpio ESP32 is taking samples from (adc1_get_raw instead of analogRead)
      int gpio2;                              // 2nd gpio if requested
      int samplingTime;                       // time between samples in ms or us
      char samplingTimeUnit [3];              // ms or us
      int screenWidthTime;                    // oscilloscope screen width in ms or us
      char screenWidthTimeUnit [3];           // ms or us 
      bool positiveTrigger;                   // true if posotive slope trigger is set
      int positiveTriggerTreshold;            // positive slope trigger treshold value
      bool negativeTrigger;                   // true if negative slope trigger is set  
      int negativeTriggerTreshold;            // negative slope trigger treshold value
      // buffers holding samples 
      oscSamples readBuffer;                  // we'll read samples into this buffer
      oscSamples sendBuffer;                  // we'll copy red buffer into this buffer before sending samples to the client
      // reader state
      readerState oscReaderState;             // helps to execute a proper stopping sequence
    };

    // oscilloscope reader read samples to read-buffer of shared memory - it will be copied to send buffer when it is ready to be sent

    void oscReader (void *sharedMemory) {
        bool doAnalogRead =                 !strcmp (((oscSharedMemory *) sharedMemory)->readType, "analog");
        bool unitIsMicroSeconds =           !strcmp (((oscSharedMemory *) sharedMemory)->samplingTimeUnit, "us");
        int samplingTime =                  ((oscSharedMemory *) sharedMemory)->samplingTime;
        bool positiveTrigger =              ((oscSharedMemory *) sharedMemory)->positiveTrigger;
        bool negativeTrigger =              ((oscSharedMemory *) sharedMemory)->negativeTrigger;
        unsigned char gpio1 =               (unsigned char) ((oscSharedMemory *) sharedMemory)->gpio1; // easier to check validity with unsigned char then with integer 
        unsigned char gpio2 =               (unsigned char) ((oscSharedMemory *) sharedMemory)->gpio2; // easier to check validity with unsigned char then with integer
        adc1_channel_t adcchannel1 =        ((oscSharedMemory *) sharedMemory)->adcchannel1;
        adc1_channel_t adcchannel2 =        ((oscSharedMemory *) sharedMemory)->adcchannel2;
        int positiveTriggerTreshold =       ((oscSharedMemory *) sharedMemory)->positiveTriggerTreshold;
        int negativeTriggerTreshold =       ((oscSharedMemory *) sharedMemory)->negativeTriggerTreshold;
        int screenWidthTime =               ((oscSharedMemory *) sharedMemory)->screenWidthTime; 
        oscSamples *readBuffer =            &((oscSharedMemory *) sharedMemory)->readBuffer;
        oscSamples *sendBuffer =            &((oscSharedMemory *) sharedMemory)->sendBuffer;

        // thanks to gin66 (https://github.com/BojanJurca/Esp32_oscilloscope/issues/19 we can also read GPIOs that were configured for OUTPUT or PWM
        if (gpio1 <= 39) PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG [gpio1]);
        if (gpio2 <= 39) PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG [gpio2]);

        // wait for the START signal
        while (((oscSharedMemory *) sharedMemory)->oscReaderState != START) delay (1);
        ((oscSharedMemory *) sharedMemory)->oscReaderState = STARTED; 


        // do the sampling

        // triggered or untriggered mode of operation
        bool triggeredMode = positiveTrigger || negativeTrigger;

        if (unitIsMicroSeconds) {
            // ----- samplingTime and screenWidthTime are in us -----

            // Calculate screen refresh period. It sholud be arround 50 ms (sustainable screen refresh rate is arround 20 Hz) but it is better if it is a multiple value of screenWidthTime.
            unsigned long screenRefreshMilliseconds; // screen refresh period
            int noOfSamplesPerScreen = screenWidthTime / samplingTime; if (noOfSamplesPerScreen * samplingTime < screenWidthTime) noOfSamplesPerScreen ++;
            int correctedScreenWidthTime = noOfSamplesPerScreen * samplingTime;                         
            // DEBUG: Serial.printf ("[oscilloscope][oscReader] screenWidthTime = %i   noOfSamplesPerScreen = %lu   correctedScreenWidthTime = %i\n", screenWidthTime, noOfSamplesPerScreen, correctedScreenWidthTime);
            screenRefreshMilliseconds = correctedScreenWidthTime >= 50000 ? correctedScreenWidthTime / 1000 : ((50500 / correctedScreenWidthTime) * correctedScreenWidthTime) / 1000;
            // DEBUG: Serial.printf ("[oscilloscope][oscReader] screenRefreshMilliseconds = %lu ms (should be close to 50 ms) => screen refresh frequency = %f Hz (should be close to 20 Hz)\n", screenRefreshMilliseconds, 1000.0 / screenRefreshMilliseconds);

            TickType_t lastScreenRefreshTicks = xTaskGetTickCount ();               // for timing screen refresh intervals

            while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { // sampling from the left of the screen - while not getting STOP signal


                int screenTime = 0;                                                 // in us - how far we have already got from the left of the screen (we'll compare this value with screenWidthTime)
                unsigned long deltaTime = 0;                                        // in us - delta from previous sample
                unsigned long lastSampleMicroseconds = micros ();                   // for sample timing                
                unsigned long newSampleMicroseconds = lastSampleMicroseconds;

                // insert first dummy sample to read-buffer this tells javascript client to start drawing from the left of the screen
                readBuffer->samples [0] = {-1, -1, -1}; // no real data sample can look like this
                readBuffer->sampleCount = 1;
                // DEBUG: Serial.printf ("[oscilloscope][oscReader] first (dummy) sample inserted\n");

                if (triggeredMode) { // if no trigger is set then skip this (waiting) part and start sampling immediatelly

                    // take the first sample
                    oscSample lastSample; 
                    if (doAnalogRead) lastSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) 0}; else lastSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) 0}; // gpio1 should always be valid PIN

                    // wait for trigger condition
                    while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { 
                        // DEBUG: Serial.printf ("[oscilloscope][oscReader] waiting for a trigger event, state = %i\n", ((oscSharedMemory *) sharedMemory)->oscReaderState);

                        // wait befor continuing to next sample and calculate delta offset for it
                        unsigned long passedMicroseconds = micros () - lastSampleMicroseconds;
                        if (passedMicroseconds < samplingTime) delayMicroseconds (samplingTime - passedMicroseconds);
                        newSampleMicroseconds = micros ();
                        deltaTime = newSampleMicroseconds - lastSampleMicroseconds;
                        lastSampleMicroseconds = newSampleMicroseconds;
                        
                        // take the second sample
                        oscSample newSample; 
                        if (doAnalogRead) newSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) deltaTime}; // gpio1 should always be valid 

                        // compare both samples to check if the trigger condition has occured, only gpio1 is used to trigger the sampling 
                        if ((positiveTrigger && lastSample.signal1 < positiveTriggerTreshold && newSample.signal1 >= positiveTriggerTreshold) || (negativeTrigger && lastSample.signal1 > negativeTriggerTreshold && newSample.signal1 <= negativeTriggerTreshold)) { 
                            // trigger condition has occured, insert both samples into read buffer
                            readBuffer->samples [1] = lastSample; // timeOffset (from left of the screen) = 0
                            readBuffer->samples [2] = newSample;  // this is the first sample after triggered
                            screenTime = deltaTime;     // start measuring screen time from new sample on
                            readBuffer->sampleCount = 3;
                    
                            // correct screenTime
                            screenTime = deltaTime;

                            // wait befor continuing to next sample and calculate delta offset for it
                            unsigned long passedMicroseconds = micros () - lastSampleMicroseconds;
                            if (passedMicroseconds < samplingTime) delayMicroseconds (samplingTime - passedMicroseconds);
                            newSampleMicroseconds = micros ();
                            deltaTime = newSampleMicroseconds - lastSampleMicroseconds;
                            lastSampleMicroseconds = newSampleMicroseconds;
                                
                            break; // trigger event occured, stop waiting and proceed to sampling
                        } else {
                            // just forget the first sample and continue waiting for trigger condition - copy just signal values and let the timing start from 0
                            lastSample.signal1 = newSample.signal1;
                            lastSample.signal2 = newSample.signal2;
                        }
                    } // while not triggered
                } // if in trigger mode

                // take (the rest of the) samples that fit on one screen
                while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { // while screenTime < screenWidthTime

                    // if we already passed screenWidthMilliseconds then copy read buffer to send buffer so it can be sent to the javascript client
                    if (screenTime >= screenWidthTime) { 
                        // DEBUG: Serial.printf ("[oscilloscope] end of packet sampling - full screen: %i >=? %i   samplingTime = %i    samples = %i\n", screenTime, screenWidthTime, samplingTime, readBuffer->sampleCount);
                        // copy read buffer to send buffer so that oscilloscope sender can send it to javascript client 
                        if (!sendBuffer->samplesAreReady) 
                            *sendBuffer = *readBuffer; // this also copies 'ready' flag from read buffer which is 'true' - tell oscSender to send the packet, this would refresh client screen
                        // else send buffer with previous frame is still waiting to be sent, do nothing now, skip this frame

                        // break out of the loop and than start taking new samples
                        break; // get out of while loop to start sampling from the left of the screen again
                    }

                    // take the next sample
                    oscSample newSample; 
                    if (doAnalogRead) newSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) deltaTime}; // gpio1 should always be valid 
                    if (readBuffer->sampleCount < 128 - 1) // should always be true, but check anyway
                        readBuffer->samples [readBuffer->sampleCount ++] = newSample;
                    // DEBUG: else Serial.printf ("[oscilloscope][oscReader] full buffer: %i    sampling time: %i    screen width: %i >=? %i\n", readBuffer->sampleCount, samplingTime, screenTime, screenWidthTime);
                    screenTime += deltaTime;

                    // wait befor continuing to next sample and calculate delta offset for it
                    unsigned long passedMicroseconds = micros () - lastSampleMicroseconds;
                    if (passedMicroseconds < samplingTime) delayMicroseconds (samplingTime - passedMicroseconds);
                    newSampleMicroseconds = micros ();
                    deltaTime = newSampleMicroseconds - lastSampleMicroseconds;
                    lastSampleMicroseconds = newSampleMicroseconds;

                } // while screenTime < screenWidthTime

                // wait before next screen refresh
                vTaskDelayUntil (&lastScreenRefreshTicks, pdMS_TO_TICKS (screenRefreshMilliseconds));

            } // while sampling

        } else {
            // ----- samplingTime and screenWidthTime are in ms -----

            // determine mode of operation sample at a time or screen at a time - this only makes sense when screenWidthTime is measured in ms
            bool oneSampleAtATime = !unitIsMicroSeconds && screenWidthTime > 1000;
            // DEBUG: Serial.printf ("[oscilloscope][oscReader] oneSampleAtATime mode = %i   unit = %s   screenWidthTime = %i\n", oneSampleAtATime, unitIsMicroSeconds ? "us" : "ms", screenWidthTime);

            // Calculate screen refresh period. It sholud be arround 50 ms (sustainable screen refresh rate is arround 20 Hz) but it is better if it is a multiple value of screenWidthTime.
            unsigned long screenRefreshMilliseconds; // screen refresh period
            int noOfSamplesPerScreen = screenWidthTime / samplingTime; if (noOfSamplesPerScreen * samplingTime < screenWidthTime) noOfSamplesPerScreen ++;
            int correctedScreenWidthTime = noOfSamplesPerScreen * samplingTime;                         
            // DEBUG: Serial.printf ("[oscilloscope][oscReader] screenWidthTime = %i   noOfSamplesPerScreen = %lu   correctedScreenWidthTime = %i\n", screenWidthTime, noOfSamplesPerScreen, correctedScreenWidthTime);
            screenRefreshMilliseconds = correctedScreenWidthTime >= 50 ? correctedScreenWidthTime : ((505 / (correctedScreenWidthTime * 10)) * correctedScreenWidthTime);
            // DEBUG: Serial.printf ("[oscilloscope][oscReader] screenRefreshMilliseconds = %lu ms (should be close to 50 ms) => screen refresh frequency = %f Hz (should be close to 20 Hz)\n", screenRefreshMilliseconds, 1000.0 / screenRefreshMilliseconds);

            TickType_t lastScreenRefreshTicks = xTaskGetTickCount ();               // for timing screen refresh intervals            

            while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { // sampling from the left of the screen - while not getting STOP signal

                int screenTime = 0;                                                 // in ms - how far we have already got from the left of the screen (we'll compare this value with screenWidthTime)
                unsigned long deltaTime = 0;                                        // in ms - delta from previous sample
                TickType_t lastSampleTicks = xTaskGetTickCount ();                  // for sample timing                
                TickType_t newSampleTicks = lastSampleTicks;

                // insert first dummy sample to read-buffer this tells javascript client to start drawing from the left of the screen
                readBuffer->samples [0] = {-1, -1, -1}; // no real data sample can look like this
                readBuffer->sampleCount = 1;
                // DEBUG: Serial.printf ("[oscilloscope][oscReader] first (dummy) sample inserted\n");

                if (triggeredMode) { // if no trigger is set then skip this (waiting) part and start sampling immediatelly

                    // take the first sample
                    oscSample lastSample; 
                    if (doAnalogRead) lastSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) 0}; else lastSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) 0}; // gpio1 should always be valid PIN

                    // wait for trigger condition
                    while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { 
                        // DEBUG: Serial.printf ("[oscilloscope][oscReader] waiting for a trigger event, state = %i\n", ((oscSharedMemory *) sharedMemory)->oscReaderState);

                        // wait befor continuing to next sample and calculate delta offset for it
                        vTaskDelayUntil (&newSampleTicks, pdMS_TO_TICKS (samplingTime));
                        deltaTime = pdTICKS_TO_MS (newSampleTicks - lastSampleTicks); // in ms - this value will be used for the next sample offset
                        lastSampleTicks = newSampleTicks;

                        // take the second sample
                        oscSample newSample; 
                        if (doAnalogRead) newSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) deltaTime}; // gpio1 should always be valid 

                        // compare both samples to check if the trigger condition has occured, only gpio1 is used to trigger the sampling 
                        if ((positiveTrigger && lastSample.signal1 < positiveTriggerTreshold && newSample.signal1 >= positiveTriggerTreshold) || (negativeTrigger && lastSample.signal1 > negativeTriggerTreshold && newSample.signal1 <= negativeTriggerTreshold)) { 
                            // trigger condition has occured, insert both samples into read buffer
                            readBuffer->samples [1] = lastSample; // timeOffset (from left of the screen) = 0
                            readBuffer->samples [2] = newSample;  // this is the first sample after triggered
                            screenTime = deltaTime;     // start measuring screen time from new sample on
                            readBuffer->sampleCount = 3;
                    
                            // correct screenTime
                            screenTime = deltaTime;

                            // wait befor continuing to next sample and calculate delta offset for it
                            vTaskDelayUntil (&newSampleTicks, pdMS_TO_TICKS (samplingTime));
                            deltaTime = pdTICKS_TO_MS (newSampleTicks - lastSampleTicks); // in ms - this value will be used for the next sample offset
                            lastSampleTicks = newSampleTicks;
                                
                            break; // trigger event occured, stop waiting and proceed to sampling
                        } else {
                            // just forget the first sample and continue waiting for trigger condition - copy just signal values and let the timing start from 0
                            lastSample.signal1 = newSample.signal1;
                            lastSample.signal2 = newSample.signal2;
                        }
                    } // while not triggered
                } // if in trigger mode
 
                // take (the rest of the) samples that fit on one screen
                while (((oscSharedMemory *) sharedMemory)->oscReaderState == STARTED) { // while screenTime < screenWidthTime

                    // if we already passed screenWidthMilliseconds then copy read buffer to send buffer so it can be sent to the javascript client
                    if (screenTime >= screenWidthTime) { 
                        // DEBUG: Serial.printf ("[oscilloscope] end of packet sampling - full screen: %i >=? %i   samplingTime = %i    samples = %i\n", screenTime, screenWidthTime, samplingTime, readBuffer->sampleCount);
                        // copy read buffer to send buffer so that oscilloscope sender can send it to javascript client 
                        if (!sendBuffer->samplesAreReady) 
                            *sendBuffer = *readBuffer; // this also copies 'ready' flag from read buffer which is 'true' - tell oscSender to send the packet, this would refresh client screen
                        // else send buffer with previous frame is still waiting to be sent, do nothing now, skip this frame

                        // break out of the loop and than start taking new samples
                        break; // get out of while loop to start sampling from the left of the screen again
                    }

                    // one sample at a time mode requires sending (copying) the readBuffer to the sendBuffer so it can be sent to the javascript client even before it gets full (of samples that fit to one screen)
                    if (oneSampleAtATime && readBuffer->sampleCount) {
                        // DEBUG: Serial.printf ("[oscilloscope][oscReader] oneSampleAtATime mode, sampleCount = %i (ready to be sent)\n", readBuffer->sampleCount);
                        // copy read buffer to send buffer so that oscilloscope sender can send it to javascript client 
                        if (!sendBuffer->samplesAreReady) {
                            *sendBuffer = *readBuffer; // this also copies 'ready' flag from read buffer which is 'true' - tell oscSender to send the packet, this would refresh client screen
                            *sendBuffer = *readBuffer;
                            readBuffer->sampleCount = 0; // empty read buffer so we don't send the same data again later
                        }
                        // else send buffer with previous frame is still waiting to be sent, but the buffer is not full yet, so just continue sampling into the same frame
                    }
        
                    // take the next sample
                    oscSample newSample; 
                    if (doAnalogRead) newSample = {(int16_t) adc1_get_raw (adcchannel1), gpio2 < 100 ? (int16_t) adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) deltaTime}; // gpio1 should always be valid 
                    if (readBuffer->sampleCount < 128 - 1) // should always be true, but check anyway
                        readBuffer->samples [readBuffer->sampleCount ++] = newSample;
                    // DEBUG: else Serial.printf ("[oscilloscope][oscReader] full buffer: %i    sampling time: %i    screen width: %i >=? %i\n", readBuffer->sampleCount, samplingTime, screenTime, screenWidthTime);
                    screenTime += deltaTime;

                    // wait befor continuing to next sample and calculate delta offset for it
                    vTaskDelayUntil (&newSampleTicks, pdMS_TO_TICKS (samplingTime));
                    deltaTime = pdTICKS_TO_MS (newSampleTicks - lastSampleTicks); // in ms - this value will be used for the next sample offset
                    // DEBUG: Serial.printf ("[oscilloscope][oscReader] seamples int readBuffer: %i   deltaTime = %lu\n", readBuffer->sampleCount, deltaTime);
                    lastSampleTicks = newSampleTicks;

                } // while screenTime < screenWidthTime

                // wait before next screen refresh
                vTaskDelayUntil (&lastScreenRefreshTicks, pdMS_TO_TICKS (screenRefreshMilliseconds));

            } // while sampling

        }

        // DEBUG: Serial.printf ("[oscilloscope][oscReader] stopping, state = %i\n", ((oscSharedMemory *) sharedMemory)->oscReaderState);

        // wait for the STOP signal
        while (((oscSharedMemory *) sharedMemory)->oscReaderState != STOP) delay (1);
        ((oscSharedMemory *) sharedMemory)->oscReaderState = STOPPED; 

        vTaskDelete (NULL);
    }

    // oscilloscope sender is always sending both streams (both GPIO samples) regardless if only one is in use - let javascript client pick out only those that it rquested
    
    void oscSender (void *sharedMemory) {
      oscSamples *sendBuffer =                 &((oscSharedMemory *) sharedMemory)->sendBuffer;
      sendBuffer->samplesAreReady = false;     
      bool clientIsBigEndian =                 ((oscSharedMemory *) sharedMemory)->clientIsBigEndian;
      WebSocket *webSocket =                   ((oscSharedMemory *) sharedMemory)->webSocket; 
    
      while (true) { 
        delay (1);
        // send samples to javascript client if they are ready
        if (sendBuffer->samplesAreReady) {
          // copy buffer with samples within critical section
          oscSamples sendSamples = *sendBuffer;
          sendBuffer->samplesAreReady = false; // oscRader will set this flag when buffer is the next time ready for sending
          // swap bytes if javascript client is big endian
          int sendBytes = sendSamples.sampleCount * sizeof (oscSample);   // number of 8 bit bytes = number of samles * 6, since there are 6 bytes used by each sample
          int sendWords = sendBytes >> 1;                                 // number of 16 bit words = number of bytes / 2
          if (clientIsBigEndian) {
            uint16_t *w = (uint16_t *) &sendSamples;
            for (size_t i = 0; i < sendWords; i ++) w [i] = htons (w [i]);
          }
          if (!webSocket->sendBinary ((byte *) &sendSamples,  sendBytes)) return;
          // DEBUG: Serial.printf ("[oscilloscope] sent   samples: %i   bytes: %i\n", sendSamples.sampleCount, sendBytes);
        }
    
        // read (text) stop command form javscrip client if it arrives - according to oscilloscope protocol the string could only be 'stop' - so there is no need checking it
        if (webSocket->available () != WebSocket::NOT_AVAILABLE) return; // this also covers ERROR and TIME_OUT
      }
    }

    // main oscilloscope function - it reads request from javascript client then starts two threads: oscilloscope reader (that reads samples ans packs them into buffer) and oscilloscope sender (that sends buffer to javascript client)

    void runOscilloscope (WebSocket *webSocket) {
    
      // set up oscilloscope shared memory that will be shared among all 3 oscilloscope threads
      oscSharedMemory sharedMemory = {};                           // get some memory that will be shared among all oscilloscope threads and initialize it with zerros
      sharedMemory.webSocket = webSocket;                          // put webSocket rference into shared memory
      sharedMemory.readBuffer.samplesAreReady = true;              // this value will be copied into sendBuffer later where this flag will be checked
    
      // oscilloscope protocol starts with binary endian identification from the client
      uint16_t endianIdentification = 0;
      if (webSocket->readBinary ((byte *) &endianIdentification, sizeof (endianIdentification)) == sizeof (endianIdentification))
        sharedMemory.clientIsBigEndian = (endianIdentification == 0xBBAA); // cient has sent 0xAABB
      if (!(endianIdentification == 0xAABB || endianIdentification == 0xBBAA)) {
        #ifdef __DMESG__
          dmesg ("[oscilloscope] communication does not follow oscilloscope protocol. Expected endian identification.");
        #endif
        webSocket->sendString ("[oscilloscope] communication does not follow oscilloscope protocol. Expected endian identification."); // send error also to javascript client
        return;
      }
    
      // oscilloscope protocol continues with (text) start command in the following forms:
      // start digital sampling on GPIO 36 every 250 ms screen width = 10000 ms
      // start analog sampling on GPIO 22, 23 every 100 ms screen width = 400 ms set positive slope trigger to 512 set negative slope trigger to 0
      string s = webSocket->readString (); 
      // DEBUG: Serial.printf ("[oscilloscope] command: %s\n", s);

      if (s == "") {
        #ifdef __DMESG__
          dmesg ("[oscilloscope] communication does not follow oscilloscope protocol. Expected start oscilloscope parameters.");
        #endif
        webSocket->sendString ("[oscilloscope] communication does not follow oscilloscope protocol. Expected start oscilloscope parameters."); // send error also to javascript client
        return;
      }
      
      // try to parse what we have got from client
      char posNeg1 [9] = "";
      char posNeg2 [9] = "";
      int treshold1;
      int treshold2;
      char *cmdPart1 = (char *) s;
      char *cmdPart2 = strstr (cmdPart1, " every"); 
      char *cmdPart3 = NULL;
      if (cmdPart2) {
        *(cmdPart2++) = 0;
        cmdPart3 = strstr (cmdPart2, " set"); 
        if (cmdPart3) 
          *(cmdPart3++) = 0;
      }
      // parse 1st part
      sharedMemory.gpio1 = sharedMemory.gpio2 = 255; // invalid GPIO
      if (sscanf (cmdPart1, "start %7s sampling on GPIO %2i, %2i", sharedMemory.readType, &sharedMemory.gpio1, &sharedMemory.gpio2) < 2) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] oscilloscope protocol syntax error.");
        #endif
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;
      }
      // use adc1_get_raw instead of analogRead
      if (!strcmp (sharedMemory.readType, "analog")) {
        switch (sharedMemory.gpio1) {
          // ADC1
          case 36: sharedMemory.adcchannel1 = ADC1_CHANNEL_0; break;
          case 37: sharedMemory.adcchannel1 = ADC1_CHANNEL_1; break;
          case 38: sharedMemory.adcchannel1 = ADC1_CHANNEL_2; break;
          case 39: sharedMemory.adcchannel1 = ADC1_CHANNEL_3; break;
          case 32: sharedMemory.adcchannel1 = ADC1_CHANNEL_4; break;
          case 33: sharedMemory.adcchannel1 = ADC1_CHANNEL_5; break;
          case 34: sharedMemory.adcchannel1 = ADC1_CHANNEL_6; break;
          case 35: sharedMemory.adcchannel1 = ADC1_CHANNEL_7; break;
          // ADC2 (GPIOs 4, 0, 2, 15, 13, 12, 14, 27, 25, 26), reading blocks when used together with WiFi 
          // other GPIOs do not have ADC
          default:  webSocket->sendString (string ("[oscilloscope] can't analogRead GPIO ") + string (sharedMemory.gpio1) + (char *) "."); // send error also to javascript client
                    return;  
        }
        switch (sharedMemory.gpio2) {
          // ADC1
          case 36: sharedMemory.adcchannel2 = ADC1_CHANNEL_0; break;
          case 37: sharedMemory.adcchannel2 = ADC1_CHANNEL_1; break;
          case 38: sharedMemory.adcchannel2 = ADC1_CHANNEL_2; break;
          case 39: sharedMemory.adcchannel2 = ADC1_CHANNEL_3; break;
          case 32: sharedMemory.adcchannel2 = ADC1_CHANNEL_4; break;
          case 33: sharedMemory.adcchannel2 = ADC1_CHANNEL_5; break;
          case 34: sharedMemory.adcchannel2 = ADC1_CHANNEL_6; break;
          case 35: sharedMemory.adcchannel2 = ADC1_CHANNEL_7; break;
          // not used
          case 255: break;
          // ADC2 (GPIOs 4, 0, 2, 15, 13, 12, 14, 27, 25, 26), reading blocks when used together with WiFi 
          // other GPIOs do not have ADC
          default:  webSocket->sendString (string ("[oscilloscope] can't analogRead GPIO ") + string (sharedMemory.gpio2) + (char *) "."); // send error also to javascript client
                    return;  
        }        
      }
      
      // parse 2nd part
      if (!cmdPart2) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] oscilloscope protocol syntax error.");
        #endif
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;        
      }
      if (sscanf (cmdPart2, "every %i %2s screen width = %i %2s", &sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, &sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit) != 4) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] oscilloscope protocol syntax error.");
        #endif
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;    
      }
          
      // parse 3rd part
      if (cmdPart3) { 
        switch (sscanf (cmdPart3, "set %8s slope trigger to %i set %8s slope trigger to %i", posNeg1, &treshold1, posNeg2, &treshold2)) {
          case 0: // no trigger
                  break;
          case 4: // two triggers
                  if (!strcmp (posNeg2, "positive")) {
                    sharedMemory.positiveTrigger = true;
                    sharedMemory.positiveTriggerTreshold = treshold2;
                  }
                  if (!strcmp (posNeg2, "negative")) {
                    sharedMemory.negativeTrigger = true;
                    sharedMemory.negativeTriggerTreshold = treshold2;
                  }    
                  // don't break, continue to the next case
          case 2: // one trigger
                  if (!strcmp (posNeg1, "positive")) {
                    sharedMemory.positiveTrigger = true;
                    sharedMemory.positiveTriggerTreshold = treshold1;
                  }
                  if (!strcmp (posNeg1, "negative")) {
                    sharedMemory.negativeTrigger = true;
                    sharedMemory.negativeTriggerTreshold = treshold1;
                  }
                  break;
          default:
                  #ifdef __DMESG__
                      dmesg ("[oscilloscope] oscilloscope protocol syntax error.");
                  #endif
                  webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
                  return;    
        }
      }

      // DEBUG: Serial.printf ("[oscilloscope] parsing command: samplingTime = %i %s, screenWidth = %i %s\n", sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit);

      // check the values and calculate derived values
      if (!(!strcmp (sharedMemory.readType, "analog") || !strcmp (sharedMemory.readType, "digital"))) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] wrong readType. Read type can only be analog or digital.");
        #endif
        webSocket->sendString ("[oscilloscope] wrong readType. Read type can only be analog or digital."); // send error also to javascript client
        return;    
      }
      if (sharedMemory.gpio1 < 0 || sharedMemory.gpio2 < 0) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] invalid GPIO.");
        #endif
        webSocket->sendString ("[oscilloscope] invalid GPIO."); // send error also to javascript client
        return;      
      }
      if (!(sharedMemory.samplingTime >= 1 && sharedMemory.samplingTime <= 25000)) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] invalid sampling time. Sampling time must be between 1 and 25000.");
        #endif
        webSocket->sendString ("[oscilloscope] invalid sampling time. Sampling time must be between 1 and 25000."); // send error also to javascript client
        return;      
      }
      if (strcmp (sharedMemory.samplingTimeUnit, "ms") && strcmp (sharedMemory.samplingTimeUnit, "us")) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] wrong samplingTimeUnit. Sampling time unit can only be ms or us.");
        #endif
        webSocket->sendString ("[oscilloscope] wrong samplingTimeUnit. Sampling time unit can only be ms or us."); // send error also to javascript client
        return;    
      }
      if (!(sharedMemory.screenWidthTime >= 4 * sharedMemory.samplingTime && sharedMemory.screenWidthTime <= 1250000)) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] the settings exceed oscilloscope capabilities.");
        #endif
        webSocket->sendString ("[oscilloscope] the settings exceed oscilloscope capabilities."); // send error also to javascript client
        return;      
      }

      if (strcmp (sharedMemory.screenWidthTimeUnit, sharedMemory.samplingTimeUnit)) {
        #ifdef __DMESG__
            dmesg ("[oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit.");
        #endif        
        webSocket->sendString ("[oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit."); // send error also to javascript client
        return;    
      }

      // DEBUG: Serial.printf ("[oscilloscope] parsing4 command: samplingTime = %i %s, screenWidth = %i %s\n", sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit);
      
      if (sharedMemory.positiveTrigger) {
        if (sharedMemory.positiveTriggerTreshold > 0 && sharedMemory.positiveTriggerTreshold <= (strcmp (sharedMemory.readType, "analog") ? 1 : 4095)) {
          ;// Serial.printf ("[oscilloscope] positive slope trigger treshold = %i\n", sharedMemory.positiveTriggerTreshold);
        } else {
          #ifdef __DMESG__
              dmesg ("[oscilloscope] invalid positive slope trigger treshold (according to other settings).");
          #endif
          webSocket->sendString ("[oscilloscope] invalid positive slope trigger treshold (according to other settings)."); // send error also to javascript client
          return;      
        }
      }
      if (sharedMemory.negativeTrigger) {
        if (sharedMemory.negativeTriggerTreshold >= 0 && sharedMemory.negativeTriggerTreshold < (strcmp (sharedMemory.readType, "analog") ? 1 : 4095)) {
          ;//Serial.printf ("[oscilloscope] negative slope trigger treshold = %i\n", sharedMemory.negativeTriggerTreshold);
        } else {
          #ifdef __DMESG__
              dmesg ("[oscilloscope] invalid negative slope trigger treshold (according to other settings).");
          #endif
          webSocket->sendString ("[oscilloscope] invalid negative slope trigger treshold (according to other settings)."); // send error also to javascript client
          return;      
        }
      }
  
      // DEBUG: Serial.printf ("[oscilloscope] parsing5 command: samplingTime = %i %s, screenWidth = %i %s\n", sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit);

      sharedMemory.oscReaderState = INITIAL;

      #ifdef OSCILLOSCOPE_READER_CORE
          BaseType_t taskCreated = xTaskCreatePinnedToCore (oscReader, "oscReader", 4 * 1024, (void *) &sharedMemory, OSCILLOSCOPE_READER_PRIORITY, NULL, OSCILLOSCOPE_READER_CORE);
      #else
          BaseType_t taskCreated = xTaskCreate (oscReader, "oscReader", 4 * 1024, (void *) &sharedMemory, OSCILLOSCOPE_READER_PRIORITY, NULL);
      #endif
      if (pdPASS != taskCreated) {
            #ifdef __DMESG__
                dmesg ("[oscilloscope] could not start oscReader");
            #endif
      } else {

                // send oscReader START signal and wait until STARTED
                sharedMemory.oscReaderState = START; 
                while (sharedMemory.oscReaderState != STARTED) delay (1); 

        // start oscilloscope sender in this thread

        oscSender ((void *) &sharedMemory); 
        // stop reader - we can not simply vTaskDelete (oscReaderHandle) since this could happen in the middle of analogRead which would leave its internal semaphore locked
        // vTaskDelete (oscReaderHandle);

                // send oscReader STOP signal
                sharedMemory.oscReaderState = STOP; 

                // wait until oscReader STOPPED
                while (sharedMemory.oscReaderState != STOPPED) delay (1); 
      }
      
      return;
    }
