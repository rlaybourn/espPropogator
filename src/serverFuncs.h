#include <ESP8266WebServer.h>

extern bool active;
extern float setpoint;
extern float wort;
extern float ambient;
extern float inside;
extern float humidity;
extern unsigned long disconecttime;
extern int pwmcounter;
extern int pidoutput;
extern unsigned long controlcount;
extern ESP8266WebServer server;

void handleRoot();
void handleRaw();
void handleNotFound();
