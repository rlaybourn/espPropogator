#include <Arduino.h>
#include <EEPROM.h>
#include "pid.h"
#include "constants.h"

float pk = 40;
float ik = 2;
float integral = 0;
float error = 0;
float output = 0;


float maxi(float one,float two)
{
  if(one > two)
    return one;
   else
    return two;
}

float mini(float one, float two)
{
  if(one < two)
    return one;
  else
    return two;
}

void storeconsts()
{
  int pointer = constStart;
  EEPROM.put(pointer,pk);
  pointer = pointer + sizeof(float);
  EEPROM.put(pointer,ik);
  EEPROM.commit();
}

float getKp()
{
  return pk;
}

float getKi()
{
  return ik;
}

void loadconsts()
{
  int pointer = constStart;
  EEPROM.get(pointer,pk);
  pointer = pointer + sizeof(float);
  EEPROM.get(pointer,ik);
}

void setPid(float p,float i)
{
  if((p < (pk - 0.1)) || (p > (pk + 0.1)))
  {
    int pointer = constStart;
    EEPROM.put(pointer,p);
  }
  if((i < (ik - 0.1)) || (i > (ik + 0.1)))
  {
    int pointer = constStart + sizeof(float);
    EEPROM.put(pointer,i);
  }
  EEPROM.commit();
  pk = p;
  ik = i;
}

void setKp(float p)
{
  if((p < (pk - 0.1)) || (p > (pk + 0.1)))
  {
    int pointer = constStart;
    EEPROM.put(pointer,p);
    EEPROM.commit();
  }
  pk = p;
}

void setKi(float i)
{
  if((i < (ik - 0.1)) || (i > (ik + 0.1)))
  {
    int pointer = constStart + sizeof(float);
    EEPROM.put(pointer,i);
    EEPROM.commit();
  }
  ik = i;
}


int updatePid(float target,float current)
{
  float pt;
  error = target - current;
  pt = error * pk;
  integral = integral + (error * ik);
  if(integral > 100)
  {
    integral = 100;
  }else if (integral < - 100)
  {
    integral = -100;
  }

  output = integral + pt;
  output = mini(output,100);
  output = maxi(output,-100);
  return (int)output;

}
