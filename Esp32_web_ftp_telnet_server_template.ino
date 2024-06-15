/*
 
    Esp32_web_ftp_telnet_server_template.ino

        Compile this code with Arduino for:
            - one of ESP32 boards (Tools | Board) and 
            - one of FAT partition schemes (Tools | Partition scheme) for FAT file system or one of SPIFFS partition schemes for LittleFS file system

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Multitasking-Esp32-HTTP-FTP-Telnet-servers-for-Arduino
     
    May 22, 2024, Bojan Jurca


    PLEASE NOTE THAT THIS FILE IS JUST A TEMPLATE. YOU CAN INCLUDE OR EXCLUDE FUNCTIONALITIES YOU NEED OR DON'T NEED IN Esp32_servers_config.h.
    YOU CAN FREELY DELETE OR MODIFY ALL THE CODE YOU DON'T NEED. SOME OF THE CODE IS PUT HERE MERELY FOR DEMONSTRATION PURPOSES AND IT IS 
    MARKED AS SUCH. JUST DELETE OR MODIFY IT ACCORDING TO YOUR NEEDS.

*/

#include <WiFi.h>
// --- PLEASE MODIFY THIS FILE FIRST! --- This is where you can configure your network credentials, which servers will be included, etc ...
#include "Esp32_servers_config.h"


                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING DEFINITIONS -----
                    #include "measurements.hpp"
                    measurements<60> freeHeap;                  // measure free heap each minute for possible memory leaks
                    measurements<60> freeBlock;                 // measure max free heap block each minute for possible heap-related problems
                    measurements<60> httpRequestCount;          // measure how many web connections arrive each minute
                    #undef LED_BUILTIN
                    #define LED_BUILTIN 2                       // built-in led blinking is used in examples 01, 03 and 04

 
// HTTP request handler example - if you don't need to handle HTTP requests yourself just delete this function and pass NULL to httpSrv instead
// later in the code. httpRequestHandlerCallback function returns the content part of HTTP reply in HTML, json, ... format (header fields will be added by 
// httpServer before sending the HTTP reply back to the client) or empty string "". In this case, httpServer will try to handle the request
// completely by itself (like serving the right .html file, ...) or return an error code.  
// PLEASE NOTE: since httpServer is implemented as a multitasking server, httpRequestHandlerCallback function can run in different tasks at the same time
//              therefore httpRequestHandlerCallback function must be reentrant.

