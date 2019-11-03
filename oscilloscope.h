/*
 * 
 * Oscilloscope.h
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 * 
 * History:
  *         - first release,
 *            August, 14, 2019, Bojan Jurca
 *          - oscilloscope structures and functions moved into separate file, 
 *            October 2, 2019, Bojan Jurca
 *            
 */


// ----- includes, definitions and supporting functions -----

#include <WiFi.h>
#include "webServer.hpp"        // oscilloscope uses websockets defined in  webServer.hpp  


typedef struct oscilloscopeSample {
   int16_t value;       // sample value read by analogRead or digialRead   
   int16_t timeOffset;  // sampe time offset drom previous sample in ms or us
};

typedef struct oscilloscopeSamples {
   oscilloscopeSample samples [64]; // sample buffer will never exceed 41 samples, make it 64 for simplicity reasons    
   int count;                       // number of samples in the buffer
   bool ready;                      // is the buffer ready for sending
};

typedef struct oscilloscopeSharedMemoryType { // data structure to be shared with oscilloscope tasks
  // basic objects for webSocket communication
  WebSocket *webSocket;               // open webSocket for communication with javascript client
  bool clientIsBigEndian;             // true if javascript client is big endian machine
  // sampling parameters
  char readType [8];                  // analog or digital  
  bool analog;                        // true if readType is analog, false if digital
  int gpio;                           // gpio in which ESP32 is taking samples from
  int samplingTime;                   // time between samples in ms or us
  char samplingTimeUnit [3];          // ms or us
  bool microSeconds;                  // true if samplingTimeunit is us, false if ms
  byte microSecondCorrection;         // delay correction for micro second tiing
  int screenWidthTime;                // oscilloscope screen width in ms or us
  char screenWidthTimeUnit [3];       // ms or us 
  int screenRefreshTime;              // time before screen is refreshed with new samples in ms or us
  long screenRefreshTimeCommonUnit;   // screen refresh time translated into same time unit as screen width time
  char screenRefreshTimeUnit [3];     // ms or us
  int screenRefreshModulus;           // used to reduce refresh frequency down to sustainable 20 Hz
  bool positiveTrigger;               // true if posotive slope trigger is set
  int positiveTriggerTreshold;        // positive slope trigger treshold value
  bool negativeTrigger;               // true if negative slope trigger is set  
  int negativeTriggerTreshold;        // negative slope trigger treshold value
  // buffers holding samples 
  oscilloscopeSamples readBuffer;     // we'll read samples into this buffer
  oscilloscopeSamples sendBuffer;     // we'll copy red buffer into this buffer before sending samples to the client
  portMUX_TYPE csSendBuffer;          // MUX for handling critical section while copying
  // status of oscilloscope threads
  bool readerIsRunning;
  bool senderIsRunning;  
};

