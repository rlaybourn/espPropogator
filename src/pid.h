#ifndef pidfile
#define pidfile


extern float pk,ik,integral,error,output;

float maxi(float one,float two);
float mini(float one, float two);
void storeconsts();
float getKp();
float getKi();
void loadconsts();
void setPid(float p,float i);
void setKp(float p);
void setKi(float i);
int updatePid(float target,float current);
#endif