String httpRequestHandlerCallback (char *httpRequest, httpConnection *hcn) { 
    // httpServer will add HTTP header to the String that this callback function returns and then send everithing to the Web browser (this callback function is suppose to return only the content part of HTTP reply)

    #define httpRequestStartsWith(X) (strstr(httpRequest,X)==httpRequest)


                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE -----
                    httpRequestCount.increase_valueCounter ();                      // gether some statistics

                    // variables used by example 05
                    char niceSwitch1 [6] = "false";
                    static int niceSlider3 = 3;
                    char niceRadio5 [3] = "fm";

                    // ----- handle HTTP protocol requests -----
                        if (httpRequestStartsWith ("GET /example01.html "))       { // used by example 01: Dynamically generated HTML page
                                                                                      return "<HTML>Example 01 - dynamic HTML page<br><br><hr />" + String (digitalRead (LED_BUILTIN) ? "Led is on." : "Led is off.") + "<hr /></HTML>";
                                                                                  }
                    else if (httpRequestStartsWith ("GET /example07.html "))      { // used by example 07
                                                                                      cstring refreshCounter = hcn->getHttpRequestCookie ("refreshCounter");
                                                                                      if (refreshCounter == "") refreshCounter = "0";
                                                                                      refreshCounter = cstring (atoi (refreshCounter) + 1);
                                                                                      hcn->setHttpReplyCookie ("refreshCounter", refreshCounter, time () + 60);  // set 1 minute valid cookie that will be send to browser in HTTP reply
                                                                                      return "<HTML>Web cookies<br><br>This page has been refreshed " + String (refreshCounter) + " times. Click refresh to see more.</HTML>";
                                                                                  }                                                                                  
                    else if (httpRequestStartsWith ("GET /builtInLed "))          { // used by example 02, example 03, example 04, index.html: REST function for static HTML page
                                                                                      getBuiltInLed:
                                                                                          return "{\"id\":\"" + String (HOSTNAME) + "\",\"builtInLed\":\"" + String (digitalRead (LED_BUILTIN) ? "on" : "off") + "\"}\r\n";
                                                                                  }                                                                    
                    else if (httpRequestStartsWith ("PUT /builtInLed/on "))       { // used by example 03, example 04: REST function for static HTML page
                                                                                      digitalWrite (LED_BUILTIN, HIGH);
                                                                                      goto getBuiltInLed;
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /builtInLed/off "))      { // used by example 03, example 04, index.html: REST function for static HTML page
                                                                                      digitalWrite (LED_BUILTIN, LOW);
                                                                                      goto getBuiltInLed;
                                                                                  }
                    else if (httpRequestStartsWith ("GET /upTime "))              { // used by index.html: REST function for static HTML page
                                                                                      char httpResponseContentBuffer [100];
                                                                                      time_t t = getUptime ();       // t holds seconds
                                                                                      int seconds = t % 60; t /= 60; // t now holds minutes
                                                                                      int minutes = t % 60; t /= 60; // t now holds hours
                                                                                      int hours = t % 24;   t /= 24; // t now holds days
                                                                                      char c [25]; *c = 0;
                                                                                      if (t) sprintf (c, "%lu days, ", t);
                                                                                      sprintf (c + strlen (c), "%02i:%02i:%02i", hours, minutes, seconds);
                                                                                      sprintf (httpResponseContentBuffer, "{\"id\":\"%s\",\"upTime\":\"%s\"}", HOSTNAME, c);
                                                                                      return httpResponseContentBuffer;
                                                                                  }                                                                    
                    else if (httpRequestStartsWith ("GET /freeHeap "))            { // used by index.html: REST function for static HTML page
                                                                                      return freeHeap.toJson ();
                                                                                  }
                    else if (httpRequestStartsWith ("GET /freeBlock "))           { // used by index.html
                                                                                      return freeBlock.toJson ();
                                                                                  }
                    else if (httpRequestStartsWith ("GET /httpRequestCount "))    { // used by index.html: REST function for static HTML page
                                                                                      return httpRequestCount.toJson ();
                                                                                  }
                    else if (httpRequestStartsWith ("GET /niceSwitch1 "))         { // used by example 05.html: REST function for static HTML page
                                                                                      returnNiceSwitch1State:
                                                                                          return "{\"id\":\"niceSwitch1\",\"value\":\"" + String (niceSwitch1) + "\"}";
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceSwitch1/"))         { // used by example 05.html: REST function for static HTML page
                                                                                      *(httpRequest + 21) = 0;
                                                                                      strcpy (niceSwitch1, strstr (httpRequest + 17, "true") ? "true": "false");
                                                                                      goto returnNiceSwitch1State; // return success (or possible failure) back to the client
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceButton2/pressed ")) { // used by example 05.html: REST function for static HTML page
                                                                                      return "{\"id\":\"niceButton2\",\"value\":\"pressed\"}"; // the client will actually not use this return value at all but we must return something
                                                                                  }
                    else if (httpRequestStartsWith ("GET /niceSlider3 "))         { // used by example 05.html: REST function for static HTML page
                                                                                      returnNiceSlider3Value:
                                                                                          return "{\"id\":\"niceSlider3\",\"value\":\"" + String (niceSlider3) + "\"}";
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceSlider3/"))         { // used by example 05.html: REST function for static HTML page
                                                                                      niceSlider3 = atoi (httpRequest + 17);
                                                                                      Serial.printf ("[Got request from web browser for niceSlider3]: %i\n", niceSlider3);
                                                                                      goto returnNiceSlider3Value; // return success (or possible failure) back to the client
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceButton4/pressed ")) { // used by example 05.html: REST function for static HTML page
                                                                                      Serial.printf ("[Got request from web browser for niceButton4]: pressed\n");
                                                                                      return "{\"id\":\"niceButton4\",\"value\":\"pressed\"}"; // the client will actually not use this return value at all but we must return something
                                                                                  }
                    else if (httpRequestStartsWith ("GET /niceRadio5 "))          { // used by example 05.html: REST function for static HTML page
                                                                                      returnNiceRadio5Value:
                                                                                          return "{\"id\":\"niceRadio5\",\"modulation\":\"" + String (niceRadio5) + "\"}";
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceRadio5/"))          { // used by example 05.html: REST function for static HTML page
                                                                                      httpRequest [18] = 0;
                                                                                      Serial.println (httpRequest + 16);
                                                                                      strcpy (niceRadio5, strstr (httpRequest + 16, "am") ? "am" : "fm");
                                                                                      goto returnNiceRadio5Value; // return success (or possible failure) back to the client
                                                                                  }
                    else if (httpRequestStartsWith ("PUT /niceButton6/pressed ")) { // used by example 05.html: REST function for static HTML page
                                                                                      Serial.printf ("[Got request from web browser for niceButton6]: pressed\n");
                                                                                      return "{\"id\":\"niceButton6\",\"value\":\"pressed\"}"; // the client will actually not use this return value at all but we must return something
                                                                                  }

                    // ----- USED FOR DEMONSTRATION OF WEB SESSION HANDLING, YOU MAY FREELY DELETE THE FOLLOWING CODE -----
                    #ifdef WEB_SESSIONS
                        // ----- web sessions - this part is still public, the users do not need to login -----

                        Cstring<64> sessionToken = hcn->getHttpRequestCookie ("sessionToken").c_str ();
                        Cstring<64>userName;
                        if (sessionToken > "") userName = hcn->getUserNameFromToken (sessionToken); // if userName > "" the user is loggedin

                        // REST call to login       
                        if (httpRequestStartsWith ("GET /login/")) { // GET /login/userName%20password - called (for example) from login.html when "Login" button is pressed
                            // delete sessionToken from cookie and database if it already exists
                            if (sessionToken > "") {
                                hcn->deleteWebSessionToken (sessionToken);
                                hcn->setHttpReplyCookie ("sessionToken", "");
                                hcn->setHttpReplyCookie ("sessionUser", "");
                            }
                            // check new login credentials and login
                            Cstring<64>password;
                            if (sscanf (httpRequest, "%*[^/]/login/%64[^%%]%%20%64s", (char *) userName, (char *) password) == 2) {
                                if (userManagement.checkUserNameAndPassword (userName, password)) { // check if they are OK against system users or find another way of authenticating the user
                                    sessionToken = hcn->newWebSessionToken (userName, WEB_SESSION_TIME_OUT == 0 ? 0 : time () + WEB_SESSION_TIME_OUT).c_str (); // use 0 for infinite
                                    hcn->setHttpReplyCookie ("sessionToken", (char *) sessionToken, WEB_SESSION_TIME_OUT == 0 ? 0 : time () + WEB_SESSION_TIME_OUT); // TIME_OUT is in sec, use 0 for infinite
                                    hcn->setHttpReplyCookie ("sessionUser", (char *) userName, WEB_SESSION_TIME_OUT == 0 ? 0 : time () + WEB_SESSION_TIME_OUT); // TIME_OUT is in sec, use 0 for infinite
                                    return "loggedIn"; // notify the client about success
                                } else {
                                    return "Not logged in. Wrong username and/or password."; // notify the client login.html about failure
                                }
                            }
                        }

                        // REST call to logout
                        if (httpRequestStartsWith ("PUT /logout ")) {
                            // delete token from the cookie and the database
                            hcn->deleteWebSessionToken (sessionToken);
                            hcn->setHttpReplyCookie ("sessionToken", "");
                            hcn->setHttpReplyCookie ("sessionUser", "");
                            return "Logged out.";
                        }

                        // ----- web sessions - this part is portected for loggedin users only -----
                        if (httpRequestStartsWith ("GET /protectedPage.html ") || httpRequestStartsWith ("GET /logout.html ")) { 
                            // check if the user is logged in
                            if (userName > "") {
                                // prolong token validity, both in the cookie and in the database
                                if (WEB_SESSION_TIME_OUT > 0) {
                                    hcn->updateWebSessionToken (sessionToken, userName, time () + WEB_SESSION_TIME_OUT);
                                    hcn->setHttpReplyCookie ("sessionToken", (char *) sessionToken, time () + WEB_SESSION_TIME_OUT);
                                    hcn->setHttpReplyCookie ("sessionUser", (char *) userName, time () + WEB_SESSION_TIME_OUT);
                                }
                                return ""; // httpServer will search for protectedPage.html after this function returns
                            } else {
                                // redirect client to login.html
                                hcn->setHttpReplyHeaderField ("Location", "/login.html");
                                hcn->setHttpReplyStatus ("307 temporary redirect");
                                return "Not logged in.";
                            }
                        }

                    #endif      

    return ""; // httpRequestHandler did not handle the request - tell httpServer to handle it internally by returning "" reply
}


