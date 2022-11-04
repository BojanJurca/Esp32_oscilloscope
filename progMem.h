#define oscilloscopeHtml "" \
                         "" \
"<!DOCTYPE html>\n" \
"<html lang='en'>\n" \
"<head>\n" \
"  <title>Esp32_oscilloscope</title>\n" \
"\n" \
"  <meta charset='UTF-8'>\n" \
"  <meta http-equiv='X-UA-Compatible' content='IE=edge'>\n" \
"\n" \
"  <link rel='shortcut icon' type='image/x-icon' sizes='64x64' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAMAAACdt4HsAAAABGdBTUEAALGPC/xhBQAAAAFzUkdCAK7OHOkAAAAJcEhZcwAADsIAAA7CARUoSoAAAAC6UExURRshaSgujUJN4CUtih4jbTU9tDY/uRkfYcLG9hogZURP5DU+tyMrhRUaVxcbXcnM+ycwjsXJ+dDU/8zQ/Q4UYTM8swIGU9TY/wkPXBIYZD5J3ENIij5J1Nfb/xYcZzdBvy02oNvf/5WZ0DpEyTQ7sCIqgTdAvIyRx6aq3yIsrXd7tsLG8pec3g4We1tgoDI91Le77mRs4SowdkhNlBcggWFnwy02sD1GzRwkoy03xXmB7KCm9lRd1HN2qbwDMlIAAAOKSURBVFjDzVfpeqIwFE0VC4hKAzEEVKq0Vlxwr2OXef/XGrKAAalm7Nf5Jn96ivceLzd3OQJNMxsNU+MHaP0xENi0qiAYDnM4phAUCDS9Vs8+15tV0LSs/NseXsyLBJVQDveBcgEA9G5XB+J0ai3zIpSMzbpF4d1d/eWlfidOKxjWL0LJuD58TCG4v7eenqx7cRr9R+silIytRwqBaXa63Y4pTrvW0i9CyVivWyn8fhJ/hACEIfgGASDhbheSzq0EyI8HjjOI28FtBMlyYBvpseeb1i0Eq4Vn8ANH+1CFoFgH+MM1sgMNpCvUQaG4jgvuDyF/i6NCJcrlnSwc5u8s1iwP7iK53gtpgzUavMFwxP1HS+zbLIZBzy93Y2acd+MpL7n/DmmhP+JpwOq3QCR/TfN7A/qP1wtVCfylJ/mnfDOaT3fmKxIAHnPur4U9GhCMkSIB5jfnbfbCFIQsIg+rEZAZS4Dze5WbEsbo7EIVAsRTZsf4ZEo+KIM9QVcIdL39/NxmLwDnpF0bd3R+2htWlQ5OofQ0Neao02k1UgiazWZ/9c5t34Pmr6CZnTeWBHdTa8pP+332J+hvPlcUsgiOEa1cO8KF71ot6FN3SSoiaPdGrhdNaQT0tY4sgwNUnGPB0hVJOMsBv3Tv1edJPLC3hTEpTdIWq4Q1OSdArMrsCGUE1NJZ+iWCxBWVUCYAPrs0J4ugcZy7hm2nk7hIsJ/QJDi9sExA2AcGnGZ10EWRE1P/SoJdmQBsRQCHvJB0RBA42yZj1g5pP5UISGyzqkmubaYWI0ivoUiAeqxABtv6VX3Q8RgBKeiDZ8TLNsbX9UHisXvcF2bidMIuZzBsXdcHU1Zg3lSeym8bXuGLqYI+wDG/R3kvHKAYlir6QBTchJye4jkf11ugog/CLU/CaUrgiCXAjYiawCBr+n3eNlvUYnBBiBUVCprwjhYEIR9cEPpAkcAXHc0nJfD55B2xZaFEAEQSGAHwDZvvO6QusjB957nDCOQEfqkPyuKQNy5cJ7opViec4wv64Eyedl/5vnidHuZcOozeLuiDCoGciMLZsQmbgk/ra31QJdHDLd+ZrvDvTb/WB9XyFPHaMYQ/+Xupm+kOVgDoFq2cM6T+t4ntlAEa0KX+N6p1/B4761no3y73gz0hrIF+4veCvPKLSqAanowlfdDPt7+sBKqhZBzk+uBbEfyfP7r+KcEfbrKUzOYiY9wAAAAASUVORK5CYII='>\n" \
"\n" \  
"  <style>\n" \
"      hr {border: 0; border-top: 1px solid lightgray; border-bottom: 1px solid lightgray}\n" \
"\n" \
"      h1 {font-family: verdana; font-size: 40px; text-align: center}\n" \
"\n" \
"      /* define grid for controls - we'll use d2 + d3 + d4 or d2 + d3 + d3 + d5 */\n" \
"      div.d1 {position: relative; width: 100%; height: 48px; font-family: verdana; font-size: 16px; color: red;}\n" \
"      div.d2 {position: relative; float: left; width: 48%; font-family: verdana; font-size: 30px; color: gray;}\n" \
"      div.d3 {position: relative; float: left; width: 13%; font-family: verdana; font-size: 30px; color: black;}\n" \
"      div.d4 {position: relative; float: left; width: 39%;}\n" \
"      div.d5 {position: relative; float: left; width: 26%;}\n" \
"\n" \
"      /* select control */\n" \
"      select {padding: 8px 24px; border-radius: 12px; font-size: 16px; color: black; background-color: #eee; -webkit-appearance: none; -moz-appearance: none; box-sizing: border-box; border: 3px solid #ccc; -webkit-transition: 0.5s; transition: 0.5s; outline: none}\n" \
"      select::-ms-expand {display: none}\n" \
"      select:disabled {background-color: #aaa}\n" \
"\n" \
"      /* radio button control */\n" \
"      .container {display: black; position: relative; padding-left: 222px; margin-bottom: 14px; cursor: pointer; font-family: verdana; font-size: 30px; color: gray; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none}\n" \
"      .container input {position: absolute; opacity: 0; cursor: pointer; height: 0; width: 0}\n" \
"      .checkmark {position: absolute; top: 0; left: 0; height: 34px; width: 34px; background-color: #ddd; border-radius: 50%}\n" \
"      .container:hover input ~ .checkmark {background-color: #ccc}\n" \
"      .container input:checked ~ .checkmark {background-color: #2196F3}\n" \
"      .checkmark:after {content: ''; position: absolute; display: none}\n" \
"      .container input:checked ~ .checkmark:after {display: block}\n" \
"      .container .checkmark:after {top: 9px; left: 9px; width: 15px; height: 15px; border-radius: 50%; background: white}\n" \
"      .container input:disabled ~ .checkmark {background-color: gray}\n" \
"      .container input:disabled ~ .checkmark {background-color: #aaa}\n" \
"\n" \
"      /* slider control */\n" \
"      input[type='range'] {-webkit-appearance: none; -webkit-tap-highlight-color: rgba(255,255,255,0); width: 230px; height: 28px; margin: 0; border: none; padding: 4px 4px; border-radius: 28px; background: #2196F3; outline: none}\n" \
"      input[type='range']::-moz-range-track {border: inherit; background: transparent}\n" \
"      input[type='range']::-ms-track {border: inherit; color: transparent; background: transparent}\n" \
"      input[type='range']::-ms-fill-lower\n" \
"      input[type='range']::-ms-fill-upper {background: transparent}\n" \
"      input[type='range']::-ms-tooltip {display: none}\n" \
"      input[type='range']::-webkit-slider-thumb {-webkit-appearance: none; width: 38px; height: 26px; border: none; border-radius: 13px; background-color: white}\n" \
"      input[type='range']::-moz-range-thumb {width: 38px; height: 26px; border: none; border-radius: 13px; background-color: white}\n" \
"      input[type='range']::-ms-thumb {width: 38px; height: 26px; border-radius: 13px; border: 0; background-color: white}\n" \
"      input:disabled+.slider {background-color: #aaa}\n" \
"      input[type='range']:disabled {background: #aaa}\n" \
"\n" \
"      /* switch control */\n" \
"      .switch {position: relative; display: inline-block; width: 60px; height: 34px}\n" \
"      .slider {position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; -webkit-transition: .4s; transition: .4s}\n" \
"      .slider:before {position: absolute; content: ''; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; -webkit-transition: .4s; transition: .4s}\n" \
"      input:checked+.slider {background-color: #2196F3}\n" \
"      input:focus+.slider {box-shadow: 0 0 1px #2196F3}\n" \
"      input:checked+.slider:before {-webkit-transform: translateX(26px); -ms-transform: translateX(26px); transform: translateX(26px)}\n" \
"      .switch input {display: none}\n" \
"      .slider.round {border-radius: 34px}\n" \
"      .slider.round:before {border-radius: 50%}\n" \
"      input:disabled+.slider {background-color: #aaa}\n" \
"\n" \
"      /* button control */\n" \
"      .button {padding: 10px 15px; font-size: 22px;text-align: center; cursor: pointer; outline: none; color: white; border: none; border-radius: 12px; box-shadow: 1px 1px #ccc; position: relative; top: 0px; height: 42px}\n" \
"      button:disabled {background-color: #aaa}\n" \
"      button:disabled:hover {background-color: #aaa}\n" \
"\n" \
"      /* blue button */\n" \
"      .button1 {background-color: #2196F3}\n" \
"      .button1:hover {background-color: #0961aa}\n" \
"      .button1:active {background-color: #0961aa; transform: translateY(3px)}\n" \
"\n" \
"      /* green button */\n" \
"      .button2 {background-color: green}\n" \
"      .button2:hover {background-color: darkgreen}\n" \
"      .button2:active {background-color: darkgreen; transform: translateY(3px)}\n" \
"\n" \
"      /* red button */\n" \
"      .button3 {background-color: red}\n" \
"      .button3:hover {background-color: darkred}\n" \
"      .button3:active {background-color: darkred; transform: translateY(3px)}\n" \
"\n" \
"    </style>\n" \
"\n" \
"    <script type='text/javascript'>\n" \
"\n" \
"      // DEFINE VALID DIGITAL INPUT GPIOs HERE OR LEAVE EMPTY STRING FOR ALL (note that validAnalogInputs will also prevent digitalReading of listed GPIOs)\n" \
"      let validDigitalInputs = '';\n" \
"\n" \
"      // DEFINE VALID ANALOG INPUT GPIOs HERE OR LEAVE EMPTY STRING FOR ALL\n" \
"      let validAnalogInputs = '36,37,38,39,32,33,34,35';\n" \
"\n" \
"      // from: https://www.w3schools.com/js/js_cookies.asp\n" \
"      function getCookie (cname) {\n" \
"        let name = cname + '=';\n" \
"        let decodedCookie = decodeURIComponent (document.cookie);\n" \
"        let ca = decodedCookie.split (';');\n" \
"        for (let i = 0; i < ca.length; i++) {\n" \
"          let c = ca [i];\n" \
"          while (c.charAt (0) == ' ') {\n" \
"            c = c.substring (1);\n" \
"          }\n" \
"          if (c.indexOf (name) == 0) {\n" \
"            return c.substring (name.length, c.length);\n" \
"          }\n" \
"        }\n" \
"        return '';\n" \
"      }\n" \
"      // from: https://www.w3schools.com/js/js_cookies.asp\n" \
"      function setCookie (cname, cvalue, exdays) {\n" \
"        var d = new Date ();\n" \
"        d.setTime (d.getTime () + (exdays * 24 * 60 * 60 * 1000));\n" \
"        var expires = 'expires=' + d.toUTCString ();\n" \
"        document.cookie = cname + '=' + cvalue + ';' + expires + ';' // path=/';  we wont use path - cookies are used in this page only\n" \
"      }\n" \
"    </script>\n" \
"\n" \
"  </head>\n" \
"\n" \
"  <body>\n" \
"\n" \
"    <br><h1>ESP32 oscilloscope</h1>\n" \
"\n" \
"    <div class='d1'>Please customize this file, oscilloscope.html, to suit your project needs.\n" \ 
"                    Initialize the validDigitalInputs and validAnalogInputs variables with the\n" \ 
"                    lists of GPIOs that are valid for your project for example.</div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1' style='height: 50px;'>\n" \
"      <div class='d2'>&nbsp;ESP32 GPIOs <small><small>(primary, secondary)</small></div>\n" \
"      <div class='d3' style='color: gray;'>\n" \
"        <select id='gpio1'>\n" \
"                <option value='1'>GPIO  1</option><option value='2' selected='selected'>GPIO  2</option><option value='3'>GPIO  3</option><option value='4'>GPIO  4</option><option value='5'>GPIO  5</option>\n" \
"          <option value='12'>GPIO 12</option><option value='13'>GPIO 13</option><option value='14'>GPIO 14</option><option value='15'>GPIO 15</option><option value='16'>GPIO 16</option>\n" \
"          <option value='17'>GPIO 17</option><option value='18'>GPIO 18</option><option value='19'>GPIO 19</option><option value='21'>GPIO 21</option><option value='22'>GPIO 22</option>\n" \
"          <option value='23'>GPIO 23</option><option value='25'>GPIO 25</option><option value='26'>GPIO 26</option><option value='27'>GPIO 27</option><option value='32'>GPIO 32</option>\n" \
"          <option value='33'>GPIO 33</option><option value='34'>GPIO 34</option><option value='35'>GPIO 35</option><option value='36'>GPIO 36</option><option value='39'>GPIO 39</option>\n" \
"        </select>\n" \
"      </div>\n" \
"      <div class='d3' style='color: gray;'>\n" \
"        <select id='gpio2'>\n" \
"                <option value='40' selected='selected'></option>\n" \
"          <option value='1'>GPIO  1</option><option value='2'>GPIO  2</option><option value='3'>GPIO  3</option><option value='4'>GPIO  4</option><option value='5'>GPIO  5</option>\n" \
"          <option value='12'>GPIO 12</option><option value='13'>GPIO 13</option><option value='14'>GPIO 14</option><option value='15'>GPIO 15</option><option value='16'>GPIO 16</option>\n" \
"          <option value='17'>GPIO 17</option><option value='18'>GPIO 18</option><option value='19'>GPIO 19</option><option value='21'>GPIO 21</option><option value='22'>GPIO 22</option>\n" \
"          <option value='23'>GPIO 23</option><option value='25'>GPIO 25</option><option value='26'>GPIO 26</option><option value='27'>GPIO 27</option><option value='32'>GPIO 32</option>\n" \
"          <option value='33'>GPIO 33</option><option value='34'>GPIO 34</option><option value='35'>GPIO 35</option><option value='36'>GPIO 36</option><option value='39'>GPIO 39</option>\n" \
"        </select>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;digitalRead (GPIO)</div>\n" \
"      <div class='d3'>\n" \
"        <label class='container'>&nbsp;<input type='radio' checked='checked' name='radio1' id='digital' onchange=\"\n" \
"          drawBackgroundAndCalculateParameters ();\n" \
"          document.getElementById ('sensitivity').disabled = true;\n" \
"          document.getElementById ('sensitivityLabel').style.color = 'gray';\n" \
"          document.getElementById ('position').disabled = true;\n" \
"          document.getElementById ('positionLabel').style.color = 'gray';\n" \
"          document.getElementById ('posTreshold').disabled = true;\n" \
"          document.getElementById ('posTriggerLabel').style.color = 'gray';\n" \
"          document.getElementById ('negTreshold').disabled = true;\n" \
"          document.getElementById ('negTriggerLabel').style.color = 'gray';\n" \
"        \"><span class='checkmark'></span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;analogRead (GPIO)</div>\n" \
"      <div class='d3'>\n" \
"        <label class='container'>&nbsp;<input type='radio' name='radio1' id='analog' onchange=\"\n" \
"          drawBackgroundAndCalculateParameters ();\n" \
"          document.getElementById ('sensitivity').disabled = false;\n" \
"          document.getElementById ('sensitivityLabel').style.color = 'black';\n" \
"          document.getElementById ('position').disabled = false;\n" \
"          document.getElementById ('positionLabel').style.color = 'black';\n" \
"          document.getElementById ('posTreshold').disabled = false;\n" \
"          document.getElementById ('posTriggerLabel').style.color = 'black';\n" \
"          document.getElementById ('negTreshold').disabled = false;\n" \
"          document.getElementById ('negTriggerLabel').style.color = 'black';\n" \
"        \"><span class='checkmark'></span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1' id='vSens'>\n" \
"      <div class='d2'>&nbsp;Vertical sensitivity</div>\n" \
"      <div class='d3' id='sensitivityLabel'>100 %</div>\n" \
"      <div class='d4'>\n" \ 
"        <input id='sensitivity' type='range' min='0' max='5' value='0' step='1' disabled onchange=\"\n" \
"          document.getElementById ('sensitivityLabel').textContent = sensitivityLabelFromSensitivitySlider (this.value);\n" \
"          drawBackgroundAndCalculateParameters ();\n" \
"        \" />\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class='d1' id='vPos'>\n" \
"      <div class='d2'>&nbsp;Vertical position</div>\n" \
"      <div class='d3' id='positionLabel'>0</div>\n" \
"      <div class='d4'>\n" \
"        <input id='position' type='range' min='0' max='4000' value='0' step='100' disabled onchange=\"\n" \
"          document.getElementById ('positionLabel').textContent = this.value;\n" \
"          drawBackgroundAndCalculateParameters ();\n" \
"        \" />\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Trigger on positive slope <small><small>(primary)</small></small></div>\n" \
"      <div class='d3'>\n" \
"        <label class='switch'><input type='checkbox' id='posTrigger'><div class='slider round'></div></label>\n" \
"      </div>\n" \
"      <div class='d3' id='posTriggerLabel'>on 1000</div>\n" \
"      <div class='d5'>\n" \
"        <input id='posTreshold' type='range' min='1' max='4095' value='1000' step='1' disabled onchange=\"\n" \
"          document.getElementById ('posTriggerLabel').textContent = 'on ' + this.value;\n" \
"          document.getElementById ('posTrigger').checked = true;\n" \
"        \" />\n" \
"      </div>\n" \
"    </div>\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Trigger on negative slope <small><small>(primary)</small></small></div>\n" \
"      <div class='d3'>\n" \
"        <label class='switch'><input type='checkbox' id='negTrigger'><div class='slider round'></div></label>\n" \
"      </div>\n" \
"      <div class='d3' id='negTriggerLabel'>on 3000</div>\n" \
"      <div class='d5'>\n" \ 
"        <input id='negTreshold' type='range' min='0' max='4094' value='3000' step='1' disabled onchange=\"\n" \
"          document.getElementById('negTriggerLabel').textContent = 'on ' + this.value;\n" \
"          document.getElementById('negTrigger').checked = true;\n" \
"        \" />\n" \
"      </div>\n" \
"\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Horizontal frequency</div>\n" \
"      <div class='d3' id='frequencyLabel'>1 KHz</div>\n" \
"      <div class='d4'>\n" \
"        <input id='frequency' type='range' min='1' max='17' value='14' step='1' onchange=\"\n" \
"          document.getElementById ('frequencyLabel').textContent = frequencyLabelFromFrequencySlider (this.value);\n" \
"          drawBackgroundAndCalculateParameters ();\n" \
"        \" />\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Connect samples with lines</div>\n" \
"      <div class='d3'>\n" \
"        <label class='switch'><input type='checkbox' checked id='lines' onclick=\"\n" \
"          if (!this.checked) document.getElementById ('markers').checked = true;\n" \
"        \"><div class='slider round'></div></label>\n" \
"      </div>\n" \
"    </div>\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Mark samples</div>\n" \
"      <div class='d3'>\n" \
"        <label class='switch'><input type='checkbox' checked id='markers' onclick=\"\n" \
"          if (!this.checked) document.getElementById ('lines').checked = true;\n" \
"        \"><div class='slider round'></div></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;Remember settings <small><small>(uses cookies)</small></small></div>\n" \
"      <div class='d3'>\n" \
"        <label class='switch'><input type='checkbox' id='remember' onclick=\"\n" \
"          saveSettings ();\n" \
"        \"><div class='slider round'></div></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <hr />\n" \
"    <div class='d1'>\n" \
"      <div class='d2'>&nbsp;</div>\n" \
"      <div class='d3'><button class='button button2' id='startButton' onclick=\"\n" \
"\n" \
"  if (document.getElementById ('analog').checked && validAnalogInputs != '' && (',' + validAnalogInputs + ',').indexOf (',' + document.getElementById ('gpio1').value + ',') == -1) {\n" \ 
"           alert ('Cannot analogRead GPIO ' + document.getElementById ('gpio1').value + '.');\n" \ 
"        } else if (document.getElementById ('analog').checked && document.getElementById ('gpio2').value != '40' && validAnalogInputs != '' && (',' + validAnalogInputs + ',').indexOf (',' + document.getElementById ('gpio2').value + ',') == -1) {\n" \ 
"           alert ('Cannot analogRead GPIO ' + document.getElementById ('gpio2').value + '.');\n" \ 
"        } else if (document.getElementById ('digital').checked && (validDigitalInputs != '' && (',' + validDigitalInputs + ',').indexOf (',' + document.getElementById ('gpio1').value + ',') == -1 || (',' + validAnalogInputs + ',').indexOf (',' + document.getElementById ('gpio1').value + ',') > -1)) {\n" \ 
"           alert ('Cannot digitalRead GPIO ' + document.getElementById ('gpio1').value + '.');\n" \ 
"        } else if (document.getElementById ('digital').checked && (document.getElementById ('gpio2').value != '40' && validDigitalInputs != '' && (',' + validDigitalInputs + ',').indexOf (',' + document.getElementById ('gpio2').value + ',') == -1 || (',' + validAnalogInputs + ',').indexOf (',' + document.getElementById ('gpio2').value + ',') > -1)) {\n" \ 
"           alert ('Cannot digitalRead GPIO ' + document.getElementById ('gpio2').value + '.');\n" \ 
"        } else {\n" \
"\n" \
"           saveSettings ();\n" \
"           drawBackgroundAndCalculateParameters ();\n" \
"           enableDisableControls (true);\n" \
"           startOscilloscope ();\n" \
"\n" \
"        }\n" \
"\n" \
"      \">&nbsp;START&nbsp;</button></div>\n" \
"      <div class='d4'><button class='button button3' id='stopButton' disabled onclick=\"\n" \
"        stopOscilloscope ();\n" \
"        enableDisableControls (false);\n" \
"      \">&nbsp;STOP&nbsp;</button></div>\n" \
"    </div>\n" \
"    <br>\n" \
"\n" \
"    <canvas id='oscilloscope' width='968' height='512'></canvas></div>\n" \
"\n" \
"    <script type='text/javascript'>\n" \
"\n" \
"      let lastWidth = 0;\n" \
"      let currentWidth = document.documentElement.clientWidth;\n" \
"      window.addEventListener ('resize', (e) => {\n" \
"        currentWidth = document.documentElement.clientWidth;\n" \
"        // if oscilloscope is not running we can resize and redraw canvas right now\n" \
"        if (document.getElementById ('stopButton').disabled == true) drawBackgroundAndCalculateParameters ();\n" \
"      });\n" \
"      // correct canvas size\n" \
"      function resizeCanvas () {\n" \
"        if (lastWidth != currentWidth) {\n" \
"          document.getElementById ('oscilloscope').width = document.getElementById ('vSens').clientWidth;\n" \
"          lastWidth = currentWidth;\n" \
"        }\n" \
"      }\n" \
"      resizeCanvas ();\n" \
"\n" \
"      // initialization\n" \
"      function sensitivityLabelFromSensitivitySlider (value) {\n" \
"        switch (value) {\n" \
"          case '0': return '100 %';\n" \
"          case '1': return '200 %';\n" \
"          case '2': return '400 %';\n" \
"          case '3': return '1000 %';\n" \
"          case '4': return '2000 %';\n" \
"          case '5': return '4000 %';\n" \
"        }\n" \
"      }\n" \
"      function frequencyLabelFromFrequencySlider (value) {\n" \
"        switch (value) {\n" \
"          case '1': return '0,1 Hz';\n" \
"          case '2': return '0,2 Hz';\n" \
"          case '3': return '0,5 Hz';\n" \
"          case '6': return '5 Hz';\n" \
"          case '7': return '10 Hz';\n" \
"          case '8': return '20 Hz';\n" \
"          case '9': return '50 Hz';\n" \
"          case '10':  return '60 Hz';\n" \
"          case '11':  return '100 Hz';\n" \
"          case '12':  return '200 Hz';\n" \
"          case '13':  return '500 Hz';\n" \
"          case '14':  return '1 KHz';\n" \
"          case '15':  return '2 KHz';\n" \
"          case '16':  return '5 KHz';\n" \
"          case '17':  return '10 KHz';\n" \
"        }\n" \
"      }\n" \
"\n" \
"      // console.log (document.cookie);\n" \
"\n" \
"      var v;\n" \
"      v = getCookie ('gpio1'); if (v != '') document.getElementById ('gpio1').value = v;\n" \
"      v = getCookie ('gpio2'); if (v != '') document.getElementById ('gpio2').value = v;\n" \
"      v = getCookie ('analog'); if (v == 'true') document.getElementById ('analog').checked = true; else document.getElementById ('digital').checked = true;\n" \
"      v = getCookie ('sensitivity'); if (v != '') { document.getElementById ('sensitivity').value = v; document.getElementById ('sensitivityLabel').textContent = sensitivityLabelFromSensitivitySlider (v);}\n" \
"      v = getCookie ('position'); if (v != '') { document.getElementById ('position').value = v; document.getElementById ('positionLabel').textContent = v;}\n" \
"      v = getCookie ('posTrigger'); if (v == 'true') document.getElementById ('posTrigger').checked = true;\n" \
"      v = getCookie ('posTreshold'); if (v != '') { document.getElementById ('posTreshold').value = v; document.getElementById ('posTriggerLabel').textContent = 'on ' + v;}\n" \
"      v = getCookie ('negTrigger'); if (v == 'true') document.getElementById ('negTrigger').checked = true;\n" \
"      v = getCookie ('negTreshold'); if (v != '') { document.getElementById ('negTreshold').value = v; document.getElementById ('negTriggerLabel').textContent = 'on ' + v;}\n" \
"      v = getCookie ('frequency'); if (v != '') { document.getElementById ('frequency').value = v; document.getElementById ('frequencyLabel').textContent = frequencyLabelFromFrequencySlider (v);}\n" \
"      v = getCookie ('lines'); if (v == 'false') document.getElementById ('lines').checked = false;\n" \
"      v = getCookie ('markers'); if (v == 'false') document.getElementById ('markers').checked = false;\n" \
"      v = getCookie ('remember'); if (v == 'true') document.getElementById ('remember').checked = true;\n" \
"\n" \
"      enableDisableControls (false);\n" \
"\n" \
"      // save settings into cookies\n" \
"      function saveSettings () {\n" \
"        if (document.getElementById ('remember').checked) {\n" \
"          // set cookies for 10 years\n" \
"          setCookie ('gpio1', document.getElementById ('gpio1').value, 3652);\n" \
"          setCookie ('gpio2', document.getElementById ('gpio2').value, 3652);\n" \
"          setCookie ('analog', document.getElementById ('analog').checked, 3652);\n" \
"          setCookie ('sensitivity', document.getElementById ('sensitivity').value, 3652);\n" \
"          setCookie ('position', document.getElementById ('position').value, 3652);\n" \
"          setCookie ('posTrigger', document.getElementById ('posTrigger').checked, 3652);\n" \
"          setCookie ('posTreshold', document.getElementById ('posTreshold').value, 3652);\n" \
"          setCookie ('negTrigger', document.getElementById ('negTrigger').checked, 3652);\n" \
"          setCookie ('negTreshold', document.getElementById ('negTreshold').value, 3652);\n" \
"          setCookie ('frequency', document.getElementById ('frequency').value, 3652);\n" \
"          setCookie ('lines', document.getElementById ('lines').checked, 3652);\n" \
"          setCookie ('markers', document.getElementById ('markers').checked, 3652);\n" \
"          setCookie ('remember', document.getElementById ('remember').checked, 3652);\n" \
"        } else {\n" \
"          // delete cookies\n" \
"          setCookie ('gpio1', '', -1);\n" \
"          setCookie ('gpio2', '', -1);\n" \
"          setCookie ('analog', '', -1);\n" \
"          setCookie ('sensitivity', '', -1);\n" \
"          setCookie ('position', '', -1);\n" \
"          setCookie ('posTrigger', '', -1);\n" \
"          setCookie ('posTreshold', '', -1);\n" \
"          setCookie ('negTrigger', '', -1);\n" \
"          setCookie ('negTreshold', '', -1);\n" \
"          setCookie ('frequency', '', -1);\n" \
"          setCookie ('lines', '', -1);\n" \
"          setCookie ('markers', '', -1);\n" \
"          setCookie ('remember', '', -1);\n" \
"        }\n" \
"      }\n" \
"\n" \
"      var webSocket = null;\n" \
"\n" \
"      function stopOscilloscope () {\n" \
"        if (webSocket != null) {\n" \
"          webSocket.send ('stop');\n" \
"          webSocket.close ();\n" \
"          webSocket = null;\n" \
"        }\n" \
"      }\n" \
"\n" \
"      function startOscilloscope () {\n" \
"        stopOscilloscope ();\n" \
"\n" \
"         if ('WebSocket' in window) {\n" \
"          // open a web socket\n" \
"          var ws = new WebSocket ('ws://' + self.location.host + '/runOscilloscope');\n" \
"          webSocket = ws;\n" \
"\n" \
"          ws.onopen = function () {\n" \
"            // first send endian identification Uint16 so ESP32 server will know the client architecture\n" \
"            endianArray = new Uint16Array (1); endianArray [0] = 0xAABB;\n" \
"            ws.send (endianArray);\n" \
"\n" \
"            // then send start command with sampling parameters\n" \
"            var startCommand = 'start ' + (document.getElementById ('analog').checked ? 'analog' : 'digital') + ' sampling on GPIO ' + document.getElementById ('gpio1').value + (document.getElementById ('gpio2').value == 40 ? '' : ', ' + document.getElementById ('gpio2').value) + ' every ';\n" \
"            switch (document.getElementById ('frequency').value) {\n" \
"\n" \
"              // real sampling times will be passed back to browser in 16 bit integers -\n" \
"              // take care that values are <= 32767 !\n" \
"\n" \
"              // 1 sample at a time mode, measurements in ms\n" \
"\n" \
"              case '1': // horizontal frequency = 0,1 Hz, screen width = 10 s, 40 samples per screen, 250 ms per sample, server refreshes every sample\n" \
"                  startCommand += '250 ms screen width = 10000 ms'; // sampling: 4 Hz, horizontal frequency: 0,1 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '2': // horizontal frequency = 0,2 Hz, whole width = 5 s, 40 samples per screen, 125 ms per sample, server refreshes every sample\n" \
"                  startCommand += '125 ms screen width = 5000 ms'; // sampling: 8 Hz, horizontal frequency:0,2 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '3': // horizontal frequency = 0,5 Hz, whole width = 2 s, 40 samples per screen, 50 ms per sample, server refreshes every sample\n" \
"                  startCommand += '50 ms screen width = 2000 ms'; // sampling: 20 Hz, horizontal frequency:0,5 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"\n" \
"              // screen at a time mode from now on: 40 samples per packet, measurements in ms\n" \
"\n" \
"              case '4': // horizontal frequency = 1 Hz, whole width = 1 s, 40 samples per screen, 25 ms per sample, server refreshes every 1 s, all 40 samples will be displyed at once\n" \
"                  startCommand += '25 ms screen width = 1000 ms'; // sampling: 40 Hz, horizontal frequency: 1 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"\n" \
"              // switch to measurements in us for better accuracy with shorter time intervals\n" \
"\n" \
"              case '5': // horizontal frequency = 2 Hz, whole width = 0,5 s, 40 samples per screen, 12.500 us per sample, server refreshes every 0,5 s, all 40 samples will be displyed at once\n" \
"                  startCommand += '12500 us screen width = 500000 us'; // sampling: 80 Hz, horizontal frequency: 2 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '6': // horizontal frequency = 5 Hz, whole width = 0,2 s, 40 samples per screen, 5.000 us per sample, server refreshes every 0,2 s, all 40 samples will be displyed at once\n" \
"                  startCommand += '5000 us screen width = 200000 us'; // sampling: 200 Hz, horizontal frequency: 5 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '7': // horizontal frequency = 10 Hz, whole width = 0,1 s, 40 samples per screen, 2.500 us per sample, server refreshes every 0,1 s, all 40 samples will be displyed at once\n" \
"                  startCommand += '2500 us screen width = 100000 us'; // sampling: 400 Hz, horizontal frequency: 10 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '8': // horizontal frequency = 20 Hz, whole width = 0,05 s, 40 samples per screen, 1.250 us per sample, server refreshes every 0,05 s, all 40 samples will be displyed at once\n" \
"                  startCommand += '1250 us screen width = 50000 us'; // sampling: 800 Hz, horizontal frequency: 20 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '9': // horizontal frequency = 50 Hz, whole width = 20 ms, 40 samples per screen, 500 us per sample, server refreshes every 40 ms (20 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '500 us screen width = 20000 us';  // sampling: 2 KHz, horizontal frequency: 50 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '10':  // horizontal frequency = 60 Hz, whole width = 16,667 ms, 40 samples per screen, 417 us per sample, server refreshes every 34 ms (16,667 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '425 us screen width = 17000 us'; // sampling: 2,4 KHz, horizontal frequency: 60 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '11':  // horizontal frequency = 100 Hz, whole width = 10 ms, 40 samples per screen, 250 us per sample, server refreshes every 50 ms (10 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '250 us screen width = 10000 us'; // sampling: 4 KHz, horizontal frequency: 100 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '12':  // horizontal frequency = 200 Hz, whole width = 5 ms, 40 samples per screen, 125 us per sample, server refreshes every 50 ms (5 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '125 us screen width = 5000 us'; // sampling: 8 KHz, horizontal frequency: 200 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '13':  // horizontal frequency = 500 Hz, whole width = 2 ms, 40 samples per screen, 50 us per sample, server refreshes every 50 ms (2 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '50 us screen width = 2000 us'; // sampling: 20 KHz, horizontal frequency: 500 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '14':  // horizontal frequency = 1000 Hz, whole width = 1 ms, 40 samples per screen, 25 us per sample, server refreshes every 50 ms (1 would be too fast), all 40 samples will be displyed at once\n" \
"                  startCommand += '25 us screen width = 1000 us'; // sampling: 40 KHz, horizontal frequency: 1 KHz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '15':  // horizontal frequency = 2000 Hz, whole width = 500 us, 40 samples per screen, 12 us per sample, server refreshes every 50 ms (500 us would be too fast), all (less then, server can not keep up) 40) samples will be displyed at once\n" \
"                  startCommand += '12 us screen width = 500 us every 50 ms'; // sampling: 80 KHz, horizontal frequency: 50 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '16':  // horizontal frequency = 5000 Hz, whole width = 200 us, 40 samples per screen, 5 us per sample, server refreshes every 50 ms (200 us would be too fast), all (less then, server can not keep up) 40) samples will be displyed at once\n" \
"                  startCommand += '5 us screen width = 200 us'; // sampling: 200 KHz, horizontal frequency: 50 Hz, max 40 samples per screen\n" \
"                  break;\n" \
"              case '17':  // horizontal frequency = 10000 Hz, whole width = 100 us, 33 samples per screen, 3 us per sample, server refreshes every 50 ms (100 us would be too fast), all (less then, server can not keep up) 40) samples will be displyed at once\n" \
"                  startCommand += '3 us screen width = 100 us'; // sampling: 300 KHz, horizontal frequency: 50 Hz, max 33 samples per screen\n" \
"                  break;\n" \
"\n" \
"            }\n" \
"            if (document.getElementById ('posTrigger').checked) startCommand += ' set positive slope trigger to ' + (document.getElementById ('analog').checked ? document.getElementById ('posTreshold').value : 1);\n" \
"            if (document.getElementById ('negTrigger').checked) startCommand += ' set negative slope trigger to ' + (document.getElementById ('analog').checked ? document.getElementById ('negTreshold').value : 0);\n" \
"\n" \
"            ws.send (startCommand);\n" \
"          };\n" \
"\n" \
"          ws.onmessage = function (evt) {\n" \
"            if (typeof (evt.data) === 'string' || evt.data instanceof String) { // UTF-8 formatted string - error message from ESP32 server\n" \
"              alert ('Error message from server: ' + evt.data); // oscilloscope code reporting error (synatx error, ...)\n" \
"              enableDisableControls (false);\n" \
"            }\n" \
"            if (evt.data instanceof Blob) { // binary data - array of samples\n" \
"              // receive binary data as blob and then convert it into array buffer and draw oscilloscope signal\n" \
"              var myInt16Array = null;\n" \
"              var myArrayBuffer = null;\n" \
"              var myFileReader = new FileReader ();\n" \
"              myFileReader.onload = function (event) {\n" \
"                myArrayBuffer = event.target.result;\n" \
"                myInt16Array = new Int16Array (myArrayBuffer);\n" \
"                // console.log (myInt16Array);\n" \
"                drawSignal (myInt16Array, 0, myInt16Array.length - 1);\n" \
"              };\n" \
"              myFileReader.readAsArrayBuffer (evt.data);\n" \
"            }\n" \
"          };\n" \
"\n" \
"          ws.onclose = function () {\n" \
"            // websocket is closed.\n" \
"            console.log ('WebSocket closed.');\n" \
"            enableDisableControls (false);\n" \
"          };\n" \
"\n" \
"          ws.onerror = function (event) {\n" \
"            ws.close ();\n" \
"            alert ('WebSocket error: ' + event);\n" \
"            enableDisableControls (false);\n" \
"          };\n" \
"\n" \
"        } else {\n" \
"          alert ('WebSockets are not supported by your browser.');\n" \
"        }\n" \
"      }\n" \
"\n" \
"      // drawing parameters\n" \
"\n" \
"      var screenWidthTime;    // oscilloscope screen width in time units\n" \
"      var restartDrawingSignal; // used for drawing the signal\n" \
"      var screenTimeOffset;   // used for drawing the signal\n" \
"\n" \
"      var xOffset;\n" \
"      var xScale;\n" \
"      var yOffset;\n" \
"      var yScale;\n" \
"      var yLast;\n" \
"\n" \
"      function drawBackgroundAndCalculateParameters () {\n" \
"\n" \
"        resizeCanvas ();\n" \
"\n" \
"        var x;    // x coordinate\n" \
"        var y;    // y coordinate\n" \
"        var i;    // x coordinate in canvas units\n" \
"        var j;    // y coordinate in canas units\n" \
"        var yGridTick;\n" \
"        var gridTop;  // y value at the top of the grid\n" \
"\n" \
"        restartDrawingSignal = true;  // drawing parameter - for later use\n" \
"        screenTimeOffset = 0;   // drawing parameter - for later use\n" \
"\n" \
"        var canvas = document.getElementById ('oscilloscope');\n" \
"        var ctx = canvas.getContext ('2d');\n" \
"\n" \
"        // colour background\n" \
"        ctx.fillStyle = '#031c30';\n" \
"        ctx.beginPath ();\n" \
"        ctx.moveTo (0, 0);\n" \
"        ctx.lineTo (canvas.width - 1, 0);\n" \
"        ctx.lineTo (canvas.width - 1, canvas.height - 1);\n" \
"        ctx.lineTo (0, canvas.height - 1);\n" \
"        ctx.fill ();\n" \
"\n" \
"        // calculate drawing parametes and draw grid and scale\n" \
"        ctx.strokeStyle = '#2196F3';\n" \
"        ctx.lineWidth = 3;\n" \
"        ctx.font = '16px Verdana';\n" \
"\n" \
"        xOffset = 50;\n" \
"\n" \
"        // draw analog signal grid\n" \
"\n" \
"        if (document.getElementById ('analog').checked) {\n" \
"\n" \
"          // signal sensitivity\n" \
"\n" \
"          yScale = -(canvas.height - 60) / 4096;\n" \
"          yGridTick = 1000;\n" \
"\n" \
"          switch (document.getElementById ('sensitivity').value) {\n" \
"            case '0': yScale *= 1; break;\n" \
"            case '1': yScale *= 2; break;\n" \
"            case '2': yScale *= 4; yGridTick = 200; break;\n" \
"            case '3': yScale *= 8; yGridTick = 200; break;\n" \
"            case '4': yScale *= 10; yGridTick = 100; break;\n" \
"            case '5': yScale *= 20; yGridTick = 100; break;\n" \
"          }\n" \
"\n" \
"          yOffset = canvas.height - 50 - parseInt (document.getElementById ('position').value) * yScale;\n" \
"          gridTop = yOffset + yScale * 4095 + 5;\n" \
"\n" \
"          // draw horizontal grid and scale\n" \
"\n" \
"          for (y = 0; y < 4096; y += yGridTick) {\n" \
"            j = yOffset + yScale * y;\n" \
"            ctx.strokeText (y.toString (), 5, j + 5);\n" \
"            ctx.beginPath ();\n" \
"            ctx.moveTo (xOffset - 5, j);\n" \
"            ctx.lineTo (canvas.width, j);\n" \
"            ctx.stroke ();\n" \
"          }\n" \
"\n" \
"        // draw digital signal grid\n" \
"\n" \
"        } else {\n" \
"          yOffset = canvas.height - 100;\n" \
"          yScale = -(canvas.height - 200);\n" \
"          for (y = 0; y <= 1; y += 0.333333) {\n" \
"            j = yOffset + yScale * y;\n" \
"            ctx.beginPath ();\n" \
"            ctx.moveTo (xOffset - 5, j);\n" \
"            ctx.lineTo (canvas.width, j);\n" \
"            ctx.stroke ();\n" \
"          }\n" \
"          ctx.strokeText ('0', 5, yOffset + 5);\n" \
"          ctx.strokeText ('1', 5, yOffset + yScale * 1 + 5);\n" \
"          gridTop = yOffset + yScale * 1 - 25;\n" \
"        }\n" \
"\n" \
"        // draw vertical grid and scale\n" \
"\n" \
"        switch (document.getElementById ('frequency').value) {\n" \
"\n" \
"          case '1': screenWidthTime = 10000; // (ms) horizontal frequency = 0,1 Hz, whole width = 10 s, grid tick width = 1 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '1 s';\n" \
"              break;\n" \
"          case '2': screenWidthTime = 5000; // (ms) horizontal frequency = 0,2 Hz, whole width = 5 s, grid tick width = 0,5 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '500 ms';\n" \
"              break;\n" \
"          case '3': screenWidthTime = 2000; // (ms) horizontal frequency = 0,5 Hz, whole width = 2 s, grid tick width = 0,2 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '200 ms';\n" \
"              break;\n" \
"          case '4': screenWidthTime = 1000; // (ms) horizontal frequency = 1 Hz, whole width = 1 s, grid tick width = 0,1 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '100 ms';\n" \
"              break;\n" \
"          case '5': screenWidthTime = 500000; // (us) horizontal frequency = 2 Hz, whole width = 0,5 s, grid tick width = 0,05 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '50 ms';\n" \
"              break;\n" \
"          case '6': screenWidthTime = 200000; // (us) horizontal frequency = 5 Hz, whole width = 0,2 s, grid tick width = 0,02 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '20 ms';\n" \
"              break;\n" \
"          case '7': screenWidthTime = 100000; // (us) horizontal frequency = 10 Hz, whole width = 0,1 s, grid tick width = 0,01 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '10 ms';\n" \
"              break;\n" \
"          case '8': screenWidthTime = 50000; // (us) horizontal frequency = 20 Hz, whole width = 0,05 s, grid tick width = 0,005 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '5 ms';\n" \
"              break;\n" \
"          case '9': screenWidthTime = 20000; // horizontal frequency = 50 Hz, whole width = 0,02 s, grid tick width = 0,002 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '2 ms';\n" \
"              break;\n" \
"          case '10':  screenWidthTime = 17000; // horizontal frequency = 60 Hz, whole width = 0,017 s, grid tick width = 0,002 s\n" \
"              xGridTick = 2000;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '2 ms';\n" \
"              break;\n" \
"          case '11':  screenWidthTime = 10000; // horizontal frequency = 100 Hz, whole width = 0,01 s, grid tick width = 0,001 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '1 ms';\n" \
"              break;\n" \
"          case '12':  screenWidthTime = 5000; // horizontal frequency = 200 Hz, whole width = 0,005 s, grid tick width = 0,0005 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '500 us';\n" \
"              break;\n" \
"          case '13':  screenWidthTime = 2000; // horizontal frequency = 500 Hz, whole width = 0,002 s, grid tick width = 0,0002 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '200 us';\n" \
"              break;\n" \
"          case '14':  screenWidthTime = 1000; // horizontal frequency = 1000 Hz, whole width = 0,001 s, grid tick width = 0,0001 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '100 us';\n" \
"              break;\n" \
"          case '15':  screenWidthTime = 500; // horizontal frequency = 2000 Hz, whole width = 0,00005 s, grid tick width = 0,00005 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '50 us';\n" \
"              break;\n" \
"          case '16':  screenWidthTime = 200; // horizontal frequency = 5000 Hz, whole width = 0,00002 s, grid tick width = 0,00002 s\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '20 us';\n" \
"              break;\n" \
"          case '17':  screenWidthTime = 100; // horizontal frequency = 10000 Hz, whole width = 0,0001 s (100 us), grid tick width = 0,0001 s (10 us)\n" \
"              xGridTick = screenWidthTime / 10;\n" \
"              xScale = (canvas.width - xOffset) / screenWidthTime;\n" \
"              xLabel = '10 us';\n" \
"              break;\n" \
"        }\n" \
"\n" \
"        for (x = 0; x < screenWidthTime; x += xGridTick) {\n" \
"          i = xOffset + xScale * x;\n" \
"          ctx.strokeText (xLabel, i + 25, yOffset + 25);\n" \
"          ctx.beginPath ();\n" \
"          ctx.moveTo (i, yOffset + 5);\n" \
"          ctx.lineTo (i, gridTop);\n" \
"          ctx.stroke ();\n" \
"        }\n" \
"      }\n" \
"\n" \
"      drawBackgroundAndCalculateParameters ();\n" \
"\n" \
"      var lastI;  // last drawn sample (time)\n" \
"      var lastJ1; // signal 1\n" \
"      var lastJ2; // signal 2\n" \
"\n" \
"      function drawSignal (myInt16Array, startInd, endInd) {\n" \
"        if (startInd > endInd) return;\n" \
"\n" \
"        // find dummy sample (the one with value of -1) which will tells javascript cliento to start drawing from beginning of the screen\n" \
"        for (var ind = startInd; ind <= endInd; ind += 3) {\n" \
"          if (myInt16Array [ind] == -1) { // if signal value = -1 (dummy value)\n" \
"            drawSignal (myInt16Array, startInd, ind - 3); // upt to peviuos sample, there are 3 16 bit words in each sample\n" \
"            drawBackgroundAndCalculateParameters ();\n" \
"            drawSignal (myInt16Array, ind + 3, endInd); // from next sample on, there are 3 16 bit words in each sample\n" \
"            return;\n" \
"          }\n" \
"        }\n" \
"\n" \
"        var canvas = document.getElementById ('oscilloscope');\n" \
"        var ctx = canvas.getContext ('2d');\n" \
"\n" \
"        var analog = document.getElementById ('analog').checked;\n" \
"        var lines = document.getElementById ('lines').checked;\n" \
"        var markers = document.getElementById ('markers').checked;\n" \
"\n" \
"        ctx.lineWidth = 5;\n" \
"\n" \
"        for (var ind = startInd; ind < endInd; ind += 3) { // there are 3 16 bit words in each sample\n" \
"\n" \
"          // calculate sample position\n" \
"\n" \
"          screenTimeOffset += myInt16Array [ind + 2];\n" \
"          i = xOffset + xScale * screenTimeOffset;   // time\n" \
"          j1 = yOffset + yScale * myInt16Array [ind]; // signal 1\n" \
"          j2 = myInt16Array [ind + 1] >=0 ? yOffset + yScale * myInt16Array [ind + 1] : -1; // signal 2\n" \
"\n" \
"          // lines\n" \
"          if (lines) {\n" \
"            if (restartDrawingSignal) {\n" \
"              restartDrawingSignal = false;\n" \
"            } else {\n" \
"              if (analog) { // analog\n" \
"                // signal 2\n" \
"                if (j2 >= 0) {\n" \
"                  ctx.strokeStyle = '#ff8000';\n" \
"                  ctx.beginPath ();\n" \
"                  ctx.moveTo (lastI, lastJ2);\n" \
"                  ctx.lineTo (i, j2);\n" \
"                  ctx.stroke ();\n" \
"                }\n" \
"                // signal 1\n" \
"                ctx.strokeStyle = '#ffbf80';\n" \
"                ctx.beginPath ();\n" \
"                ctx.moveTo (lastI, lastJ1);\n" \
"                ctx.lineTo (i, j1);\n" \
"                ctx.stroke ();\n" \
"              } else { // digital\n" \
"                // signal 2\n" \
"                if (j2 >= 0) {\n" \
"                  ctx.strokeStyle = '#ff8000';\n" \
"                  ctx.beginPath ();\n" \
"                  ctx.moveTo (lastI, lastJ2);\n" \
"                  ctx.lineTo (i, lastJ2);\n" \
"                  ctx.lineTo (i, j2);\n" \
"                  ctx.stroke ();\n" \
"                }\n" \
"                // signal 1\n" \
"                ctx.strokeStyle = '#ffbf80';\n" \
"                ctx.beginPath ();\n" \
"                ctx.moveTo (lastI, lastJ1);\n" \
"                ctx.lineTo (i, lastJ1);\n" \
"                ctx.lineTo (i, j1);\n" \
"                ctx.stroke ();\n" \
"              }\n" \
"            }\n" \
"\n" \
"          }\n" \
"\n" \
"          // markers\n" \
"          if (markers) {\n" \
"            // signal 2\n" \
"            if (j2 >= 0) {\n" \
"              ctx.strokeStyle = '#ff8000';\n" \
"              ctx.beginPath ();\n" \
"              ctx.arc (i, j2, 5, 0, 2 * Math.PI, false);\n" \
"              ctx.stroke ();\n" \
"            }\n" \
"            // signal 1\n" \
"            ctx.strokeStyle = '#ffbf80';\n" \
"            ctx.beginPath ();\n" \
"            ctx.arc (i, j1, 5, 0, 2 * Math.PI, false);\n" \
"            ctx.stroke ();\n" \
"          }\n" \
"\n" \
"          lastI = i;\n" \
"          lastJ1 = j1;\n" \
"          lastJ2 = j2;\n" \
"        }\n" \
"\n" \
"      }\n" \
"\n" \
"      // eneable and disable controls\n" \
"\n" \
"      function enableDisableControls (workMode) {\n" \
"        if (workMode) {\n" \
"          // disable GPIO, analog/digital, trigger, frequency and start, enable stop\n" \
"          document.getElementById ('gpio1').disabled = true;\n" \
"          document.getElementById ('gpio2').disabled = true;\n" \
"          document.getElementById ('analog').disabled = true;\n" \
"          document.getElementById ('digital').disabled = true;\n" \
"          document.getElementById ('posTrigger').disabled = true;\n" \
"          document.getElementById ('posTreshold').disabled = true;\n" \
"          document.getElementById ('posTriggerLabel').style.color = 'gray';\n" \
"          document.getElementById ('negTrigger').disabled = true;\n" \
"          document.getElementById ('negTreshold').disabled = true;\n" \
"          document.getElementById ('negTriggerLabel').style.color = 'gray';\n" \
"          document.getElementById ('frequency').disabled = true;\n" \
"          document.getElementById ('frequencyLabel').style.color = 'gray';\n" \
"          document.getElementById ('startButton').disabled = true;\n" \
"          document.getElementById ('stopButton').disabled = false;\n" \
"        } else {\n" \
"          // enable GPIO, analog/digital, trigger, frequency and start, disable stop\n" \
"          document.getElementById ('gpio1').disabled = false;\n" \
"          document.getElementById ('gpio2').disabled = false;\n" \
"          document.getElementById ('analog').disabled = false;\n" \
"          document.getElementById ('digital').disabled = false;\n" \
"          document.getElementById ('posTrigger').disabled = false;\n" \
"          document.getElementById ('negTrigger').disabled = false;\n" \
"          document.getElementById ('frequency').disabled = false;\n" \
"          document.getElementById ('frequencyLabel').style.color = 'black';\n" \
"          document.getElementById ('startButton').disabled = false;\n" \
"          document.getElementById ('stopButton').disabled = true;\n" \
"          if (document.getElementById ('analog').checked) {\n" \
"            document.getElementById ('sensitivity').disabled = false;\n" \
"            document.getElementById ('sensitivityLabel').style.color = 'black';\n" \
"            document.getElementById ('position').disabled = false;\n" \
"            document.getElementById ('positionLabel').style.color = 'black';\n" \
"            document.getElementById ('posTreshold').disabled = false;\n" \
"            document.getElementById ('posTriggerLabel').style.color = 'black';\n" \
"            document.getElementById ('negTreshold').disabled = false;\n" \
"            document.getElementById ('negTriggerLabel').style.color = 'black';\n" \
"          }\n" \
"        }\n" \
"      }\n" \
"\n" \
"    </script>\n" \
"  </body>\n" \
"</html>\n"
