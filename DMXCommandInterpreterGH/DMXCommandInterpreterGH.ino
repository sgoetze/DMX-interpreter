 /*
 * DMX Command Interpreter
 * ***********************
 * replacing these cheep 192 CH DMX Light Controllers (8 channel sliders)
 * with web interface to edit and run a simple command language  
 * **********************************************************************
 * using esp_dmx (DMXWrite)
 * Created 10 September 2021
 * By Mitch Weisbrod
 * https://github.com/someweisguy/esp_dmx
 * **********************************************************************
 * created in December 2021
 * by Stefan Goetze
 * **********************************************************************
 * Many thanks to Sara and Ruis Santos (Random Nerd Tutorials) 
 * for knowledge and tricks about ESP8286 and ESP32 
 * 
 */
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <esp_dmx.h>


// Web-GUI and log messages in German when uncommented
#define GERMAN

#ifdef GERMAN
#include "website_de.h"
#else
#include "website_en.h"
#endif

// EEPROM
#define EEPROM_SIZE 4096
#define FILE_SIZE 2040

//DMX_MAX_PACKET_SIZE from esp_dmx is 513 (512+1)
//here it only needs 192 channels 
#define DMX_PACKET_SIZE 193   //for use with esp_dmx +1

typedef WebServer myWebServer;

myWebServer server(80);

//you have to access the web gui
//const IP address may be helpful
//#define USE_DHCP                      //comment out "//#define ..." for no dhcp use
#ifndef USE_DHCP
IPAddress ip(192, 168, x, y);         //IP-parameters for this client
IPAddress dns(192, 168, x, d);    
IPAddress gateway(192, 168, x, g);
IPAddress subnet(255, 255, 255, 0);
#endif

//WiFi credentials
const char* ssid = "Uijuijui";         //WiFi-AP 
const char* password = "Hmmm???";      //PSK

//parameters for OTA are set directly in setup()

//ESP32 connection to Maxim MAX485
int transmitPin = 17;  //to MAX485-Pin4:DI
int receivePin = 16;   //to MAX485-Pin1:RO (possibly with level shifter if MAX485 powered with 5V, but ESP32 is said to have 5V tolerant IO)

dmx_port_t dmxPort = 2;   //which UART ist configured for DMX connection above

//values that possibly have to be changed
#define DMX_TIME  10                          // ms to send next dmx packet
                                              // 88 us (gap) + 193 * 40 us packet = 8 ms (but there are little other things to do)  
#define FADE_TIME 1                           // ms for one fade step -> to fade between buffers = 254 steps = about 1/4 sec
                                              // that means F1:1 fades from buffer 1 to buffer 2 in 254 ms --- because such fast fades 
                                              // are fairly useless you can change FADE_TIME to 4 -> F1:1 fades in 1 sec
                                              
#define STROBE_TIME 19                        // let's try this it's * 100ms
#define STARTSTROBE 3                         // value defining length of flash
                                                               
#define MAX_VAL 48                            // maximum of values which can be assigned to channels // use two command if you need more

#define MAX_LOOP 100                          // maximum for nesting loop commands // you can have maximum 99 nested loops in an outer loop

#define MAX_LOG 10                            //log lines  (max. 255)

//global variables
String multi;                                 //buffer for editing command sequences - work buffer
String runni;                                 //buffer for running command sequences                   
int16_t cmdCounter;                           //cmd by cmd going through runni
String logs[MAX_LOG];                         //last MAX_LOG log messages
uint8_t log_index;
uint8_t buffer_dmx[DMX_PACKET_SIZE] = { 0 };  //buffer for transmitting DMX-channels time controlled
uint8_t buffer_strobe [DMX_PACKET_SIZE];      //to hold temporarily content of buffer_dmx for strobe flashs
uint8_t dmxBuffer1[DMX_PACKET_SIZE];          //2 buffers for channel values to display and fade between
uint8_t dmxBuffer2[DMX_PACKET_SIZE];
uint8_t fade_diff[DMX_PACKET_SIZE];           //calculated fader steps per channel
uint8_t *bufPfrom, *bufPto;                   //pointer to buffers for dynamic use
uint8_t lastSourceBuffer;                     //buffer last located as source by d or f cmd
int16_t fade_counter;                         //running index for fade
int16_t fade_timer;                           //fade time (secs)
uint8_t eeprom_no = 1;                        //file buffers 1 & 2
uint16_t strobeOnVal = 0;                     //strobeOnVal = flash - number of dmxTicks 
uint16_t strobeOffVal = 0;                    //strobeOffVal = normal gap between flashs
uint16_t strobeOn, strobeOff;                 //counters for above 
int16_t startStr, endStr;                     //borders for limiting the strobe to a few channels
uint8_t strobeBuffer;                         //if stobe is switched on it can be always (0) or only if buffer 1,2 is source
bool justStrobe = false;                      //if strobe conditions changed just during stobe this enables to reload buffer_dmx              
bool prepareRun = false;                      //button run pressed or run on boot - prepare for runMode
bool runMode = false;                         //executing cmd sequence in runni
bool displayWait = false;                     //constant display a given time in runTick is activated
bool fadeWait = false;                        //fade steps with given time in runTick is activated
bool strobeEna = false;                       //additional strobe effect during constant display or fade 
bool dmxPacketSent = false;                   //to have the test for DMX sent to the begin of the loop
uint32_t dmxTick;                             //clock for sending DMX frames
uint32_t fadeTick;                            //clock to check/change channel values during fade           
uint32_t runTick;                             //clock to delay for given times during runMode
uint8_t runState;                             //state machine for workflow in commands D, F, S
uint32_t endStrobeDemo;                       //for stopping strobe effects when initiated by execute and not by run
struct loop_param {                           // struct for loop management
          int16_t cmd;
          uint32_t secs;
          int16_t cnt;
} loopMem[MAX_LOOP];
uint8_t loopIndex = 0;                        //nested loop pointer