void oscilloscopeReader (void *parameters) {
  unsigned char gpio =                ((oscilloscopeSharedMemoryType *) parameters)->gpio; 
  bool analog =                       !strcmp (((oscilloscopeSharedMemoryType *) parameters)->readType, "analog");
  int16_t samplingTime =              ((oscilloscopeSharedMemoryType *) parameters)->samplingTime;
  bool microSeconds =                 ((oscilloscopeSharedMemoryType *) parameters)->microSeconds;
  int screenWidthTime =               ((oscilloscopeSharedMemoryType *) parameters)->screenWidthTime; 
  int screenRefreshTime =             ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshTime;  
  long screenRefreshTimeCommonUnit =  ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshTimeCommonUnit;  
  int screenRefreshModulus =          ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshModulus;  
  oscilloscopeSamples *readBuffer =   &((oscilloscopeSharedMemoryType *) parameters)->readBuffer;
  oscilloscopeSamples *sendBuffer =   &((oscilloscopeSharedMemoryType *) parameters)->sendBuffer;
  bool positiveTrigger =              ((oscilloscopeSharedMemoryType *) parameters)->positiveTrigger;
  int16_t positiveTriggerTreshold =   ((oscilloscopeSharedMemoryType *) parameters)->positiveTriggerTreshold;
  bool negativeTrigger =              ((oscilloscopeSharedMemoryType *) parameters)->negativeTrigger;
  int16_t negativeTriggerTreshold =   ((oscilloscopeSharedMemoryType *) parameters)->negativeTriggerTreshold;
  portMUX_TYPE *csSendBuffer =        &((oscilloscopeSharedMemoryType *) parameters)->csSendBuffer;
  
  int screenTimeOffset = 0;
  int16_t sampleTimeOffset = 0;
  int screenRefreshCounter = 0;
  while (true) {
    
    // insert first dummy sample that tells javascript client to start drawing from the left
    readBuffer->samples [0] = {-1, -1};
    readBuffer->count = 1;

    unsigned int lastSampleTime = microSeconds ? micros () : millis ();
    screenTimeOffset = 0;    
    bool triggeredMode = positiveTrigger || negativeTrigger;

    if (triggeredMode) { // if no trigger is set then skip this part and start sampling immediatelly
      // Serial.printf ("[oscilloscope] waiting to be triggered ...\n");
      uint16_t lastSample = analog ? analogRead (gpio) : digitalRead (gpio);
      lastSampleTime = microSeconds ? micros () : millis ();

      while (true) { 
        if (microSeconds) SPIFFSsafeDelayMicroseconds (samplingTime);
        else              SPIFFSsafeDelay (samplingTime);
        uint16_t newSample = analog ? analogRead (gpio) : digitalRead (gpio);
        unsigned int newSampleTime = microSeconds ? micros () : millis ();
        if ((positiveTrigger && lastSample < positiveTriggerTreshold && newSample >= positiveTriggerTreshold) || 
            (negativeTrigger && lastSample > negativeTriggerTreshold && newSample <= negativeTriggerTreshold)) {
          // insert both samples into the buffer
          readBuffer->samples [1] = {lastSample, 0};
          readBuffer->samples [2] = {newSample, screenTimeOffset = newSampleTime - lastSampleTime};
          readBuffer->count = 3;
          break;
        }
        lastSample = newSample;
        lastSampleTime = newSampleTime;
        // stop reading if sender is not running any more
        if (!((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning) { 
          ((oscilloscopeSharedMemoryType *) parameters)->readerIsRunning = false;
          vTaskDelete (NULL); // instead of return; - stop this thread
        }
      } // while (true)
    } // if (positiveTrigger || negativeTrigger)

    // take (the rest of the) samples that fit into one screen
    do {

      unsigned int newSampleTime = microSeconds ? micros () : millis ();
      uint16_t sampleTimeOffset = newSampleTime - lastSampleTime;
      screenTimeOffset += sampleTimeOffset;      
      lastSampleTime = newSampleTime;        

      // if we are past screen refresh time copy readBuffer into sendBuffer then empty readBuffer
      if (screenTimeOffset >= screenRefreshTimeCommonUnit) {
        // but only if modulus == 0 to reduce refresh frequency to sustainable 20 Hz
        if (triggeredMode || !(screenRefreshCounter = (screenRefreshCounter + 1) % screenRefreshModulus)) {
          portENTER_CRITICAL (csSendBuffer);
          *sendBuffer = *readBuffer; // this also copies 'ready' flag which is 'true'
          portEXIT_CRITICAL (csSendBuffer);
        }
        // empty readBuffer
        readBuffer->count = 0; 
      } // if (screenTimeOffset >= screenRefreshTimeCommonUnit)
      // take the next sample
      readBuffer->samples [readBuffer->count] = {analog ? analogRead (gpio) : digitalRead (gpio), sampleTimeOffset};
      readBuffer->count = (readBuffer->count + 1) & 0b00111111; // 0 .. 63 max (which is inside buffer size) - just in case, number of samples will never exceed 41  
      // stop reading if sender is not running any more
      if (!((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning) { 
        ((oscilloscopeSharedMemoryType *) parameters)->readerIsRunning = false;
        vTaskDelete (NULL); // instead of return; - stop this thread
      }

      if (microSeconds) SPIFFSsafeDelayMicroseconds (samplingTime); 
      else              SPIFFSsafeDelay (samplingTime);

    } while (screenTimeOffset < screenWidthTime);

    // after the screen frame is processed we have to handle the preparations for the next screen frame differently for triggered and non triggered sampling
    if (triggeredMode) {    
      // in triggered mode we have to wait for refresh time to pass before trying again
      // one screen frame has already been sent, we have to wait yet screenRefreshModulus - 1 screen frames
      // for the whole screen refresh time to pass
      for (int i = 1; i < screenRefreshModulus; i++) {
        if (microSeconds) SPIFFSsafeDelayMicroseconds (screenRefreshTimeCommonUnit); 
        else              SPIFFSsafeDelay (screenRefreshTimeCommonUnit);        
      }
    } else {
      // correct sampleTimeOffset for the first sample int the next screen frame in non triggered mode
      sampleTimeOffset = screenTimeOffset - screenWidthTime;      
    }
  } // while (true)
}

void oscilloscopeSender (void *parameters) {
  WebSocket *webSocket =            ((oscilloscopeSharedMemoryType *) parameters)->webSocket; 
  bool clientIsBigEndian =          ((oscilloscopeSharedMemoryType *) parameters)->clientIsBigEndian;
  oscilloscopeSamples *sendBuffer = &((oscilloscopeSharedMemoryType *) parameters)->sendBuffer;
  portMUX_TYPE *csSendBuffer =      &((oscilloscopeSharedMemoryType *) parameters)->csSendBuffer;

  while (true) {
    SPIFFSsafeDelay (1);
    // send samples to javascript client if they are ready
    if (sendBuffer->ready) {
      oscilloscopeSamples samples;
      portENTER_CRITICAL (csSendBuffer);
      samples = *sendBuffer;
      sendBuffer->ready = false;
      portEXIT_CRITICAL (csSendBuffer);
      // swap bytes if javascript calient is big endian
      int size8_t = samples.count << 2;   // number of bytes = number of samles * 4
      int size16_t = samples.count << 1;  // number of 16 bit words = number of samles * 2
      if (clientIsBigEndian) {
        uint16_t *a = (uint16_t *) &samples;
        for (size_t i = 0; i < size16_t; i ++) a [i] = htons (a [i]);
      }
      if (!webSocket->sendBinary ((byte *) &samples,  size8_t)) {
        ((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning = false; // notify oscilloscope functions that session is finished
        vTaskDelete (NULL); // instead of return; - stop this task
      }
      // Serial.printf ("[oscilloscope] send %i samples, %i bytes to the client\n", samples.count, size8_t);
    }
    // read command form javscrip client if it arrives
    if (webSocket->available () == WebSocket::STRING) { // according to oscilloscope protocol the string could only be 'stop' - so there is no need checking it
      String s = webSocket->readString (); 
      Serial.printf ("[oscilloscope] %s\n", s.c_str ());
      ((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning = false; // notify oscilloscope functions that session is finished
      vTaskDelete (NULL); // instead of return; - stop this task
    }
  }
}

void runOscilloscope (WebSocket *webSocket) {
  // set up oscilloscope memory that will be shared among oscilloscope threads
  oscilloscopeSharedMemoryType oscilloscopeSharedMemory = {}; // get some memory that will be shared among all oscilloscope threads and initialize it with zerros
  oscilloscopeSharedMemory.webSocket = webSocket;             // put webSocket rference into shared memory
  oscilloscopeSharedMemory.readBuffer.ready = true;           // this value will be copied into sendBuffer later
  oscilloscopeSharedMemory.csSendBuffer = portMUX_INITIALIZER_UNLOCKED;   // initialize critical section mutex in shared memory

  // oscilloscope protocol starts with binary endian identification from the client
  uint16_t endianIdentification = 0;
  if (webSocket->readBinary ((byte *) &endianIdentification, sizeof (endianIdentification)) == sizeof (endianIdentification)) {
    oscilloscopeSharedMemory.clientIsBigEndian = (endianIdentification == 0xBBAA); // cient has sent 0xAABB
  } 
  if (endianIdentification == 0xAABB || endianIdentification == 0xBBAA) {
    Serial.printf ("[oscilloscope] clientIsBigEndian = %i\n", oscilloscopeSharedMemory.clientIsBigEndian);
  } else {
    Serial.printf ("[oscilloscope] communication does not follow oscilloscope protocol - expected endian identification\n");
    webSocket->sendString ("Oscilloscope error. Expected endian identification bytes but got something else."); // send error also to javascript client
    return;
  }

  // oscilloscope protocol continues with text start command in the following form:
  // "start digital sampling on GPIO 22 every 100 ms refresh screen of width 400 ms every 100 ms set positive slope trigger to 512 set negative slope trigger to 0"
  String s = webSocket->readString (); 
  Serial.printf ("[oscilloscope] %s\n", s.c_str ());
  // try to parse what we have got from client
  char posNeg1 [9] = "";
  char posNeg2 [9] = "";
  int treshold1;
  int treshold2;
  switch (sscanf (s.c_str (), "start %7s sampling on GPIO %i every %i %2s refresh screen of width %i %2s every %i %2s set %8s slope trigger to %i set %8s slope trigger to %i", 
                                oscilloscopeSharedMemory.readType, 
                                &oscilloscopeSharedMemory.gpio, 
                                &oscilloscopeSharedMemory.samplingTime, 
                                oscilloscopeSharedMemory.samplingTimeUnit, 
                                &oscilloscopeSharedMemory.screenWidthTime, 
                                oscilloscopeSharedMemory.screenWidthTimeUnit, 
                                &oscilloscopeSharedMemory.screenRefreshTime, 
                                oscilloscopeSharedMemory.screenRefreshTimeUnit,
                                posNeg1, &treshold1, posNeg2, &treshold2)) {
                                  
    case 8:   // no trigger
              // Serial.printf ("No trigger\n");
              break;

    case 12:  // two triggers
              // Serial.printf ("Two triggers\n");
              if (!strcmp (posNeg2, "positive")) {
                oscilloscopeSharedMemory.positiveTrigger = true;
                oscilloscopeSharedMemory.positiveTriggerTreshold = treshold2;
              }
              if (!strcmp (posNeg2, "negative")) {
                oscilloscopeSharedMemory.negativeTrigger = true;
                oscilloscopeSharedMemory.negativeTriggerTreshold = treshold2;
              }    
              // break; // no break;

    case 10:  // one trigger
              // Serial.printf ("One trigger\n");
              if (!strcmp (posNeg1, "positive")) {
                oscilloscopeSharedMemory.positiveTrigger = true;
                oscilloscopeSharedMemory.positiveTriggerTreshold = treshold1;
              }
              if (!strcmp (posNeg1, "negative")) {
                oscilloscopeSharedMemory.negativeTrigger = true;
                oscilloscopeSharedMemory.negativeTriggerTreshold = treshold1;
              }
              break;
              
    default:
              Serial.printf ("[oscilloscope] communication does not follow oscilloscope protocol\n");
              return;
  }

  // check the values and calculate derived values
  if (!strcmp (oscilloscopeSharedMemory.readType, "analog") || !strcmp (oscilloscopeSharedMemory.readType, "digital")) {
    Serial.printf ("[oscilloscope] readType = %s\n", oscilloscopeSharedMemory.readType);
    oscilloscopeSharedMemory.analog = !strcmp (oscilloscopeSharedMemory.readType, "analog");
    Serial.printf ("[oscilloscope] analog = %i\n", oscilloscopeSharedMemory.analog);
  } else {
    Serial.printf ("[oscilloscope] wrong readType\n");
    webSocket->sendString ("Oscilloscope error. Read type can only be analog or digital."); // send error also to javascript client
    return;    
  }
  if (oscilloscopeSharedMemory.gpio >= 0 && oscilloscopeSharedMemory.gpio <= 255) {
     Serial.printf ("[oscilloscope] GPIO = %i\n", oscilloscopeSharedMemory.gpio);
  } else {
    Serial.printf ("[oscilloscope] wrong GPIO\n");
    webSocket->sendString ("Oscilloscope error. GPIO must be between 0 and 255."); // send error also to javascript client
    return;      
  }
  if (oscilloscopeSharedMemory.samplingTime >= 1 && oscilloscopeSharedMemory.samplingTime <= 25000) {
     Serial.printf ("[oscilloscope] samplingTime = %i\n", oscilloscopeSharedMemory.samplingTime);
  } else {
    Serial.printf ("[oscilloscope] invalid sampling time\n");
    webSocket->sendString ("Oscilloscope error. Sampling time must be between 1 and 25000."); // send error also to javascript client
    return;      
  }
  if (!strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "ms") || !strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "us")) {
    Serial.printf ("[oscilloscope] samplingTimeUnit = %s\n", oscilloscopeSharedMemory.samplingTimeUnit);
    oscilloscopeSharedMemory.microSeconds = !strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "us");
    Serial.printf ("[oscilloscope] microSeconds = %i\n", oscilloscopeSharedMemory.microSeconds);
    if (oscilloscopeSharedMemory.microSeconds) oscilloscopeSharedMemory.microSecondCorrection = oscilloscopeSharedMemory.analog ? 12 : 2;   
    Serial.printf ("[oscilloscope] microSecondCorrection = %i\n", oscilloscopeSharedMemory.microSecondCorrection);
  } else {
    Serial.printf ("[oscilloscope] wrong samplingTimeUnit\n");
    webSocket->sendString ("Oscilloscope error. Sampling time unit can only be ms or us."); // send error also to javascript client
    return;    
  }
  if (oscilloscopeSharedMemory.screenWidthTime >= 4 * oscilloscopeSharedMemory.samplingTime  && oscilloscopeSharedMemory.screenWidthTime <= 1250000) {
     Serial.printf ("[oscilloscope] screenWidthTime = %i\n", oscilloscopeSharedMemory.screenWidthTime);
  } else {
    Serial.printf ("[oscilloscope] invalid screen width time\n");
    webSocket->sendString ("Oscilloscope error. Screen width time must be between 4 * sampling time and 125000."); // send error also to javascript client
    return;      
  }
  if (!strcmp (oscilloscopeSharedMemory.screenWidthTimeUnit, oscilloscopeSharedMemory.samplingTimeUnit)) {
    Serial.printf ("[oscilloscope] screenWidthTimeUnit = %s\n", oscilloscopeSharedMemory.screenWidthTimeUnit);
  } else {
    Serial.printf ("[oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit\n");
    webSocket->sendString ("Oscilloscope error. Screen width time unit must be the same as sampling time unit."); // send error also to javascript client
    return;    
  }
  if (oscilloscopeSharedMemory.screenRefreshTime >= 40 && oscilloscopeSharedMemory.screenRefreshTime <= 1000) {
     Serial.printf ("[oscilloscope] screenRefreshTime = %i\n", oscilloscopeSharedMemory.screenRefreshTime);
  } else {
    Serial.printf ("[oscilloscope] invalid screen refresh time\n");
    webSocket->sendString ("Oscilloscope error. Screen refresh time must be between 40 and 1000."); // send error also to javascript client
    return;      
  }
  if (!strcmp (oscilloscopeSharedMemory.screenRefreshTimeUnit, "ms")) {
    Serial.printf ("[oscilloscope] screenRefreshTimeUnit = %s\n", oscilloscopeSharedMemory.screenRefreshTimeUnit);
  } else {
    Serial.printf ("[oscilloscope] screenRefreshTimeUnit must be ms\n");
    webSocket->sendString ("Oscilloscope error. Screen refresh time unit must be ms."); // send error also to javascript client
    return;    
  }
  if (!strcmp (oscilloscopeSharedMemory.screenWidthTimeUnit, "ms")) 
    oscilloscopeSharedMemory.screenRefreshTimeCommonUnit =  oscilloscopeSharedMemory.screenRefreshTime;
  else
    oscilloscopeSharedMemory.screenRefreshTimeCommonUnit =  oscilloscopeSharedMemory.screenRefreshTime * 1000;
  oscilloscopeSharedMemory.screenRefreshModulus = oscilloscopeSharedMemory.screenRefreshTimeCommonUnit / oscilloscopeSharedMemory.screenWidthTime;
  if (oscilloscopeSharedMemory.screenRefreshModulus < 1) // if requested refresh time is less then screen width time then leave it as it is
    oscilloscopeSharedMemory.screenRefreshModulus = 1;
  else
    // if requested refresh time is equal or greater then screen width time then make it equal to screen width time and increase modulus 
    // for actual refresh if needed (this approach somehow simplyfies sampling code)
    oscilloscopeSharedMemory.screenRefreshTimeCommonUnit = oscilloscopeSharedMemory.screenWidthTime;
  Serial.printf ("[oscilloscope] screenRefreshModulus = %i\n", oscilloscopeSharedMemory.screenRefreshModulus);
  if (oscilloscopeSharedMemory.screenRefreshTimeCommonUnit > oscilloscopeSharedMemory.screenWidthTime && oscilloscopeSharedMemory.screenRefreshTimeCommonUnit != (long) oscilloscopeSharedMemory.screenRefreshModulus * (long) oscilloscopeSharedMemory.screenWidthTime) 
    Serial.printf ("[oscilloscope] it would be better if screen refresh time is multiple value of screen width time or equal to sampling time\n"); // just a suggestion
  Serial.printf ("[oscilloscope] screenRefreshTimeCommonUnit = %i (same time unit as screen width time)\n", oscilloscopeSharedMemory.screenRefreshTimeCommonUnit);
  oscilloscopeSharedMemory.samplingTime -= oscilloscopeSharedMemory.microSecondCorrection;
  if (oscilloscopeSharedMemory.samplingTime >= 1) {
     Serial.printf ("[oscilloscope] samplingTime (after correction) = %i\n", oscilloscopeSharedMemory.samplingTime);
  } else {
    Serial.printf ("[oscilloscope] invalid sampling time after correction\n");
    webSocket->sendString ("Oscilloscope error. Sampling time is too low according to other settings."); // send error also to javascript client
    return;      
  }
  if (oscilloscopeSharedMemory.positiveTrigger) {
    if (oscilloscopeSharedMemory.positiveTriggerTreshold > 0 && oscilloscopeSharedMemory.positiveTriggerTreshold <= (strcmp (oscilloscopeSharedMemory.readType, "analog") ? 1 : 4095)) {
      Serial.printf ("[oscilloscope] positive slope trigger treshold = %i\n", oscilloscopeSharedMemory.positiveTriggerTreshold);
    } else {
      Serial.printf ("[oscilloscope] invalid positive slope trigger treshold\n");
      webSocket->sendString ("Oscilloscope error. Positive slope trigger treshold is not valid (according to other settings)."); // send error also to javascript client
      return;      
    }
  }
  if (oscilloscopeSharedMemory.negativeTrigger) {
    if (oscilloscopeSharedMemory.negativeTriggerTreshold >= 0 && oscilloscopeSharedMemory.negativeTriggerTreshold < (strcmp (oscilloscopeSharedMemory.readType, "analog") ? 1 : 4095)) {
      Serial.printf ("[oscilloscope] negative slope trigger treshold = %i\n", oscilloscopeSharedMemory.negativeTriggerTreshold);
    } else {
      Serial.printf ("[oscilloscope] invalid negative slope trigger treshold\n");
      webSocket->sendString ("Oscilloscope error. Negative slope trigger treshold is not valid (according to other settings)."); // send error also to javascript client
      return;      
    }
  }

  #define tskNORMAL_PRIORITY 1
  if (pdPASS != xTaskCreate ( oscilloscopeSender, 
                              "oscilloscopeSender", 
                              4096, 
                              (void *) &oscilloscopeSharedMemory, // pass parameters to oscilloscopeSender
                              tskNORMAL_PRIORITY,
                              NULL)) {
    Serial.printf ("[oscilloscope] could not start oscilloscopeSender\n");
  } else {   
    oscilloscopeSharedMemory.senderIsRunning = true;
  }

  #define tskNORMAL_PRIORITY 1
  if (pdPASS != xTaskCreate ( oscilloscopeReader, 
                              "oscilloscopeReader", 
                              4096, 
                              (void *) &oscilloscopeSharedMemory, // pass parameters to oscilloscopeReader
                              tskNORMAL_PRIORITY,
                              NULL)) {
    Serial.printf ("[oscilloscope] could not start oscilloscopeReader\n");
  } else {
    oscilloscopeSharedMemory.readerIsRunning = true;  
  }

  while (oscilloscopeSharedMemory.senderIsRunning || oscilloscopeSharedMemory.readerIsRunning) SPIFFSsafeDelay (100); // check every 1/10 of secod

  return;
}
