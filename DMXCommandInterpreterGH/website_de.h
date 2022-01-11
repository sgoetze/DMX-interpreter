//Website CSS, HTML, Javascript deutsch
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html><head>
  <script src="https://code.jquery.com/jquery-latest.min.js"></script>
  <title>DMX-Befehlsinterpreter</title>
  <style>
    html {font-size:100%;}
    h1 {font-size:3rem;}
    h2 {font-size:1.8rem;}
    h3 {font-size:1.4rem;}
  
    .header {height:60px; width:99%; position:relative; background-color:#ccc; border-radius:.5em;}
    .vertical-center {font-size:1.5rem; position:absolute; right:10px; top: 50%; -ms-transform: translateY(-50%); transform: translateY(-50%);}
    .wrapper{display:table; width:100%; }
    .main{width:99%; background: gray; display:inline; }
    .main > textarea{width:inherit; font-size: 1.1rem; }
    
    .toggle-buttons input[type="radio"] {
      clip:rect(0 0 0 0);
      clip-path:inset(50%);
      height:1px;
      overflow:hidden;
      position:absolute;
      white-space:nowrap;
      width:1px
    }
    .toggle-buttons label {
      font-family:sans-serif;
      font-weight:600;
      font-size:1.2rem;
      border:2px solid #333;
      border-radius:0.5em;
      padding:0.5em;
      margin-left:23px;
      margin-right:36px;  
    }
    .toggle-buttons input:checked + label {
      background:#ebf5d7;
      color:#5a9900;
      box-shadow:none;
    }
    input:hover + label,
    input:focus + label {
      background:#ffebe6; 
    }
    .buttons{display:table; width:99%; }
    .left{width:40%; display:table-cell; }
    .center{width:20%; display:table-cell; text-align:center;}
    .right{width:40%; display:table-cell; text-align:right;}
    #save {margin-left:5px;}
    #load {margin-left:25px;}
    #run {margin-right:25px;}
    #stop {margin-right:5px;} 
    button {width:120px; color:navy background-color:#f0f0f0; border:2px solid #333; font-size:1.4rem;}
    button:hover {color:teal; background-color:#ffebe6;}

    .logtext {
      font-family:sans-serif;
      font-weight:500;
      font-size:1.1rem;
    }
  </style>
</head><body>
<script type="text/javascript">
  var isRunning = false;
  $(document).ready(function(){
    setInterval(function(){loadLog()},2000);
    setInterval(function(){checkUpdate()},100);
  });
  function loadLog() {
    if (isRunning)
      $("#logtext").load("http://$$$var01$$$/reload" + " #logtext" );
  }
  function checkUpdate() {
    var myElement = document.getElementById("multi");
    var myStart = myElement.selectionStart;
    var myEnd = myElement.selectionEnd;
    var myUrl = window.location.href;
    document.getElementById("startpos").value = myStart;
    document.getElementById("endpos").value = myEnd;
    isRunning = myUrl.includes("/run");
  }
</script>
<div class="header">
  <h1>&nbsp;192-CH-DMX-Befehlsinterpreter</h1>
  <div class="vertical-center">
    <a href="/">IP:$$$var01$$$</a>&nbsp;
  </div>
</div>
<h2>Befehlseingabe:</h2>
<form method='GET'>
  <input type="hidden" id="startpos" name="startpos">
  <input type="hidden" id="endpos" name="endpos">
  <div class="wrapper">
    <div class="main">
      <textarea id="multi" name="multi" rows="16" 
       ondblclick="this.select();">$$$var02$$$</textarea>
    </div> 
  </div><br>
  <div class="toggle-buttons">
    <input type="radio" id="f1" name="eeprom" value="1" $$$var03$$$>
    <label for="f1">Datei 1</label>
    <input type="radio" id="f2" name="eeprom" value="2" $$$var04$$$>
    <label for="f2">Datei 2</label>
  </div>
  <br>
  <div class="buttons">
    <div class="left">
      <button id="save" type="submit" formaction="/save">Speichern</button>
      <button id="load" type="submit" formaction="/load">Laden</button>
    </div>
    <div class="center">
      <button id="exec" type="submit" formaction="/exec">Ausf&uuml;hren</button>
    </div>
    <div class="right">
      <button id="run" type="submit" formaction="/run">Starten</button>
      <button id="stop" type="submit" formaction="/stop">Stopp</button>
    </div>
  </div>
</form>
<h3>Log:</h3>
<div id="logtext" class="logtext">
$$$var05$$$
</div>
</body></html>  
)rawliteral";

//Log messages
#define HTTPSTARTED "HTTP server gestartet"
#define EEPROMFAILED "Fehler beim Initialisieren des EEPROMs"
#define COMPRESSEDSAVE1 "Eingabebereich komprimiert ("
#define COMPRESSEDSAVE2 " Byte) gespeichert in Datei "
#define FULLSAVE1 "Eingabebereich komplett ("
#define FULLSAVE2 " Byte) gespeichert in Datei "
#define SAVETOOBIG "Programm im Eingabebereich zu gross"
#define PROGLENGTH "L&auml;nge des Programms: "
#define SAVEBLOCKED "Programm l&auml;uft - Speichern geblockt"
#define NOFILE "keine Befehle in EEPROM-Datei "
#define PARTLYREAD "DMX-Befehle nur teilweise gelesen aus EEPROM-Datei "
#define FULLREAD "DMX-Befehle vollst&auml;ndig gelesen aus EEPROM-Datei "
#define READBLOCKED "Programm l&auml;uft - Laden geblockt"
#define OTA "dieser Server erh&auml;lt gerade ein Update per OTA"
#define ERRORPOS "Fehler ca. an Position "
#define EXECBLOCKED "Programm l&auml;uft - Direkt abarbeiten wurde geblockt"
#define STILLRUNNING "Programm l&auml;uft..."
#define OK_DONE "OK - fertig"
#define RUNNING "Gestartet..."
#define STOPPED "Gestoppt"
#define BLACKOUT "Blackout"
#define FADERUN "Fade geht nur bei Programmausf&uuml'hrung"
#define PROGERROR "Programmierfehler: "
#define FINISHED "Fertig"
#define DMX_WAITED "Warten auf DMX: "
#define COMMENT_ERROR "Fehler in Kommentaren"