//prototypes for functions in interpreter tab
int16_t cmdInterpreter(String, bool);
void findCmd(String, int16_t, int16_t, int16_t *, int16_t *);
String noComments(String, char);
String noUseless(String);

int16_t writeEEPROMFile(String str, int16_t indexE) {   //write to EEPROM position <indexE> content of multi or runni, length control for development
  int16_t i;
  EEPROM.write(indexE++, 'D');
  EEPROM.write(indexE++, 'M');
  EEPROM.write(indexE++, 'X');
  for (i=0; i<str.length(); i++) {
    EEPROM.write(indexE++, str[i]);
    if (i > 2042) break;
  }
  EEPROM.write(indexE,'\0');
  EEPROM.commit();
  return (i+4);
}
  
int16_t readEEPROMFile (int16_t indexE) {  //read from EEPROM position <indexE> until '\0' or overflow to <multi>  
  int16_t i = 0;
  char c;
  if ((EEPROM.read(indexE) != 'D') || (EEPROM.read(indexE+1) != 'M') || (EEPROM.read(indexE+2) != 'X')) 
    return (-1); 
  indexE += 3;
  multi = "";
  c = EEPROM.read(indexE++);
  while (c != '\0') {
    multi.concat(c);
    c = EEPROM.read(indexE++);
    i++;
    if (i>2047) break;
  }
  return (i);
}

void fill_runni() {       //eleminate comments and not nessecary chars in cmd buffer for easier cmd analyzing - multi fills runni
  if ((runni = noComments(multi, '"')) == "")
    logWrite(COMMENT_ERROR);
  runni = noUseless(runni);
  if (runni[0] == '\n' || runni[0] == ';')
    runni = runni.substring(1);  
}

void webPage(String area) {   //insert working elements in webpage prototype
  String html = "";
  html.concat(String(index_html));
  html.replace("$$$var01$$$", WiFi.localIP().toString());
  html.replace("$$$var02$$$", area);
  if (eeprom_no == 1) {
    html.replace("$$$var03$$$", "checked");
    html.replace("$$$var04$$$", "");
  } else {
    html.replace("$$$var03$$$", "");
    html.replace("$$$var04$$$", "checked");
  }
  html.replace("$$$var05$$$", logList());
  server.send(200, "text/html", html);
}

String logList() {     //build last lines of website from logs
  String tempS = "";
  uint8_t k = log_index;
  for (uint8_t i=0; i<MAX_LOG; i++) {
    k = (k == 0) ? (MAX_LOG-1) : k-1;
    tempS.concat(logs[k]);
    if (i<(MAX_LOG-1)) tempS.concat("<br>");
  }
  return (tempS); 
}

void logWrite(String myLog) {   //store a log message
  logs[log_index++] = String(myLog);
  log_index = log_index % MAX_LOG;
}

void handleRoot() {
  multi = server.arg("multi");
  webPage(multi); 
}

void handleSave() {
  bool compressed = false;
  multi = server.arg("multi");
  multi.replace("\r", "");
  if (!runMode) {
    if (multi.length() > 2042) {
      fill_runni();
      if (runni.length() >2042) {
        logWrite(SAVETOOBIG + String(runni.length()));
        return;
      } else {
        compressed = true;
      }
    }
    String eFile = server.arg("eeprom");
    int16_t index;
    eeprom_no = eFile.toInt();
    if (eeprom_no == 1) index = 0;
    else if (eeprom_no == 2) index = 2048;
    else {
      Serial.println("error handleSave, no file number"); 
      return;
    }
    writeEEPROMFile(compressed ? runni : multi, index);
    logWrite((!compressed ? FULLSAVE1+String(multi.length())+FULLSAVE2 : COMPRESSEDSAVE1+String(runni.length())+COMPRESSEDSAVE2) + eFile);
  } else
    logWrite(SAVEBLOCKED);  
  webPage(multi); 
}
 