// Web socket request handler example - if you don't web sockets just delete this function and pass NULL to httpSrv instead
// later in the code. 
// PLEASE NOTE: since httpServer is implemented as a multitasking server, wsRequestHandlerCallback function can run in different tasks at the same time.
//              Therefore wsRequestHandlerCallback function must be reentrant.

void wsRequestHandlerCallback (char *wsRequest, WebSocket *webSocket) {
    // this callback function is supposed to handle WebSocket communication - once it returns WebSocket will be closed
  
    #define wsRequestStartsWith(X) (strstr(wsRequest,X)==wsRequest)
    
    #ifdef __OSCILLOSCOPE__
          if (wsRequestStartsWith ("GET /runOscilloscope"))      runOscilloscope (webSocket);      // used by oscilloscope.html
    #endif

                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE -----              
                    if (wsRequestStartsWith ("GET /rssiReader"))   { 
                                                                        unsigned long startMillis = millis ();
                                                                        char c;
                                                                        do {
                                                                            // if (millis () - startMillis >= 300000) {
                                                                            //    webSocket->sendString ("WebScket is automatically closed after 5 minutes for demonstration purpose.");
                                                                            // return;
                                                                            // }
                                                                            delay (100);
                                                                            int i = WiFi.RSSI ();
                                                                            c = (char) i;
                                                                            // Serial.printf ("[WebSocket data streaming] sending %i to web client\n", i);
                                                                        } while (webSocket->sendBinary ((byte *) &c, sizeof (c))); // keep sending RSSI information as long as web browser is receiving it
                                                                    }

}


