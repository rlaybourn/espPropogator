#include "serverFuncs.h"
#include "constants.h"
#include "pid.h"
#include <Arduino.h>
#include <EEPROM.h>

void handleRoot() {
  String message = "";
  message += "{\"temp\":";
  message += String(wort);
  message += ",\"heater\":";
  message += String(humidity);
  //message += String(controlcount);
  message += ",\"integral\":";
  message += String(integral);
  message += ",\"pidout\":";
  message += String(pidoutput);
  message += ",\"pwmcount\":";
  message += String(pwmcounter);
  message += "}";
  server.send(200, "application/json",message);
  Serial.println("root");
  disconecttime = millis();
}

void handleRaw(){
  bool updatePIDvals = false;
  bool stored = false;
  float ki,kp;

  //update setpoint if values given
  for (uint8_t i=0; i<server.args(); i++){
    if(server.argName(i) == "setpoint")
    {
      lastsetpoint = setpoint;
      setpoint = server.arg(i).toFloat();
      if(abs(setpoint - lastsetpoint) > 0.1)
      {
        EEPROM.put(20,setpoint);
        EEPROM.commit();
        stored = true;
      }

    }

    //manualy set integral term
    if(server.argName(i) == "integral"){
      integral = server.arg(i).toFloat();
    }

    //update PID if values given
    if(server.argName(i) == "kp"){
      kp = server.arg(i).toFloat();
      setKp(kp);
      //updatePIDvals = true;
    }
    else
    {
      kp = getKp();
    }
    if(server.argName(i) == "ki"){
      ki = server.arg(i).toFloat();
      setKi(ki);
      //updatePIDvals = true;
    }
    else
    {
      ki = getKi();
    }
  }
   //if kp or ki was changed
  // if(updatePIDvals)
  // {
  //   setPid(kp,ki);
  // }
  float storedsp;
  EEPROM.get(20,storedsp);


  String message = "";
  message += "{\"wort\":";
  message += String(wort);
  message += ",\"amb\":";
  message += String(stored);
  message += ",\"setp\":";
  message += String(setpoint);
  message += ",\"stored\":";
  message += String(storedsp);
  message += ",\"kp\":";
  message += String(getKp());
  message += ",\"Ki\":";
  message += String(getKi());
  message += "}";
  server.send(200, "application/json",message);
   disconecttime = millis();
  Serial.println("raw");
}

void handleNotFound(){

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);

  //disconecttime = millis();
  // reportbuf = message;
  // wantsend = true;
}