void handleLoad() {
  multi = server.arg("multi");
  if (!runMode) {
    String eFile = server.arg("eeprom");
    int16_t index, r;
    char c;
    eeprom_no = eFile.toInt();
    if (eeprom_no == 1) index = 0;
    else if (eeprom_no == 2) index = 2048;
    else {
      Serial.println("error handleLoad, no file number"); 
      return;
    }
    r = readEEPROMFile(index);
    logWrite((r < 0)?String(NOFILE):(r > 2047)?String(PARTLYREAD):String(FULLREAD) + eFile);
  } else
    logWrite(READBLOCKED);
  webPage(multi);
}
 
void handleExec() {
  int16_t cmdStart, cmdEnd;
  String myStart = server.arg("startpos");
  multi = server.arg("multi");
  if (!runMode) {
    findCmd(multi, myStart.toInt(),0,&cmdStart,&cmdEnd);
    String cmdLine = "";
    cmdLine.concat(noUseless(multi.substring(cmdStart, cmdEnd)));
    int16_t r = cmdInterpreter(cmdLine, true); //only one cmd
    if (r<0)
      logWrite(String(ERRORPOS) + String(abs(r)) + ": |" + cmdLine + "|");
    else 
      logWrite(String(OK_DONE) + ": |" + cmdLine + "|");
  } else
    logWrite(EXECBLOCKED);
  webPage(multi);  
}

void handleReload() {
  webPage(multi);
}

void handleRun() {
  multi = server.arg("multi");
  if (!runMode && !prepareRun) {
    prepareRun = true;
    logWrite(RUNNING);
  } else {
    logWrite(STILLRUNNING);
  }
  webPage(multi);
}

void handleStop() {
  static uint32_t stopPressed = 0;
  multi = server.arg("multi");
  uint32_t myNow = millis();
  if (runMode) {
    runMode = false;
    logWrite(STOPPED);
  }
  if (myNow < stopPressed)
    memset(buffer_dmx, 0, DMX_PACKET_SIZE);
  webPage(multi);
  stopPressed = myNow + 3000L;
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  //prepare log array
  for (uint8_t i=0; i<MAX_LOG;i++)
    logs[i]="";
  log_index = 0;
  
  //begin WiFi 
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
#ifndef USE_DHCP  
  WiFi.config(ip, dns, gateway, subnet);
#endif
  WiFi.begin(ssid, password);
  Serial.println("");
  // Wait for connection
  uint8_t tryIt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (tryIt++ > 20) ESP.restart();
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32dmx")) {
    Serial.println("MDNS responder started");
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("DMX_CI");

  // No authentication by default
  // ArduinoOTA.setPassword("");

  // Password can be set with it's md5 value as well
  ArduinoOTA.setPasswordHash("*** this is what I used  ***");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
      logWrite(OTA);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/load", handleLoad);
  server.on("/exec", handleExec);
  server.on("/reload", handleReload);
  server.on("/run", handleRun);
  server.on("/stop", handleStop);
  server.onNotFound(handleNotFound);

  server.begin();
  logWrite(HTTPSTARTED);
  
  /*  Configure the DMX hardware to the default DMX settings and tell the DMX
      driver which hardware pins we are using. */
  dmx_config_t dmxConfig = DMX_DEFAULT_CONFIG;
  dmx_param_config(dmxPort, &dmxConfig);
  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  /* Now we can install the DMX driver! We'll tell it which DMX port to use and
    how big our DMX packet is expected to be. Typically, we'd pass it a handle
    to a queue, but since we are only transmitting DMX, we don't need a queue.
    We can write `NULL` where we'd normally put our queue handle. We'll also
    pass some interrupt priority information. The interrupt priority can be set
    to 1. */
  int queueSize = 0;
  int interruptPriority = 1;
  dmx_driver_install(dmxPort, DMX_PACKET_SIZE, queueSize, NULL, interruptPriority);

  /* Finally, since we are transmitting DMX, we should tell the DMX driver that
    we are transmitting, not receiving. Even setting first Byte in dmxOutBuffer */
    
  dmx_set_mode(dmxPort, DMX_MODE_TX);

  //enabling EEPROM with maximum 0f 4kb = 2 command files with 2040 byte length
  if (!EEPROM.begin(EEPROM_SIZE)) {
    logWrite(EEPROMFAILED);
    delay(10000);
  }

  //prepare the logic
  runMode = false;
  strobeEna = false;
  prepareRun = false;                         
  dmxPacketSent = false;
  dmxTick = millis();

  //reading File1 from EEPROM for run after boot
  eeprom_no = 1;
  if (readEEPROMFile(0) != -1) {
    prepareRun = true;
  }
}