// Telnet command handler example - if you don't need to telnet commands yourself just delete this function and pass NULL to telnetSrv instead
// later in the code. telnetCommandHandlerCallback function returns the command's output (reply) or empty string "". In this case, telnetServer 
// will try to handle the command completely by itself (serving already built-in commands, like ls, vi, ...) or return an error message.  
// PLEASE NOTE: since telnetServer is implemented as a multitasking server, telnetCommandHandlerCallback function can run in different tasks at the same time.
//              Therefore telnetCommandHandlerCallback function must be reentrant.

String telnetCommandHandlerCallback (int argc, char *argv [], telnetConnection *tcn) {
    // the String that this callback function returns will be written to Telnet client console as a response to Telnet command (already parsed to argv)

    #define argv0is(X) (argc > 0 && !strcmp (argv[0], X))  
    #define argv1is(X) (argc > 1 && !strcmp (argv[1], X))
    #define argv2is(X) (argc > 2 && !strcmp (argv[2], X))
    // add more #definitions if neeed 

                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE -----     
                            if (argv0is ("led") && argv1is ("state"))                 { // led state telnet command
                                                                                        return "Led is " + String (digitalRead (LED_BUILTIN) ? "on." : "off.");
                                                                                      } 
                    else if (argv0is ("turn") && argv1is ("led") && argv2is ("on"))   { // turn led on telnet command
                                                                                        digitalWrite (LED_BUILTIN, HIGH);
                                                                                        return "Led is on.";
                                                                                      }
                    else if (argv0is ("turn") && argv1is ("led") && argv2is ("off"))  { // turn led off telnet command
                                                                                        digitalWrite (LED_BUILTIN, LOW);
                                                                                        return "Led is off.";
                                                                                      }

                    // ----- USED FOR DEMONSTRATION OF RETRIEVING INFORMATION FROM WEB SESSION TOKEN DATABASE, YOU MAY FREELY DELETE THE FOLLOWING CODE -----
                    #ifdef WEB_SESSIONS
                        if (argv0is ("web") && argv1is ("sessions")) {  
                            signed char e; // or just int
                            webSessionTokenInformation_t webSessionTokenInformation;
                            tcn->sendTelnet ((char *) "web session token                                                valid for  user name\n\r-------------------------------------------------------------------------------------");
                            for (auto p: webSessionTokenDatabase) {
                                e = webSessionTokenDatabase.FindValue (p->key, &webSessionTokenInformation, p->blockOffset);
                                if (e == err_ok && time () && webSessionTokenInformation.expires > time ()) {
                                    char c [180];
                                    sprintf (c, "\n\r%s %5li s    %s", p->key.c_str (), webSessionTokenInformation.expires - time (), webSessionTokenInformation.userName.c_str ());
                                    tcn->sendTelnet (c);
                                }
                            }
                            return "\r"; // in fact nothing, but if the handler returns "", the Telnet server will try to handle the command internally, so just return a harmless \r
                        }
                    #endif

    return ""; // telnetCommand has not been handled by telnetCommandHandler - tell telnetServer to handle it internally by returning "" reply
}


                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE AND PASS NULL TO THE SERVERS LATER IN THE CODE INSTEAD -----               
                    bool firewallCallback (char *connectingIP) {            // firewall callback function, return true if IP is accepted or false if not - must be reentrant!
                      if (!strcmp (connectingIP, "10.0.0.2")) return false; // block 10.0.0.2 (for the purpose of this example) 
                      return true;                                          // ... but let every other client through
                    }


