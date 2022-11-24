/*
 
    oscilloscope.h
 
    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
                
    Issues:
            - when WiFi is in WIFI_AP or WIFI_STA_AP mode is oscillospe causes WDT problem when working at higher frequenceses

    October, 23, 2022, Bojan Jurca
             
*/


    // ----- includes, definitions and supporting functions -----

    #include <WiFi.h>
    #include <soc/gpio_sig_map.h> // to digitalRead PWM and other GPUOs ...
    #include <soc/io_mux_reg.h>   // thanks to gin66: https://github.com/BojanJurca/Esp32_oscilloscope/issues/19    
    #include "driver/adc.h"       // to use adc1_get_raw instead of analogRead


    // ----- CODE -----

    #include "httpServer.hpp"                 // oscilloscope uses websockets defined in webServer.hpp  

      // deprecated, for backwards compatibility only:
      [[deprecated("adc (channel) is deprecated, use adc1_get_raw (channel) instead.")]]
      inline int16_t adc (adc1_channel_t channel) __attribute__((always_inline));
      int16_t adc (adc1_channel_t channel) { return adc1_get_raw (channel); };

    // oscilloscope samples
    struct oscSample {                        // one sample
       int16_t signal1;                       // signal value of 1st GPIO read by analogRead or digialRead   
       int16_t signal2;                       // signal value of 2nd GPIO if requested   
       int16_t deltaTime;                     // sample time - offset from previous sample in ms or us  
    }; // = 6 bytes per sample
    
    struct oscSamples {                       // buffer with samples
       oscSample samples [64];                // sample buffer will never exceed 41 samples, make it 64 - that will simplify the code and thus making it faster    
       int sampleCount;                       // number of samples in the buffer
       bool samplesAreReady;                  // is the buffer ready for sending
    }; // = max 64 samples of 384 bytes

    enum readerState { readerRunning = 0, readerShouldStop =  1, readerStopped = 2 };

    struct oscSharedMemory {                  // data structure to be shared among oscilloscope tasks
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
      unsigned long screenRefreshPeriod;      // approximately 20 Hz, in ms
      int screenRefreshModulus;               // used to reduce refresh frequency down to approximatelly sustainable 20 Hz (samples are passet via websocket to internet browser)
      bool oneSampleAtATime;                  // if horizontl frequency <= 1 Hz sample by sample, whole screen (all samples that fit on one screen) at a time otherwise
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
      int16_t samplingTime =              ((oscSharedMemory *) sharedMemory)->samplingTime;
      bool positiveTrigger =              ((oscSharedMemory *) sharedMemory)->positiveTrigger;
      bool negativeTrigger =              ((oscSharedMemory *) sharedMemory)->negativeTrigger;
      unsigned char gpio1 =               (unsigned char) ((oscSharedMemory *) sharedMemory)->gpio1; // easier to check validity with unsigned char then with integer 
      unsigned char gpio2 =               (unsigned char) ((oscSharedMemory *) sharedMemory)->gpio2; // easier to check validity with unsigned char then with integer
      adc1_channel_t adcchannel1 =        ((oscSharedMemory *) sharedMemory)->adcchannel1;
      adc1_channel_t adcchannel2 =        ((oscSharedMemory *) sharedMemory)->adcchannel2;
      int16_t positiveTriggerTreshold =   ((oscSharedMemory *) sharedMemory)->positiveTriggerTreshold;
      int16_t negativeTriggerTreshold =   ((oscSharedMemory *) sharedMemory)->negativeTriggerTreshold;
      int screenWidthTime =               ((oscSharedMemory *) sharedMemory)->screenWidthTime; 
      unsigned long screenRefreshPeriod = ((oscSharedMemory *) sharedMemory)->screenRefreshPeriod; 
      bool oneSampleAtATime =             ((oscSharedMemory *) sharedMemory)->oneSampleAtATime;
      int screenRefreshModulus =          ((oscSharedMemory *) sharedMemory)->screenRefreshModulus;  
      
      oscSamples *readBuffer =   &((oscSharedMemory *) sharedMemory)->readBuffer;
      oscSamples *sendBuffer =   &((oscSharedMemory *) sharedMemory)->sendBuffer;
          
      int screenTime;                     // how far we have already got from the left of the screen (we'll compare this value with screenWidthTime)
      int16_t deltaTime;                  // how far last sample is from the previous one
      int screenRefreshCounter = 0;

      // thanks to gin66 (https://github.com/BojanJurca/Esp32_oscilloscope/issues/19 we can also read GPIOs that were configured for OUTPUT or PWM
      if (gpio1 <= 39) PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG [gpio1]);
      if (gpio2 <= 39) PIN_INPUT_ENABLE (GPIO_PIN_MUX_REG [gpio2]);
            
      while (((oscSharedMemory *) sharedMemory)->oscReaderState == readerRunning) {
    
        // insert first dummy sample to read-buffer that tells javascript client to start drawing from the left of the screen
        readBuffer->samples [0] = {-1, -1, -1}; // no normal data sample can look like this
        readBuffer->sampleCount = 1;
    
        unsigned long lastSampleTime = unitIsMicroSeconds ? micros () : millis ();
        screenTime = 0; 
        bool triggeredMode = positiveTrigger || negativeTrigger;
    
        if (triggeredMode) { // if no trigger is set then skip this (waiting) part and start sampling immediatelly
    
          lastSampleTime = unitIsMicroSeconds ? micros () : millis ();
          oscSample lastSample; 
          if (doAnalogRead) lastSample = {adc1_get_raw (adcchannel1), gpio2 < 100 ? adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) 0}; else lastSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) 0}; // gpio1 should always be valid 
          
          if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime); 

          while (((oscSharedMemory *) sharedMemory)->oscReaderState == readerRunning) { // wait for trigger condition
            taskYIELD (); // give other tasks a chance to run
            unsigned long newSampleTime = unitIsMicroSeconds ? micros () : millis ();
            oscSample newSample; 
            // if (doAnalogRead) newSample = {(int16_t) analogRead (gpio1), gpio2 < 100 ? (int16_t) analogRead (gpio2) : (int16_t) -1, (int16_t) (screenTime = newSampleTime - lastSampleTime)}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) (screenTime = newSampleTime - lastSampleTime)}; // gpio1 should always be valid 
            if (doAnalogRead) newSample = {adc1_get_raw (adcchannel1), gpio2 < 100 ? adc1_get_raw (adcchannel2) : (int16_t) -1, (int16_t) (screenTime = newSampleTime - lastSampleTime)}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, (int16_t) (screenTime = newSampleTime - lastSampleTime)}; // gpio1 should always be valid 

            if ((positiveTrigger && lastSample.signal1 < positiveTriggerTreshold && newSample.signal1 >= positiveTriggerTreshold) || (negativeTrigger && lastSample.signal1 > negativeTriggerTreshold && newSample.signal1 <= negativeTriggerTreshold)) { // only gpio1 is used to trigger the sampling 
              // insert both samples into read buffer
              lastSample.deltaTime = 0;             // correct timing for last sample - it should alwaye be displayed leftmost
              readBuffer->samples [1] = lastSample; // timeOffset (from left of the screen) = 0
              readBuffer->samples [2] = newSample;  // this is the first sample after triggered
              screenTime = newSample.deltaTime;     // start measuring screen time from new sample on
              lastSampleTime = newSampleTime;
              readBuffer->sampleCount = 3;
              
              if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime);
              
              break; // trigger event occured, stop waiting and proceed to sampling
            }
            
            lastSample = newSample;
            lastSampleTime = newSampleTime;
            if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime);
    
          } // while not triggered
         
        } // if triggered mode
    
        // take (the rest of the) samples that fit on one screen
        while (((oscSharedMemory *) sharedMemory)->oscReaderState == readerRunning) { // while screenTime < screenWidthTime
         
          if (oneSampleAtATime && readBuffer->sampleCount) {
            // copy read buffer to send buffer so that oscilloscope sender can send it to javascript client 
            while (sendBuffer->samplesAreReady) if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime); // wait until sendBuffer has been send and is free
            *sendBuffer = *readBuffer;
            readBuffer->sampleCount = 0; // empty read buffer so we don't send the same data again later
          }
    
          // if we are past screenWidthTime
          if (screenTime >= screenWidthTime) { 
            
            // but only if modulus == 0 to reduce refresh frequency to sustainable 20 Hz
            if (triggeredMode || !(screenRefreshCounter = (screenRefreshCounter + 1) % screenRefreshModulus)) {
              // copy read buffer to send buffer
              if (!sendBuffer->samplesAreReady) *sendBuffer = *readBuffer; // this also copies 'ready' flag from read buffer which is 'true'
            }
            if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime);
            break; // get out of while loop to start sampling from the left of the screen
          } 
          // else continue sampling
    
          unsigned long newSampleTime = unitIsMicroSeconds ? micros () : millis ();
          screenTime += (deltaTime = newSampleTime - lastSampleTime);      
          lastSampleTime = newSampleTime;        
          oscSample newSample; 
          // if (doAnalogRead) newSample = {(int16_t) analogRead (gpio1), gpio2 < 100 ? (int16_t) analogRead (gpio2) : (int16_t) -1, deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, deltaTime}; // gpio1 should always be valid 
          if (doAnalogRead) newSample = {adc1_get_raw (adcchannel1), gpio2 < 100 ? adc1_get_raw (adcchannel2) : (int16_t) -1, deltaTime}; else newSample = {(int16_t) digitalRead (gpio1), gpio2 < 100 ? (int16_t) digitalRead (gpio2) : (int16_t) -1, deltaTime}; // gpio1 should always be valid 

          readBuffer->samples [readBuffer->sampleCount] = newSample;
          readBuffer->sampleCount = (readBuffer->sampleCount + 1) & 0b00111111; // 0 .. 63 max (which is inside buffer size) - just in case, the number of samples will never exceed 41  
    
          if (unitIsMicroSeconds) delayMicroseconds (samplingTime); else delay (samplingTime);
        
        } // while screenTime < screenWidthTime
    
        // in triggered mode we have to wait for refresh time to pass before trying again
        // one screen frame has already been sent, we have to wait yet screenRefreshModulus - 1 screen frames
        // for the whole screen refresh time to pass
        if (triggeredMode) {
          if (unitIsMicroSeconds) delay (screenRefreshPeriod - screenWidthTime / 1000); // screenRefreshPeriod is always in ms, never in us
          else if (screenRefreshPeriod > screenWidthTime) delay (screenRefreshPeriod - screenWidthTime);
               else                                       delay (screenRefreshPeriod);
        } else {
          taskYIELD (); // give other tasks a chance to run
        }
        
      } // while (true)

      ((oscSharedMemory *) sharedMemory)->oscReaderState = readerStopped;
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
          oscSamples sendSamples; 
          sendSamples = *sendBuffer;
          sendBuffer->samplesAreReady = false; // oscRader will set this flag when buffer is the next time ready for sending
          // swap bytes if javascript client is big endian
          int sendBytes = sendSamples.sampleCount * sizeof (oscSample);   // number of 8 bit bytes = number of samles * 6, since there are 6 bytes used by each sample
          int sendWords = sendBytes >> 1;                                 // number of 16 bit words = number of bytes / 2
          if (clientIsBigEndian) {
            uint16_t *w = (uint16_t *) &sendSamples;
            for (size_t i = 0; i < sendWords; i ++) w [i] = htons (w [i]);
          }
          if (!webSocket->sendBinary ((byte *) &sendSamples,  sendBytes)) return;
        }
    
        // read (text) stop command form javscrip client if it arrives - according to oscilloscope protocol the string could only be 'stop' - so there is no need checking it
        // if (webSocket->available () == WebSocket::STRING) { String s = webSocket->readString (); Serial.println (s); return; }
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
        Serial.println ("[oscilloscope] communication does not follow oscilloscope protocol. Expected endian identification.");
        webSocket->sendString ("[oscilloscope] communication does not follow oscilloscope protocol. Expected endian identification."); // send error also to javascript client
        return;
      }
      // Serial.printf ("[oscilloscope] javascript client is %s endian.\n", sharedMemory.clientIsBigEndian ? "big" : "little");
    
      // oscilloscope protocol continues with (text) start command in the following forms:
      // start digital sampling on GPIO 36 every 250 ms screen width = 10000 ms
      // start analog sampling on GPIO 22, 23 every 100 ms screen width = 400 ms set positive slope trigger to 512 set negative slope trigger to 0
      String s = webSocket->readString (); 
      
      // try to parse what we have got from client
      char posNeg1 [9] = "";
      char posNeg2 [9] = "";
      int treshold1;
      int treshold2;
      char *cmdPart1 = (char *) s.c_str ();
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
        Serial.println ("[oscilloscope] oscilloscope protocol syntax error.");
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;
      }
      Serial.printf ("[oscilloscope] parsing command: readType = %s, gpio1 = %i, gpio2 = %s\n", sharedMemory.readType, sharedMemory.gpio1, sharedMemory.gpio2 != 255 ? String (sharedMemory.gpio2).c_str () : "(not defined)");
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
          default:  webSocket->sendString ("[oscilloscope] can't analogRead GPIO " + String (sharedMemory.gpio1) + "."); // send error also to javascript client
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
          default:  webSocket->sendString ("[oscilloscope] can't analogRead GPIO " + String (sharedMemory.gpio2) + "."); // send error also to javascript client
                    return;  
        }        
      }
      
      // parse 2nd part
      if (!cmdPart2) {
        Serial.println ("[oscilloscope] oscilloscope protocol syntax error.");
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;        
      }
      if (sscanf (cmdPart2, "every %i %2s screen width = %i %2s", &sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, &sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit) != 4) {
        Serial.println ("[oscilloscope] oscilloscope protocol syntax error.");
        webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
        return;    
      }
      Serial.printf ("[oscilloscope] parsing command: samplingTime = %i %s, screenWidth = %i %s\n", sharedMemory.samplingTime, sharedMemory.samplingTimeUnit, sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit);
          
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
                  Serial.println ("[oscilloscope] oscilloscope protocol syntax error.");
                  webSocket->sendString ("[oscilloscope] oscilloscope protocol syntax error."); // send error also to javascript client
                  return;    
        }
        Serial.printf ("[oscilloscope] parsing command: positiveTrigger = %s, negativeTrigger = %s\n", sharedMemory.positiveTrigger ? String (sharedMemory.positiveTriggerTreshold).c_str () : "(not defined)", sharedMemory.negativeTrigger ? String (sharedMemory.negativeTriggerTreshold).c_str () : "(not defined)");
      }
    
      // check the values and calculate derived values
      if (!(!strcmp (sharedMemory.readType, "analog") || !strcmp (sharedMemory.readType, "digital"))) {
        Serial.println ("[oscilloscope] wrong readType. Read type can only be analog or digital.");
        webSocket->sendString ("[oscilloscope] wrong readType. Read type can only be analog or digital."); // send error also to javascript client
        return;    
      }
      if (sharedMemory.gpio1 < 0 || sharedMemory.gpio2 < 0) {
        Serial.println ("[oscilloscope] invalid GPIO.");
        webSocket->sendString ("[oscilloscope] invalid GPIO."); // send error also to javascript client
        return;      
      }
      if (!(sharedMemory.samplingTime >= 1 && sharedMemory.samplingTime <= 25000)) {
        Serial.println ("[oscilloscope] invalid sampling time. Sampling time must be between 1 and 25000.");
        webSocket->sendString ("[oscilloscope] invalid sampling time. Sampling time must be between 1 and 25000."); // send error also to javascript client
        return;      
      }
      if (strcmp (sharedMemory.samplingTimeUnit, "ms") && strcmp (sharedMemory.samplingTimeUnit, "us")) {
        Serial.println ("[oscilloscope] wrong samplingTimeUnit. Sampling time unit can only be ms or us.");
        webSocket->sendString ("[oscilloscope] wrong samplingTimeUnit. Sampling time unit can only be ms or us."); // send error also to javascript client
        return;    
      }
      if (!(sharedMemory.screenWidthTime >= 4 * sharedMemory.samplingTime && sharedMemory.screenWidthTime <= 1250000)) {
        Serial.println ("[oscilloscope] invalid screen width time. Screen width time must be between 4 * sampling time and 125000.");
        webSocket->sendString ("[oscilloscope] invalid screen width time. Screen width time must be between 4 * sampling time and 125000."); // send error also to javascript client
        return;      
      }
    
    
      if (!strcmp (sharedMemory.samplingTimeUnit, "us")) {
        // calculate delayMicroseconds correction for more accurrate timing
        //  80 MHz CPU analogRead takes 100 us, digitalRead takes  6 us
        // 160 MHz CPU analogRead takes  80 us, digitalRead takes  3 us
        // 240 MHz CPU analogRead takes  60 us, digitalRead takes  2 us
        int correction;
        if (!strcmp (sharedMemory.readType, "analog")) correction = ESP.getCpuFreqMHz () < 240 ? ( ESP.getCpuFreqMHz () < 160 ? 100 : 90 ) : 60; 
        else                                           correction = ESP.getCpuFreqMHz () < 240 ? ( ESP.getCpuFreqMHz () < 160 ?   6 :  3 ) :  2; 
        sharedMemory.samplingTime -= correction;
        if (sharedMemory.samplingTime < 0) sharedMemory.samplingTime = 0;
      }

    
      if (strcmp (sharedMemory.screenWidthTimeUnit, sharedMemory.samplingTimeUnit)) {
        Serial.println ("[oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit.");
        webSocket->sendString ("[oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit."); // send error also to javascript client
        return;    
      }
      // calculate modulus so screen refresh frequency would be somewhere near 20 Hz which can still be trensfered via websocket and displayed on browser window 
      if (!strcmp (sharedMemory.screenWidthTimeUnit, "ms")) {
        sharedMemory.screenRefreshModulus = 50 / sharedMemory.screenWidthTime; // 50 ms corresponds to 20 Hz
        if (!sharedMemory.screenRefreshModulus) { // screen refresh frequency is <= 20 Hz which can be displayed without problems
          sharedMemory.screenRefreshModulus = 1;
          sharedMemory.screenRefreshPeriod = 50;
        } else {
          sharedMemory.screenRefreshPeriod = sharedMemory.screenWidthTime * sharedMemory.screenRefreshModulus;
        }
        sharedMemory.oneSampleAtATime = (sharedMemory.screenWidthTime > 1000); // if horizontal freequency < 1 then display samples one at a time
      } else { // screen width time is in us
        sharedMemory.screenRefreshModulus = 50000 / sharedMemory.screenWidthTime; // 50000 us corresponds to 20 Hz
        if (!sharedMemory.screenRefreshModulus) { // screen refresh frequency is <= 20 Hz which can be displayed without problems
          sharedMemory.screenRefreshModulus = 1;
          sharedMemory.screenRefreshPeriod = 50;
        } else {
          sharedMemory.screenRefreshPeriod = sharedMemory.screenWidthTime * sharedMemory.screenRefreshModulus / 1000;
        }
      }
      Serial.printf ("[oscilloscope] screenWidthTime = %i %s, screenRefreshModulus = %i, screenRefreshPeriod = %lu ms, oneSampleAtATime = %i\n", sharedMemory.screenWidthTime, sharedMemory.screenWidthTimeUnit, sharedMemory.screenRefreshModulus, sharedMemory.screenRefreshPeriod, sharedMemory.oneSampleAtATime);
      
      // TO DO: calculate correction for short timing to produce beter results
      
      if (sharedMemory.positiveTrigger) {
        if (sharedMemory.positiveTriggerTreshold > 0 && sharedMemory.positiveTriggerTreshold <= (strcmp (sharedMemory.readType, "analog") ? 1 : 4095)) {
          Serial.printf ("[oscilloscope] positive slope trigger treshold = %i\n", sharedMemory.positiveTriggerTreshold);
        } else {
          Serial.println ("[oscilloscope] invalid positive slope trigger treshold (according to other settings).");
          webSocket->sendString ("[oscilloscope] invalid positive slope trigger treshold (according to other settings)."); // send error also to javascript client
          return;      
        }
      }
      if (sharedMemory.negativeTrigger) {
        if (sharedMemory.negativeTriggerTreshold >= 0 && sharedMemory.negativeTriggerTreshold < (strcmp (sharedMemory.readType, "analog") ? 1 : 4095)) {
          Serial.printf ("[oscilloscope] negative slope trigger treshold = %i\n", sharedMemory.negativeTriggerTreshold);
        } else {
          Serial.println ("[oscilloscope] invalid negative slope trigger treshold (according to other settings).");
          webSocket->sendString ("[oscilloscope] invalid negative slope trigger treshold (according to other settings)."); // send error also to javascript client
          return;      
        }
      }
    
      #ifdef __PERFMON__
        xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
          __perfCurrentOscWebSockets__ ++;
        xSemaphoreGive (__httpServerSemaphore__);
      #endif

      // start oscillocope reader in a separate thread
      TaskHandle_t oscReaderHandle = NULL;
      #define tskNORMAL_PRIORITY 1
      if (pdPASS != xTaskCreate ( oscReader, 
                                  "oscReader", 
                                  4096, 
                                  (void *) &sharedMemory, // pass shared memmory address as parameter to oscReader
                                  tskNORMAL_PRIORITY,
                                  &oscReaderHandle)) {
            Serial.printf ("[oscilloscope] could not start oscReader\n");
      } else {
        Serial.printf ("[oscilloscope] oscReader started\n");
        // start oscilloscope sender in this thread
        oscSender ((void *) &sharedMemory); 
        // stop reader - we cn not simply vTaskDelete (oscReaderHandle) since this could happen in the middle of analogRead which would leave its internal semaphore locked
        // vTaskDelete (oscReaderHandle);
        sharedMemory.oscReaderState = readerShouldStop; // tell oscReader to stop
        while (sharedMemory.oscReaderState != readerStopped) delay (1); // wait until stopped os oscReader can still access sharedMemory meanwhile
      }

      #ifdef __PERFMON__
        xSemaphoreTake (__httpServerSemaphore__, portMAX_DELAY);
          __perfCurrentOscWebSockets__ --;
        xSemaphoreGive (__httpServerSemaphore__);
      #endif
      
      return;
    }