void loop(void) {
  uint32_t now = millis(), tmpL;
  int16_t cmdStart, cmdEnd;
  bool runEnd = false;

  //OTA
  ArduinoOTA.handle();
    
  //DMX
  if (now >= dmxTick) {
    if (dmxPacketSent) {
      dmx_wait_tx_done(dmxPort, DMX_TX_PACKET_TOUT_TICK);  //returns ESP_OK or not, but what is to do?
      dmxPacketSent = false;
      if ((tmpL = millis() - now) > DMX_TIME/2) {
        logWrite(DMX_WAITED + String(tmpL));
        Serial.print("DMX done waited: ");
        Serial.println(tmpL);
        now = millis();
      }     
    }
    
    //make strobes by temporary change of dmx packets
    if (strobeEna && ((strobeBuffer == 0) || (strobeBuffer == lastSourceBuffer))){ //is set/reset by cmdInterpeter()
      //check start condition for a new strobe cycle
      if ((strobeOnVal == STARTSTROBE) && (strobeOffVal > 0) && (strobeOn + strobeOff == 0)) {
        memcpy(buffer_strobe, buffer_dmx, DMX_PACKET_SIZE);
        memset(buffer_dmx + startStr, 0xff, endStr - startStr + 1);
        justStrobe = true;
        strobeOn = strobeOnVal;
        if (strobeOffVal <= 10) strobeOff = strobeOffVal * 10;
        else strobeOff = 5 * (strobeOffVal - 10) + (esp_random() % (20 + 30 * (strobeOffVal - 11))); 
      } else if (strobeOn > 0) {
        if (--strobeOn == 0) {
          memcpy(buffer_dmx, buffer_strobe, DMX_PACKET_SIZE);
          justStrobe = false;
        }
      } else if ((strobeOn == 0) && (strobeOff > 0)) {
        strobeOff--;
      }
      if (!runMode && (now >= endStrobeDemo)) {
        strobeEna = false;
        strobeOnVal = 0;
        strobeOffVal = 0;
        memcpy(buffer_dmx, buffer_strobe, DMX_PACKET_SIZE);
      }
    } else {
      if (justStrobe) { //strobe conditions chaned just during strobe flash -> restoring dmx_buffer
        memcpy(buffer_dmx, buffer_strobe, DMX_PACKET_SIZE);
        justStrobe = false;
      }
    }
    
    // fill & write dmx buffer
    buffer_dmx[0] = 0;  //startbyte
    dmx_write_packet(dmxPort, buffer_dmx, DMX_PACKET_SIZE);
    dmx_tx_packet(dmxPort);
    dmxPacketSent = true;
    dmxTick = now + DMX_TIME;    
  }

  //set all to run command sequence from "runni"
  if (prepareRun) {
    prepareRun = false;
    if (multi.length() > 10) {  //expected minimal length of a program
      cmdCounter = 0;
      loopIndex = 0;
      strobeOnVal = 0;
      strobeOffVal = 0;
      justStrobe = false;
      displayWait = false;
      fadeWait = false;
      strobeEna = false;
      runMode = true;
      runState = 0;
      loopIndex = 0;
      fill_runni();
    } else
      logWrite(FINISHED);
  }
      
  //run mode 
  if (runMode) {
    if (displayWait || fadeWait) {      //a running D-cmd or F-cmd waits for timeout for end / next step
      if (now >= runTick) { //is timeout?
        doRun();    //do it
      }
    } else {     
      findCmd(runni, cmdCounter, 0, &cmdStart, &cmdEnd);
      if (cmdStart >= 0) { //cmd found -> execute
        cmdCounter = cmdStart;
        String cmdLine = "";
        cmdLine.concat(runni.substring(cmdStart, cmdEnd));
        int16_t r = cmdInterpreter(cmdLine, false);
        if (r<0) {
          logWrite(String(ERRORPOS) + String(abs(r)) + ": |" + cmdLine + "|");
          runMode = false;   //wrong cmd, stop runMode immediate
        } else { 
          logWrite(String(OK_DONE) + ": |" + cmdLine + "|");
          findCmd(runni, cmdCounter, 1, &cmdStart, &cmdEnd);   //next cmd
        } 
      } 
      if (cmdStart < 0) {  //no cmd (or no next cmd) to execute 
          runMode = displayWait || fadeWait;  //end runMode if no open timer
          cmdCounter = runni.length() + 1;   //no further succesful findCmd()
          if (!runMode) {
            logWrite(FINISHED);
          }
      } else {
        cmdCounter = cmdStart;
      }
    }
    // more next loop() call  
  }
  
  server.handleClient();
}
