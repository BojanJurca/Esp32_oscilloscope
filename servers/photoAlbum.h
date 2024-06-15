// this file has been automatically generated from photoAlbum.html with html2h.vbs

#define photoAlbumBeginning "\n" \
    "<!DOCTYPE html>\n" \
    "<html lang='en'>\n" \
    "<head>\n" \
    "    <title>photoAlbum</title>\n" \
    "\n" \
    "    <meta charset='UTF-8'>\n" \
    "    <meta http-equiv='X-UA-Compatible' content='IE=edge'>\n" \
    "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n" \
    "\n" \
    "    <link rel='shortcut icon' type='image/x-icon' sizes='192x192' href='android-192-pa.png'>\n" \
    "    <link rel='icon' type='image/png' sizes='192x192' href='android-192-pa.png'>\n" \
    "    <link rel='apple-touch-icon' sizes='180x180' href='apple-180-pa.png'>\n" \
    "\n" \
    "    <style>\n" \
    "\n" \
    "    /* heading */\n" \
    "    h1 {\n" \
    "      font-family:verdana;\n" \
    "      font-size:40px;\n" \
    "      text-align:center;\n" \
    "      color:white\n" \
    "    }\n" \
    "\n" \
    "    p {\n" \
    "      font-family:verdana;\n" \
    "      font-size:18px;\n" \
    "      color:white;\n" \
    "    }\n" \
    "\n" \
    "    /* div for picture captions */\n" \
    "    div.d1 {\n" \
    "      height:65px;\n" \
    "      width:100%;\n" \
    "      font-family:verdana;\n" \
    "      font-size:24px;\n" \
    "      color:white;\n" \
    "      position:absolute;\n" \
    "      bottom:-35%;\n" \
    "    }\n" \
    "\n" \
    "    /* div for picture hit counters */\n" \
    "    div.d2 {\n" \
    "      height:65px;\n" \
    "      width:100%;\n" \
    "      font-family:verdana;\n" \
    "      font-size:14px;\n" \
    "      color:white;\n" \
    "      position:absolute;\n" \
    "      bottom:-35%;\n" \
    "      text-align:right;\n" \
    "    }\n" \
    "\n" \
    "    /* button */\n" \
    "    .button {\n" \
    "      padding:10px 15px;\n" \
    "      font-size:22px;\n" \
    "      text-align:center;\n" \
    "      cursor:pointer;\n" \
    "      outline: none;\n" \
    "      color:white;\n" \
    "      border:none;\n" \
    "      border-radius:12px;\n" \
    "      box-shadow: 1px 1px black;\n" \
    "      position:relative;\n" \
    "      top:0px;\n" \
    "      height:42px;\n" \
    "    }\n" \
    "    /* blue button */\n" \
    "    .button1 {\n" \
    "      background-color:hsl(207, 90%, 30%);\n" \
    "    }\n" \
    "    .button1:hover {\n" \
    "      background-color:hsl(207, 90%, 40%);\n" \
    "    }\n" \
    "    .button1:active {\n" \
    "      background-color:hsl(207, 90%, 50%);\n" \
    "      transform: translateY(3px);\n" \
    "    }\n" \
    "    /* red button */\n" \
    "    .button3 {\n" \
    "      background-color:hsl(0, 100%, 25%)\n" \
    "    }\n" \
    "    .button3:hover {\n" \
    "      background-color:hsl(0, 100%, 35%)\n" \
    "    }\n" \
    "    .button3:active {\n" \
    "      background-color:hsl(0, 100%, 45%);\n" \
    "      transform:translateY(3px)\n" \
    "    }\n" \
    "\n" \
    "    /* input password */\n" \
    "    input[type=password] {\n" \
    "      padding:12px 20px;\n" \
    "      font-size:22px;\n" \
    "      height:38px;\n" \
    "      width:90px;\n" \
    "      margin:8px 0;\n" \
    "      box-sizing:border-box;\n" \
    "      border:2px solid hsl(207, 90%, 40%);\n" \
    "      border-radius:12px;\n" \
    "    }\n" \
    "    input[type=password]:focus {\n" \
    "      outline:none;\n" \
    "      padding:12px 20px;\n" \
    "      margin:8px 0;\n" \
    "      box-sizing:border-box;\n" \
    "      border:2px solid hsl(207, 90%, 55%);\n" \
    "      border-radius:12px;\n" \
    "    }\n" \
    "\n" \
    "    /* image container */\n" \
    "    .image-container {\n" \
    "      display:grid;\n" \
    "      grid-template-columns:repeat(auto-fit, minmax(160px, 240px));\n" \
    "      grid-template-rows:auto;\n" \
    "      grid-gap:10%;\n" \
    "      margin-left:7%;\n" \
    "      margin-right:7%;\n" \
    "    }\n" \
    "    .image-container .image {\n" \
    "      position:relative;\n" \
    "      padding-bottom:100%;\n" \
    "    }\n" \
    "    .image-container .image img {\n" \
    "      height:auto;\n" \
    "      width:100%;\n" \
    "      position:absolute;\n" \
    "    }\n" \
    "\n" \
    "    /* image caption */\n" \
    "    textarea {\n" \
    "      font-family:verdana;\n" \
    "      font-size:14px;\n" \
    "      color:white;\n" \
    "      width:100%;\n" \
    "      height:80px;\n" \
    "      border: none;\n" \
    "      border-radius: 12px;\n" \
    "      background-color:transparent;\n" \
    "      overflow:hidden;\n" \
    "      resize:none;\n" \
    "    }\n" \
    "    textarea:focus {\n" \
    "      outline:none !important;\n" \
    "      border-color:hsl(207, 90%, 55%);\n" \
    "      box-shadow:0 0 10px hsl(207, 90%, 55%);\n" \
    "    }\n" \
    "\n" \
    "    </style>\n" \
    "\n" \
    "  <script>\n" \
    "\n" \
    "    // mechanism that makes REST calls and get their replies\n" \
    "    var httpClient = function () {\n" \
    "      this.request = function (url, method, callback) {\n" \
    "        var httpRequest = new XMLHttpRequest ();\n" \
    "        var httpRequestTimeout;\n" \
    "        httpRequest.onreadystatechange = function () {\n" \
    "          // console.log (httpRequest.readyState);\n" \
    "          if (httpRequest.readyState == 1) { // 1 = OPENED, start timing\n" \
    "            httpRequestTimeout = setTimeout (function () { alert ('Server did not reply (in time).'); }, 3000);\n" \
    "          }\n" \
    "          if (httpRequest.readyState == 4) { // 4 = DONE, call callback function with responseText\n" \
    "            clearTimeout (httpRequestTimeout);\n" \
    "            console.log (httpRequest.responseText);\n" \
    "            if (httpRequest.status == 200) callback (httpRequest.responseText); // 200 = OK\n" \
    "            else alert ('Server reported error ' + httpRequest.status + ' ' + httpRequest.responseText); // some other reply status, like 404, 503, ...\n" \
    "          }\n" \
    "        }\n" \
    "        httpRequest.open (method, url, true);\n" \
    "        httpRequest.send (null);\n" \
    "      }\n" \
    "    }\n" \
    "\n" \
    "    function onImgLoad (imgObj) {\n" \
    "      let initialWidth = imgObj.width;\n" \
    "      let initialHeight = imgObj.height;\n" \
    "      let minPictureArea = initialWidth * initialWidth * 9 / 16; // 16:9 aspect ratio gives the smallest area\n" \
    "      let f = Math.sqrt (minPictureArea / (initialWidth * initialHeight));\n" \
    "      // resize the picture so that all the pictures would have the same area\n" \
    "      imgObj.style.height = (initialHeight * f) + 'px';\n" \
    "      imgObj.style.width = (initialWidth * f) + 'px';\n" \
    "      // center the picture\n" \
    "      imgObj.style.transform = 'translate(' + Math.floor ((initialWidth - imgObj.width) / 2) + 'px, ' + Math.floor (((initialWidth - imgObj.height)) / 2) + 'px)';\n" \
    "    }\n" \
    "\n" \
    "    // reload the image: https://stackoverflow.com/questions/33807637/keep-trying-to-reload-image-until-successful-but-dont-show-intermediate-broken\n" \
    "    var loadTime = Date.now ();\n" \
    "    function onImgError (imgObj) {\n" \
    "      if(Date.now () - loadTime < 10000) { // keep trying 10 s\n" \
    "        setTimeout (function() { imgObj.src = imgObj.src; }, 1000);\n" \
    "      }\n" \
    "    }\n" \
    "\n" \
    "    // enable or disable all textareas\n" \
    "    function enableDisableImgCaptions (node, disable) { // https://stackoverflow.com/questions/4256339/how-can-i-loop-through-all-dom-elements-on-a-page\n" \
    "      if (node.name == 'imgCaption') {\n" \
    "          // console.log (node.id);\n" \
    "          node.disabled = disable;\n" \
    "          if (disable)  node.style.border = 'none';\n" \
    "          else          node.style.border = '2px solid hsl(207, 90%, 16%)';\n" \
    "      }\n" \
    "      var nodes = node.childNodes;\n" \
    "      for (var i = 0; i < nodes.length; i++) {\n" \
    "          if (!nodes[i]) continue;\n" \
    "          if (nodes[i].childNodes.length > 0) enableDisableImgCaptions (nodes [i], disable);\n" \
    "      }\n" \
    "    }\n" \
    "\n" \
    "    // detect enter or esc in password field\n" \
    "    document.addEventListener ('keydown', evt => {\n" \
    "      if (document.activeElement.id == 'password') {\n" \
    "        if (evt.key === 'Escape') processPassword (true);\n" \
    "        if (evt.key === 'Enter') processPassword (false);\n" \
    "      }\n" \
    "    });\n" \
    "\n" \
    "    // login when the user presses enter or dismiss when esc\n" \
    "    function processPassword (dismiss) {\n" \
    "      if (!dismiss) {\n" \
    "\n" \
    "        // try to login first\n" \
    "        let client = new httpClient ();\n" \
    "              let clientRequest = '/login' + '/webadmin%20' + document.getElementById ('password').value;\n" \
    "              client.request (clientRequest, 'GET', function (serverReply) {\n" \
    "          if (serverReply == \"loggedIn\") {\n" \
    "            // logged-in\n" \
    "            document.getElementById ('login').hidden = true;\n" \
    "            document.getElementById ('password').hidden = true;\n" \
    "            document.getElementById ('logout').hidden = false;\n" \
    "            enableDisableImgCaptions (document, false);\n" \
    "          } else {\n" \
    "            alert (serverReply); // report error\n" \
    "          }\n" \
    "                });\n" \
    "      }\n" \
    "\n" \
    "      // dismiss or failed to login\n" \
    "      document.getElementById ('login').hidden = false;\n" \
    "      document.getElementById ('password').hidden = true;\n" \
    "      document.getElementById ('logout').hidden = true;\n" \
    "      enableDisableImgCaptions (document, true);\n" \
    "    }\n" \
    "\n" \
    "    // save new image caption when textarea looses focus\n" \
    "    function changeImgCaption (id) {\n" \
    "        // send base64 encoded changed text to the server in REST call\n" \
    "        let client = new httpClient ();\n" \
    "              client.request ('changeImgCaption/' + id + '/' + btoa (unescape (encodeURIComponent (document.getElementById (id).value))), 'PUT', function (serverReply) {\n" \
    "            if (serverReply != 'imgCaptionChanged')\n" \
    "              alert (serverReply);\n" \
    "        });\n" \
    "    }\n" \
    "\n" \
    "    // refresh locally stored counters on page show\n" \
    "    addEventListener ('pageshow', (event) => {\n" \
    "      // update view counters if comming to this page through back button\n" \
    "      Object.keys (sessionStorage).forEach ((key) => {\n" \
    "        let count = sessionStorage.getItem (key);\n" \
    "        if (count > '') {\n" \
    "          document.getElementById (key).innerText = count;\n" \
    "        }\n" \
    "      });\n" \
    "    });\n" \
    "\n" \
    "  </script>\n" \
    "\n" \
    "</head>\n" \
    "\n" \
    "<body style='background-color:hsl(207, 90%, 6%)'>\n" \
    "\n" \
    "  <div class='d2'; style='position:absolute; top:22px; right:22px;'>\n" \
    "    <button class='button button1' id='login' onclick=\"\n" \
    "      this.hidden = true;\n" \
    "      let obj = document.getElementById ('password');\n" \
    "      obj.value = '';\n" \
    "      obj.hidden = false;\n" \
    "      obj.focus ();\n" \
    "    \">&nbsp;webadmin login&nbsp;</button>\n" \
    "\n" \
    "    <input type='password' id='password' placeholder=\"webadmin's password\" style='width:280px;' hidden required onfocusout='if (!this.hidden) processPassword (true);'>\n" \
    "\n" \
    "    <button class='button button3' id='logout' hidden onclick=\"\n" \
    "\n" \
    "        // logout first\n" \
    "        let client = new httpClient ();\n" \
    "              client.request ('/logout', 'PUT', function (serverReply) {});\n" \
    "\n" \
    "        // update GUI elements\n" \
    "        this.hidden = true;\n" \
    "        document.getElementById ('login').hidden = false;\n" \
    "        enableDisableImgCaptions (document, true);\n" \
    "    \">&nbsp;webadmin logout&nbsp;</button>\n" \
    "\n" \
    "  </div>\n" \
    "\n" \
    "    <br><br><h1>Photo album example</h1><br>\n" \
    "\n" \
    "  <p>\n" \
    "    Example of web portal demonstrating:<br>\n" \
    "    &nbsp;&nbsp;&nbsp;- dynamically generated web portal<br>\n" \
    "    &nbsp;&nbsp;&nbsp;- web sessions with login/logout options<br>\n" \
    "    &nbsp;&nbsp;&nbsp;- local database data storage<br>\n" \
    "  </p>\n" \
    "\n"


