#pragma GCC optimize("Os")

#if defined(ESP8266)
#include <ESP8266WiFi.h>	   //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#include <AsyncTCP>
#endif

#include <iostream>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>     //Local WebServer used to serve the configuration portal
#include <ESPAsyncWiFiManager.h>   //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ESP8266mDNS.h>

#define TRIGGER_PIN 0
#define IO_PWR 4
#define IO_RST 14

uint16_t timeout = 500; // seconds to run for
int serialSpeed = 9600;
String version = "1.0.2";

//input fields
const char* PARAM_INPUT_1 PROGMEM = "input1";
const char* PARAM_INPUT_2 PROGMEM = "input2";
const char* PARAM_INPUT_3 PROGMEM = "input3";

//io trigger because espasyncwebserver
// https://stackoverflow.com/questions/63237625/esp8266-espasyncwebserver-does-not-toggle-gpio-in-callback
boolean trigger_start = false;
boolean trigger_restart = false;
boolean trigger_forceshutdown = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
DNSServer dns;

//ESP8266 Update credential
//const char *myUsername = "Update";
//const char *myPass = "Update123";

// HTML web page to handle the input fields
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html id="html"><head>
<title>ESP Remote</title><meta name="viewport" content="width=device-width, initial-scale=1">
<script>function submitChange() {setTimeout(function(){document.location.reload(false);},500);}</script>
<style>#html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;} .button {background-color: #b01af0; border: none; color: white; padding: 16px 40px;
text-decoration: none; font-size: 30px; margin: 2px; width: 300px;}
@media (prefers-color-scheme: dark) { #html { background-color: #36393F; color: #2D2D2D;}
</style></head><body>
<form action="/get"><button class="button" type="submit" value="Submit" name="input1" onclick="submitChange()">Start/Shutdown</button></form><br>
<form action="/get"><button class="button" type="submit" value="Submit" name="input2" onclick="submitChange()">Restart</button></form><br>
<form action="/get"><button class="button" type="submit" value="Submit" name="input3" onclick="submitChange()">Force Shutdown</button></form><br><br><br><br>
<a href="/update"><button class="button" name="input4">Update</button></a><br><br><br>
<button class="button" name="version">1.1.0</button>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

// Get chipID based on the last 6 Characters of the MAC address without the colons
 String getChipID(){
  String mac = WiFi.macAddress();
  // Extract last 8 characters of the MAC address with colons
  String chipID = mac.substring(mac.length()-8);
  // Remove colons from the chipID
  chipID.replace(":", "");
  // return the chipID variable without the colons
  return chipID;
 }

void setup() {
  Serial.begin(serialSpeed);
  WiFi.mode(WIFI_STA);
  Serial.println("\n Starting");
 // Initialize the output variables as outputs
  pinMode(IO_PWR, OUTPUT);
  pinMode(IO_RST, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  
  // Set outputs to LOW
  digitalWrite(IO_PWR, LOW);
  digitalWrite(IO_RST, LOW);
  String chipID = getChipID();
  
  String ver = version;
  
  Serial.println("Chip ID: " + chipID);
  Serial.println("Version: " + ver);

  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect(("ESP-"+chipID).c_str(), "ESP-Remote");

  WiFi.mode(WIFI_STA);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }  

  Serial.println();
  Serial.println("Chip ID: " + chipID);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    if (request->hasParam(PARAM_INPUT_1)) {
      //StartPC();
      trigger_start = true;
    }
    // GET input2 value on <ESP_IP>/get?input2=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_2)) {
      //RestartPC();
      trigger_restart = true;
    }
    // GET input3 value on <ESP_IP>/get?input3=<inputMessage>
    else if (request->hasParam(PARAM_INPUT_3)) {
      //ForceShutdownPC();
      trigger_forceshutdown = true;
    }
    //request->send_P(200, "text/html", index_html);
  });


  server.onNotFound(notFound);
  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();

  //Initialize mDNS
  //if (!MDNS.begin(chipID.c_str())) {
    if (!MDNS.begin("espc")) {
    Serial.println("Error setting up MDNS responder!");
  }
}


void loop() {
  //Update mDNS
  MDNS.update();
  
  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == LOW ) {

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    AsyncWiFiManager wifiManager(&server,&dns);

    //reset settings - for testing
    //wifiManager.resetSettings();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    wifiManager.setTimeout(timeout);

    //it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration

    //WITHOUT THIS THE AP DOES NOT SEEM TO WORK PROPERLY WITH SDK 1.5 , update to at least 1.5.1
    WiFi.mode(WIFI_STA);

    wifiManager.setTryConnectDuringConfigPortal(false);

    String chipID = getChipID();
    if (!wifiManager.startConfigPortal(("ESP-" + chipID + " OnDemand").c_str())) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
  if (trigger_start){
    Serial.println("Start//Shutdown PC");
    digitalWrite(IO_PWR, HIGH);
    delay(300);
    digitalWrite(IO_PWR, LOW);
    trigger_start = false;
  }
  if (trigger_restart){
    Serial.println("Restart PC");
    digitalWrite(IO_RST, HIGH);
    delay(300);
    digitalWrite(IO_RST, LOW);
    trigger_restart = false;
  }
  if (trigger_forceshutdown){
    Serial.println("Force Shutdown");
    //Serial.println("Start");
    digitalWrite(IO_PWR, HIGH);
    delay(11000);
    digitalWrite(IO_PWR, LOW);
    //Serial.println("Stop");
    trigger_forceshutdown = false;
  }
}
