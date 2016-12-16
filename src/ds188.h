#include <Arduino.h>

byte scratchpad[8];
byte *devices[10];
int NumOfSensors = 0;
bool readready = false;
float temperatures[8];

unsigned long lastiter = millis();

OneWire ds(4);

int detectdevices()
{
  int numdevices = 0;
  devices[0] = new byte[8];
  int resultof = ds.search(devices[0]);
  Serial.println(resultof);
  while(resultof > 0)
  {
    numdevices++;
    Serial.print("found ");Serial.println(numdevices);
    devices[numdevices] = new byte[8];
    resultof =  ds.search(devices[numdevices]);
  }
  return numdevices;
}

void startReadings()
{
  for(int i =0;i < NumOfSensors;i++)
  {
    ds.reset();
    ds.select(devices[i]);
    ds.write(0x44,0);
  }
}

float readTemp(byte devNum)
{
  ds.reset();
  ds.select(devices[devNum]);
  ds.write(0xBE,0);
  for(int i=0;i<8;i++)
  {
    scratchpad[i] = ds.read();
  }
  int tempraw = (scratchpad[1] << 8) + scratchpad[0];
  float realtemp = tempraw * 0.0625;
  Serial.print(realtemp);
  Serial.print(" ");
  return realtemp;
}