// Cron command handler example - if you don't need it just delete this function and pass NULL to startCoronDaemon function instead
// later in the code. 

void cronHandlerCallback (const char *cronCommand) {
    // this callback function is supposed to handle cron commands/events that occur at time specified in crontab

    #define cronCommandIs(X) (!strcmp (cronCommand, X))  

                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE -----
                    if (cronCommandIs ("ONCE_AN_HOUR"))                         {   // built-in cron event, triggers once an hour even if the time is not known
                                                                                    // check if ESP32 works in WIFI_STA mode
                                                                                    wifi_mode_t wifiMode = WIFI_OFF;
                                                                                    if (esp_wifi_get_mode (&wifiMode) != ESP_OK) {
                                                                                        #ifdef __DMESG__
                                                                                            dmesgQueue << "[cronHandlerCallback] couldn't get WiFi mode";
                                                                                        #endif
                                                                                        cout << "[cronHandlerCallback] couldn't get WiFi mode\n";
                                                                                    } else {
                                                                                        if (wifiMode & WIFI_STA) { // WiFi works in STAtion mode  
                                                                                            // is  STAtion mode is diconnected?
                                                                                            if (!WiFi.isConnected ()) { 
                                                                                                #ifdef __DMESG__
                                                                                                    dmesgQueue << "[cronHandlerCallback] STAtion disconnected, reconnecting to WiFi";
                                                                                                #endif
                                                                                                cout << "[cronHandlerCallback] STAtion disconnected, reconnecting to WiFi\n";
                                                                                                WiFi.reconnect (); 
                                                                                            } else { // check if it really works anyway 
                                                                                                esp32_ping routerPing;
                                                                                                routerPing.ping (WiFi.gatewayIP ());
                                                                                                for (int i = 0; i < 4; i++) {
                                                                                                    routerPing.ping (1);
                                                                                                    if (routerPing.received ())
                                                                                                        break;
                                                                                                    delay (1000);
                                                                                                }
                                                                                                if (!routerPing.received ()) {
                                                                                                    #ifdef __DMESG__
                                                                                                        dmesgQueue << "[cronHandlerCallback] ping of router failed, reconnecting WiFi STAtion";
                                                                                                    #endif
                                                                                                    cout << "[cronHandlerCallback] ping of router failed, reconnecting WiFi STAtion\n";
                                                                                                    WiFi.reconnect (); 
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                    else if (cronCommandIs ("gotTime"))                        {   // triggers only once - the first time ESP32 sets its clock (when it gets time from NTP server for example)
                                                                                    char buf [26]; // 26 bytes are needed
                                                                                    ascTime (localTime (time ()), buf);
                                                                                    Serial.println ("Got time at " + String (buf) + " (local time), do whatever needs to be done the first time the time is known.");
                                                                                }           
                    else if (cronCommandIs ("newYear'sGreetingsToProgrammer"))  {   // triggers at the beginning of each year
                                                                                    Serial.printf ("[%10lu] [cronDaemon] *** HAPPY NEW YEAR ***!\n", millis ());    
                                                                                }
                    else if (cronCommandIs ("onMinute"))                        {   // triggers each minute - collect some measurements for the purpose of this demonstration
                                                                                    struct tm st = localTime (time ()); 
                                                                                    freeHeap.push_back ( { (unsigned char) st.tm_min, (int) (ESP.getFreeHeap () / 1024) } ); // take s sample of free heap in KB 
                                                                                    freeHeap.push_back ( { (unsigned char) st.tm_min, (int) (heap_caps_get_largest_free_block (MALLOC_CAP_DEFAULT) / 1024) } ); // take s sample of free heap in KB 
                                                                                    httpRequestCount.push_back_and_reset_valueCounter (st.tm_min);          // take sample of number of web connections that arrived last minute 
                                                                                }
}


void setup () {
    Serial.begin (115200);
    Serial.println (cstring (MACHINETYPE " (") + cstring ((int) ESP.getCpuFreqMHz ()) + (char *) " MHz) " HOSTNAME " SDK: " + ESP.getSdkVersion () + (char *) " " VERSION_OF_SERVERS " compiled at: " __DATE__ " " __TIME__); 


    #ifdef FILE_SYSTEM
        // 1. Mount file system - this is the first thing to do since all the configuration files reside on the file system.
        #if (FILE_SYSTEM & FILE_SYSTEM_LITTLEFS) == FILE_SYSTEM_LITTLEFS
            // fileSystem.formatLittleFs (); Serial.printf ("\nFormatting file system with LittleFS ...\n\n"); // format flash disk to start everithing from the scratch
            fileSystem.mountLittleFs (true);                                            
        #endif
        #if (FILE_SYSTEM & FILE_SYSTEM_FAT) == FILE_SYSTEM_FAT
            // fileSystem.formatFAT (); Serial.printf ("\nFormatting file system with FAT ...\n\n"); // format flash disk to start everithing from the scratch
            fileSystem.mountFAT (true);
        #endif
        #if (FILE_SYSTEM & FILE_SYSTEM_SD_CARD) == FILE_SYSTEM_SD_CARD
            fileSystem.mountSD ("/SD", 5); // SD Card if attached, provide the mount point and CS pin
        #endif
    #endif


    // 2. Start cron daemon - it will synchronize internal ESP32's clock with NTP servers once a day and execute cron commands.
    // fileSystem.deleteFile ("/usr/share/zoneinfo");                         // contains timezone information           - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/ntp.conf");                               // contains ntp server names for time sync - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/crontab");                                // scontains cheduled tasks                - deleting this file would cause creating empty one
    startCronDaemon (cronHandlerCallback);


    // 3. Write the default user management files /etc/passwd and /etc/passwd it they don't exist yet (it only makes sense with UNIX_LIKE_USER_MANAGEMENT).
    // fileSystem.deleteFile ("/etc/passwd");                                 // contains users' accounts information    - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/passwd");                                 // contains users' passwords               - deleting this file would cause creating default one
    userManagement.initialize ();


    // 4. Start the WiFi (STAtion and/or A(ccess) P(oint), DHCP or static IP, depending on the configuration files.
    // fileSystem.deleteFile ("/network/interfaces");                         // contation STA(tion) configuration       - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/wpa_supplicant/wpa_supplicant.conf");     // contation STA(tion) credentials         - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/dhcpcd.conf");                            // contains A(ccess) P(oint) configuration - deleting this file would cause creating default one
    // fileSystem.deleteFile ("/etc/hostapd/hostapd.conf");                   // contains A(ccess) P(oint) credentials   - deleting this file would cause creating default one
    startWiFi ();   


    // 5. Start the servers.
    #ifdef __HTTP_SERVER__                                    // all the arguments are optional
        httpServer *httpSrv = new (std::nothrow) httpServer ( httpRequestHandlerCallback, // a callback function that will handle HTTP requests that are not handled by webServer itself
                                                              wsRequestHandlerCallback,   // a callback function that will handle WS requests, NULL to ignore WS requests
                                                              "0.0.0.0",                  // start HTTP server on all available IP addresses
                                                              80,                         // default HTTP port
                                                              NULL,                       // we won't use firewallCallback function for HTTP server FOR DEMONSTRATION PURPOSES
                                                              "/var/www/html");           // (optional) httpServer root directory, the default is "/var/www/html"
        if (!httpSrv && httpSrv->state () != httpServer::RUNNING) {
            #ifdef __DMESG__
                dmesgQueue << "[httpServer] did not start";
            #endif
            cout << "[httpServer] did not start\n";          
        }
    #endif
    #ifdef __FTP_SERVER__                                 // all the arguments are optional
        ftpServer *ftpSrv = new (std::nothrow) ftpServer ("0.0.0.0",          // start FTP server on all available ip addresses
                                                          21,                 // default FTP port
                                                          firewallCallback);  // let's use firewallCallback function for FTP server FOR DEMONSTRATION PURPOSES
        if (ftpSrv && ftpSrv->state () != ftpServer::RUNNING) {
            #ifdef __DMESG__
                dmesgQueue << "[ftpServer] did not start";
            #endif
            cout << "[ftpServer] did not start\n";                    
        }
    #endif
    #ifdef __TELNET_SERVER__                                        // all the arguments are optional
        telnetServer *telnetSrv = new (std::nothrow) telnetServer ( telnetCommandHandlerCallback, // a callback function that will handle Telnet commands that are not handled by telnetServer itself
                                                                    "0.0.0.0",                    // start Telnet server on all available ip addresses 
                                                                    23,                           // default Telnet port
                                                                    firewallCallback);            // let's use firewallCallback function for Telnet server FOR DEMONSTRATION PURPOSES
        if (telnetSrv && telnetSrv->state () != telnetServer::RUNNING) {
            #ifdef __DMESG__
                dmesgQueue << "[telnetServer] did not start";
            #endif
            cout << "[telnetServer] did not start\n";          
        }
    #endif


                    // ----- USED FOR DEMONSTRATION ONLY, YOU MAY FREELY DELETE THE FOLLOWING CODE -----
                    cronTabAdd ("* * * * * * gotTime");  // triggers only once - when ESP32 reads time from NTP servers for the first time
                    cronTabAdd ("0 0 0 1 1 * newYear'sGreetingsToProgrammer");  // triggers at the beginning of each year
                    cronTabAdd ("0 * * * * * onMinute");  // triggers each minute at 0 seconds
                    cronTabAdd ("0 0 * * * * onHour");  // triggers each hour at 0:0
                    //           | | | | | | |
                    //           | | | | | | |___ cron command, this information will be passed to cronHandlerCallback when the time comes
                    //           | | | | | |___ day of week (0 - 7 or *; Sunday=0 and also 7)
                    //           | | | | |___ month (1 - 12 or *)
                    //           | | | |___ day (1 - 31 or *)
                    //           | | |___ hour (0 - 23 or *)
                    //           | |___ minute (0 - 59 or *)
                    //           |___ second (0 - 59 or *)

                    pinMode (LED_BUILTIN, OUTPUT | INPUT);
                    digitalWrite (LED_BUILTIN, LOW);

}

void loop () {

}