#define photoAlbumEmpty "\n" \
    "  <div class='d1' style='position:relative;'>\n" \
    "    <b>Quick start guide</b><br><br>\n" \
    "      Upload by using FTP your .jpg photo files to /var/www/html/photoAlbum directory.\n" \
    "      Create smaller versions of your .jpg photo files (approximate 240px x 140px) and give them \"-small\" postfix. For example:\n" \
    "      if your photo file is 20230314_095846.jpg then its smaller version should have name 20230314_095846-small.jpg.\n" \
    "      Upload these files to /var/www/html/photoAlbum directory as well.<br><br>\n" \
    "      Since built-in flash disk is small compared to picture sizes, it is recommended to use a SC card.\n" \
    "  </div>\n"


#define photoAlbumEnd "\n" \
    "</body>\n" \
    "\n" \
    "  <script>\n" \
    "\n" \
    "    // check if webadmin is already logged in (although this checking is not very safe it is only used to enable/disable\n" \
    "    // GUI elements, all the activities webadmin may take will be checked later on the server side as well)\n" \
    "    if (document.cookie.indexOf ('sessionUser=webadmin') !== -1) {\n" \
    "      document.getElementById ('login').hidden = true;\n" \
    "      document.getElementById ('password').hidden = true;\n" \
    "      document.getElementById ('logout').hidden = false;\n" \
    "      enableDisableImgCaptions (document, false);\n" \
    "    }\n" \
    "\n" \
    "    /*\n" \
    "    console.log (screen.width + 'px');\n" \
    "    console.log (screen.height + 'px');\n" \
    "    console.log (screen.orientation.type); // landscape-primary, landscape-secondary, portrait-secondary, portrait-secondary\n" \
    "\n" \
    "    window.addEventListener ('orientationchange', function () {\n" \
    "      console.log (screen.width + 'px');\n" \
    "      console.log (screen.height + 'px');\n" \
    "      console.log (screen.orientation.type); // landscape-primary, landscape-secondary, portrait-secondary, portrait-secondary\n" \
    "    });\n" \
    "    */\n" \
    "\n" \
    "  </script>\n" \
    "</html>\n"
