#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ThingerESP8266.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ACROBOTIC_SSD1306.h>
#include <EEPROM.h>
#include "ds188.h"
#include "pid.h"
#include "serverFuncs.h"
#include "constants.h"
#include <DHT.h>




const char* ssid = "gody7334";
const char* password = "88888888";
String reportbuf = "";
bool wantsend = false;


ESP8266WebServer server(80);
ThingerWifi thing("rlaybourn", "propogator", "banaka");
#define DHTPIN 5     // what pin we're connected to

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

byte addr[8];
const int led = 13;
float setpoint = 0.0;
float lastsetpoint = 0.0;
bool spchanged = false;
unsigned long controlcount = 0;

float wort;
float ambient;
float inside;
float tempkp = 0.0;
float tempki = 0.15;
float humidity = 0.0;
float DHTTemp = 0.0;
unsigned long disconecttime = millis();
unsigned long pwmtimer = millis();
int pwmcounter = 0;
int pidoutput = 100;




void setupOTA();
void setupThinger();
void updatesensors();
void controltemp();





void setup(void){
  pinMode(led, OUTPUT);
  pinMode(Fridge,OUTPUT);
  pinMode(Heater,OUTPUT);
  pinMode(Reset,OUTPUT);
  digitalWrite(Fridge,1);
  digitalWrite(Heater,0);
  digitalWrite(led, 0);
  digitalWrite(Reset,1);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");
  EEPROM.begin(100);
  Wire.begin();


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
  }

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
  setupThinger();
  setupOTA();
  // ArduinoOTA.setHostname("Brewer");
  // Serial.println("starting ota");
  // ArduinoOTA.onStart([]() {
  //   Serial.println("Start");
  // });
  // ArduinoOTA.onEnd([]() {
  //   Serial.println("\nEnd");
  // });
  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //   Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  // });
  // ArduinoOTA.onError([](ota_error_t error) {
  //   Serial.printf("Error[%u]: ", error);
  //   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //   else if (error == OTA_END_ERROR) Serial.println("End Failed");
  // });

  loadconsts();
  tempkp = pk;
  tempki = ik;
  loadsp();

}

void loop(void){
  thing.handle();
  if (WiFi.status() != WL_CONNECTED) //try to reconnect
  {
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

  }

  if((millis() - pwmtimer) > 100)  //counter for pwm , period is 10 seconds
  {
    pwmcounter++;
    pwmcounter = pwmcounter % 100;
    pwmtimer = millis();
  }

  if(spchanged)
  {
    int pointer = setpointstore;
    EEPROM.put(pointer,setpoint);
    EEPROM.commit();
    spchanged = false;
  }

  server.handleClient();
  updatesensors();
  controltemp();
  ArduinoOTA.handle(); //allow OTA programming

}

void setupThinger()
{
  thing.add_wifi(ssid, password);
  thing["temp"] >> outputValue(wort);
  thing["humidity"] >> outputValue(humidity);

  thing["DHT"] >> [](pson& out){
      out["humidity"] = humidity;
      out["temp"] = DHTTemp;
    };


  thing["setpoint"] << inputValue(setpoint,{
    if((setpoint > 10) && (setpoint < 40))
    {
      EEPROM.put(20,setpoint);
      EEPROM.commit();
    }
  });
  thing["tempkp"] << inputValue(tempkp,
    {
      if((tempkp < 100) && (tempkp > 1))
      {
        setKp(tempkp);
      }
    }
  );
  thing["tempki"] << inputValue(tempki,
    {
      if((tempki < 1) && (tempkp > 0.001))
      {
        setKi(tempki);
        ik= tempki;
      }
    }
  );
}
void setupOTA()
{

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
}

void controltemp()
{
  float controlvar = wort;

  if(pwmcounter < pidoutput)
  {
    digitalWrite(Heater,turnon);
  }
  else
  {
    digitalWrite(Heater,turnoff);
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
    humidity = dht.readHumidity();
    DHTTemp = dht.readTemperature(false);
    if(WiFi.status() == WL_CONNECTED)
    {
      digitalWrite(led, 1);
    }
    else
    {
      digitalWrite(led, 0);
    }

      lastiter = millis();
  }
}
