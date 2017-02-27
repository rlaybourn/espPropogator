#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>
#include <EEPROM.h>
#include "ds188.h"
#include "pid.h"
#include "serverFuncs.h"
#include "constants.h"




const char* ssid = "gody7334";
const char* password = "88888888";
String reportbuf = "";
bool wantsend = false;
bool active;

ESP8266WebServer server(80);


byte addr[8];
const int led = 13;
float setpoint = 0.0;

float wort;
float ambient;
float inside;
unsigned long disconecttime = millis();
unsigned long pwmtimer = millis();
int pwmcounter = 0;
int pidoutput = 100;


bool supress = false;
int wantFridge = 1;
int wantHeater = 1;




void controltemp()
{
  float controlvar = wort;

  if(!active) //abort if no temperature set yet;
    return;

  if(pwmcounter < pidoutput)
  {
    digitalWrite(Heater,turnon);
  }
  else
  {
    digitalWrite(Heater,turnoff);
  }

  if(pidoutput < -50)
  {
    digitalWrite(Fridge,turnon);
  }
  else
  {
    digitalWrite(Fridge, turnoff);
  }

}

void updatesensors()
{
  if((millis() - lastiter) > 2000) //every 2 seconds
  {
    if(readready)
    {
      for(int i = 0; i < NumOfSensors; i++)
      {
        temperatures[i] = readTemp(i);
      }
      wort = temperatures[0];
      inside = temperatures[1];
      pidoutput = updatePid(setpoint,wort);
      Serial.print(wort);Serial.print(" ");
      Serial.print(setpoint);Serial.print(" ");
      Serial.print(pidoutput);Serial.print("\n");
      readready = false;
    }
    else
    {
      startReadings();
      readready = true;
    }
    if(WiFi.status() == WL_CONNECTED)
    {

      oled.setTextXY(0,0);
      oled.putString("         ");
      oled.setTextXY(0,0);
      oled.putString("online      ");
      digitalWrite(led, 1);
    }
    else
    {
      oled.setTextXY(0,0);
      oled.putString("         ");
      oled.setTextXY(0,0);
      oled.putString("offline     ");
      digitalWrite(led, 0);
    }
      oled.setTextXY(4,0);
      oled.putString("         ");
      oled.setTextXY(4,0);
      oled.putString("Wo:");oled.putNumber(wort); oled.putString(" Sp:"); oled.putNumber(setpoint);
      oled.setTextXY(1,0);
      oled.putString(WiFi.localIP().toString());
      oled.setTextXY(6,0);
      oled.putString("         ");
      oled.setTextXY(6,0);
      oled.putNumber(millis() - disconecttime);
      lastiter = millis();
  }
}



void setup(void){
  pinMode(led, OUTPUT);
  pinMode(Fridge,OUTPUT);
  pinMode(Heater,OUTPUT);
  pinMode(Reset,OUTPUT);
  digitalWrite(Fridge,1);
  digitalWrite(Heater,1);
  digitalWrite(led, 0);
  digitalWrite(Reset,1);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");
  EEPROM.begin(100);
  Wire.begin();
  oled.init();                      // Initialze SSD1306 OLED display
  oled.clearDisplay();              // Clear screen
  oled.setTextXY(0,0);              // Set cursor position, start of line 0
  oled.putString("Startup");

  int timeout = 0;
  // Wait for connection
  while ((WiFi.status() != WL_CONNECTED) && (timeout < 50)) {
    delay(500);
    timeout ++;
    Serial.print(".");
  }
  if(timeout >= 50) //if cant connect then try another reset
  {
    digitalWrite(Reset,0);
    oled.clearDisplay();              // Clear screen
    oled.setTextXY(0,0);              // Set cursor position, start of line 0
    oled.putString("Failed Retry");
  }

  oled.setTextXY(1,0);
  oled.putString(WiFi.localIP().toString());

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  //setup server
  server.on("/", handleRoot);
  server.on("/raw",handleRaw);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
  NumOfSensors = detectdevices();
  setPid(40,0.1);
  active = false;

  ArduinoOTA.setHostname("Brewer");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  loadconsts();

}

void loop(void){
  if (WiFi.status() != WL_CONNECTED) //try to reconnect
  {

    oled.clearDisplay();              // Clear screen
    oled.setTextXY(0,0);              // Set cursor position, start of line 0
    oled.putString("Disconnected");

    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    oled.clearDisplay();              // Clear screen
    oled.setTextXY(0,0);              // Set cursor position, start of line 0
    oled.putString("Reconected");
    oled.setTextXY(2,0);
    oled.putString(WiFi.localIP().toString());
  }

  if((millis() - disconecttime) > 500000) //if no comms for more than 7 minutes reset
  {
    oled.clearDisplay();              // Clear screen
    oled.setTextXY(0,0);              // Set cursor position, start of line 0
    oled.putString("TO disconnect");
    // digitalWrite(0,HIGH);
    // digitalWrite(2,HIGH);
    digitalWrite(Reset,0);
  }

  if((millis() - pwmtimer) > 100)  //counter for pwm , period is 10 seconds
  {
    pwmcounter++;
    pwmcounter = pwmcounter % 100;
    pwmtimer = millis();
  }

  server.handleClient();
  updatesensors();
  controltemp();
  ArduinoOTA.handle(); //allow OTA programming

}
