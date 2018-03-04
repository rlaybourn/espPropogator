
#define MAX_DEVICES 10

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
#include <Losant.h>
#include "ds188.h"
#include "pid.h"
#include "serverFuncs.h"
#include "constants.h"
#include <DHT.h>

#define MAX_DEVICES 10


const char* ssid = "gody7334";
const char* password = "88888888";

// const char* mqtt_server = "192.168.0.28";
// WiFiClient espClient;
// PubSubClient mymqttclient(espClient);

// Losant credentials.
const char* LOSANT_DEVICE_ID = "5935a65642f50c000880cce0";
const char* LOSANT_ACCESS_KEY = "c1458055-39c1-4f99-adee-03cd0e72fe1c";
const char* LOSANT_ACCESS_SECRET = "fb1be3c9515ac5741cbb2c41dc02e9d6b3dbebe9abb4cb416fd34013812d56f4";

String reportbuf = "";
bool wantsend = false;


ESP8266WebServer server(80);

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
unsigned long lastUpdate = millis();

float wort;
float ambient;
float inside;
float tempkp = 0.0;
float tempki = 0.15;
float humidity = 0.0;
float DHTTemp = 0.0;
unsigned long disconecttime = millis();
unsigned long pwmtimer = millis();
unsigned long lastdbg = millis();
int pwmcounter = 0;
int pidoutput = 100;

const char* mqtt_server = "192.168.0.12";
WiFiClientSecure wifiClient;
WiFiClient espClient;
PubSubClient mymqttclient(espClient);
LosantDevice device(LOSANT_DEVICE_ID);


void setupOTA();
void setupThinger();
void updatesensors();
void controltemp();
void updateWifiLed();
void handleCommand(LosantCommand *command);
//callback for non losant mqtt connection
void callback(char* topic, byte* payload, unsigned int length);


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
  setupOTA();
  loadconsts();
  tempkp = pk;
  tempki = ik;
  loadsp();


  //losant connection
  device.onCommand(&handleCommand);
  device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);
  while(!device.connected()) {
    delay(500);
    Serial.print(".");
  }
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  Serial.println("update");
  root["setpoint"] = setpoint;
  root["power"] = 0;
  root["kp"] = pk;
  device.sendState(root);

  mymqttclient.setServer(mqtt_server, 1883);
  mymqttclient.setCallback(callback);
   if(mymqttclient.connect("proper", "richard", "banaka44"))
   {
     Serial.println("connected ok");
     mymqttclient.publish("test", "hello P");
     mymqttclient.subscribe("testin");
   }


}

void loop(void){

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

  device.loop();

  if(mymqttclient.connected())//report to personal mqtt server
  {
    mymqttclient.loop();
  }

  if(spchanged) //store setpoint
  {
    int pointer = setpointstore;
    EEPROM.put(pointer,setpoint);
    EEPROM.commit();
    spchanged = false;
  }

  if((millis() - lastdbg) > 5000)
  {
    if(mymqttclient.connected())//report to personal mqtt server
    {
      char msg[30];
      sprintf(msg,"p %d -%lu",pidoutput,millis());
      mymqttclient.publish("pr/1/dbg", msg,false);
    }
    else
    {
      mymqttclient.connect("proper", "richard", "banaka44");
    }
    lastdbg = millis();
  }

  if((millis() - lastUpdate) > 240000)
  {
    StaticJsonBuffer<400> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    Serial.println("update");
    root["curtemp"] = wort;
    root["humidity"] = 50;//humidity;
    root["setpoint"] = setpoint;
    root["power"] = pidoutput;
    root["kp"] = pk;
    device.sendState(root);
    String output;
    if(device.connected())
    {
      root.printTo(output);
    }
    else
    {
      device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);
    }
    
    if(mymqttclient.connected())//report to personal mqtt server
    {
      mymqttclient.publish("pr/1/stat", output.c_str(),true);
    }
    else
    {
      if(mymqttclient.connect("proper", "richard", "banaka44"))
      {
        Serial.println("connected ok");
        mymqttclient.publish("pr/1/stat", output.c_str(),true);
        mymqttclient.subscribe("ir/1/cmd");
      }
    }



    lastUpdate =  millis();
  }

  server.handleClient();
  updatesensors();
  controltemp();
  ArduinoOTA.handle(); //allow OTA programming
  updateWifiLed();

}

void updateWifiLed()
{
    if(WiFi.status() == WL_CONNECTED)//update wifi status led
    {
      digitalWrite(led, 1);
    }
    else
    {
      digitalWrite(led, 0);
    }

}


void setupOTA() //setup wifi programming
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

void controltemp() //implement pwm  based on counter
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
  if((millis() - pwmtimer) > 100)  //counter for pwm , period is 10 seconds
  {
    pwmcounter++;
    pwmcounter = pwmcounter % 100;
    pwmtimer = millis();
  }

}

void updatesensors() //read sensors without blocking
{
  if((millis() - lastiter) > 2000) //every 2 seconds
  {
    if(readready)//if readings already triggered then collect results
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
    else //otherwise trigger a reading ready for next time
    {
      startReadings();
      readready = true;
    }
    humidity = dht.readHumidity(); //read DHT22
    DHTTemp = dht.readTemperature(false);
    if(WiFi.status() == WL_CONNECTED)//update wifi status led
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

void handleCommand(LosantCommand *command)
{
  JsonObject& payload = *command->payload;
  if(strcmp(command->name, "setp") == 0)
  {
    Serial.println("seting new period");
    float newTemp = payload["T"];
    Serial.println(newTemp);
    setpoint = newTemp;
    int pointer = setpointstore;
    EEPROM.put(pointer,setpoint);
    EEPROM.commit();
  }
}

//callback for non losant mqtt connection
void callback(char* topic, byte* payload, unsigned int length) {
}
