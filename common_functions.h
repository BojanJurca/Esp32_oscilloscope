/*

    common_functions.h

    This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template

    It contins some useful functions used by other modules 
    
    September, 13, 2021, Bojan Jurca
    
 */


#ifndef __COMMON_FUNCTIONS__
  #define __COMMON_FUNCTIONS__


  // missing C function in Arduino, but we are going to need it
  char *stristr (char *haystack, char *needle) { 
    if (!haystack || !needle) return NULL; // nonsense
    int nCheckLimit = strlen (needle);                     
    int hCheckLimit = strlen (haystack) - nCheckLimit + 1;
    for (int i = 0; i < hCheckLimit; i++) {
      int j = i;
      int k = 0;
      while (*(needle + k)) {
          char nChar = *(needle + k ++); if (nChar >= 'a' && nChar <= 'z') nChar -= 32; // convert to upper case
          char hChar = *(haystack + j ++); if (hChar >= 'a' && hChar <= 'z') hChar -= 32; // convert to upper case
          if (nChar != hChar) break;
      }
      if (!*(needle + k)) return haystack + i; // match!
    }
    return NULL; // no match
  }


  // String parsing
  String between (String input, String openingBracket, String closingBracket) { // returns content inside of opening and closing brackets
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

  // pad string with spaces
  String pad (String s, int toLenght) {
    while (s.length () < toLenght) s += " ";
    return s;
  }

  // inet_ntos
  static SemaphoreHandle_t __ntosSemaphore__ = xSemaphoreCreateMutex (); // to prevent two threads to call inet_ntoa simultaneously
  
  String inet_ntos (ip_addr_t addr) { // equivalent of inet_ntoa
    char s [40];
    xSemaphoreTake (__ntosSemaphore__, portMAX_DELAY);
      strcpy (s, inet_ntoa (addr)); 
    xSemaphoreGive (__ntosSemaphore__);
    return String (s);
  }


#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0) //Checks if board manager version for esp32 is  >= 2.0.0
  String inet_ntos (esp_ip4_addr_t addr) { // equivalent of inet_ntoa
    char s [40];
    xSemaphoreTake (__ntosSemaphore__, portMAX_DELAY);
      strcpy (s, inet_ntoa (addr)); 
    xSemaphoreGive (__ntosSemaphore__);
    return String (s);
  }
#endif

  String inet_ntos (ip4_addr_t addr) { // equivalent of inet_ntoa
    char s [40];
    xSemaphoreTake (__ntosSemaphore__, portMAX_DELAY);
      strcpy (s, inet_ntoa (addr)); 
    xSemaphoreGive (__ntosSemaphore__);
    return String (s);
  }

  String inet_ntos (in_addr addr) { // equivalent of inet_ntoa
    char s [40];
    xSemaphoreTake (__ntosSemaphore__, portMAX_DELAY);
      strcpy (s, inet_ntoa (addr)); 
    xSemaphoreGive (__ntosSemaphore__);
    return String (s);
  }

  // converts binary MAC address into String
  String mac_ntos (byte *MacAddress, byte MacAddressLength) {
    String s = "";
    char c [3];
    for (byte i = 0; i < MacAddressLength; i++) {
      sprintf (c, "%02x", *(MacAddress ++));
      s += String (c);
      if (i < 5) s += ":";
    }
    return s;
  }


#endif
