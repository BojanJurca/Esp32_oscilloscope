/*
 * 
 * common_functions.h
 * 
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *  
 *  It contins some useful functions commonly used by other modules 
 *  
 * History:
 *          - first release, 
 *            February 3, 2021, Bojan Jurca
 *  
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

#endif
