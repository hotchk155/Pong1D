#include "sd600.h"

#define SWITCH_A A3
#define SWITCH_B A2
#define BAT_A 49
#define STRIP_LEN 50

#define COL_BATON RGB(0x00,0x00,0xff)
#define COL_BATOFF RGB(0x00,0x00,0x01)

sd600 strip(STRIP_LEN);
void setup()
{
  pinMode(7,OUTPUT);
  pinMode(SWITCH_A,INPUT);
  digitalWrite(SWITCH_A,HIGH);
  digitalWrite(7,LOW);
  delay(10);
  strip.begin();
  Serial.begin(9600);
//  SD600.setLed(0, RGB(255,0,0));
}


unsigned long track[STRIP_LEN];

#define FADE_PERIOD 100
class CPulse 
{
  byte trail[STRIP_LEN];
  
  public:
    float pos;
    float velocity;
    unsigned long colour;
    unsigned long nextFadeTime;
    
    CPulse() {
      pos = 0;
      velocity = 0.4;
      colour = 0xfffffeL;
      memset(trail,0,sizeof(trail));
    }
    
    void run(unsigned long milliseconds)
    {
        if(milliseconds > nextFadeTime) 
        {
          for(int i=0; i<STRIP_LEN; ++i) {
            trail[i] /= 2;
          }
          nextFadeTime = milliseconds + FADE_PERIOD;
        }
        
        int oldPos = (pos + 0.5);
        pos += velocity;
        if(pos < 0) {
          velocity = -velocity;
          pos = 0;
        }
        else if(pos >= STRIP_LEN-1) {
          velocity = -velocity;
          pos = STRIP_LEN - 1;
        }       
        
        for(int i=0; i<STRIP_LEN; ++i)
          track[i] = RGB(trail[i],0,0);
        trail[(int)pos] = 10;
        track[(int)pos] = 0xfefefe;
        
    }
};


#define BAT_RAISE_SPEED 10
#define BAT_LOWER_SPEED 100

enum {
  BAT_RELEASED,
  BAT_RAISE2,
  BAT_RAISE3,
  BAT_HELD,
  BAT_LOWER3,
  BAT_LOWER2,
  BAT_RELEASED2
};


class CBat 
{
  public:
    unsigned long nextEvent;
    int batPos;
    int batDir;
    byte pin;
    byte state;
    CBat() {
      nextEvent = 0;
      batPos = 0;
      batDir = 0;
      pin = SWITCH_A;
      state = BAT_RELEASED;
    }
    void run(unsigned long milliseconds) {
      switch(state)
      {
         case BAT_RELEASED:
           if(digitalRead(pin) == HIGH)
             break;
           batPos = 49;
           batDir = -1;
           nextEvent = 0;
         case BAT_RAISE2:
         case BAT_RAISE3:         
           if(milliseconds > nextEvent) {
              strip.set(batPos,COL_BATON);             
             state++;
             batPos += batDir;
             nextEvent = milliseconds + BAT_RAISE_SPEED;
           }
           break;
         case BAT_HELD:
           if(digitalRead(pin) == LOW)
             break;
           nextEvent = 0;
         case BAT_LOWER3:
         case BAT_LOWER2:         
           if(milliseconds > nextEvent) {
              batPos -= batDir;
              strip.set(batPos,0);             
             state++;
             nextEvent = milliseconds + BAT_LOWER_SPEED;
           }
           break;
         case BAT_RELEASED2:
              strip.set(49,COL_BATOFF);             
           state = BAT_RELEASED;
           break;
      }      
    }
};

CPulse Pulse[1];
CBat BatA;


int i= 0;
int d = 1;
int q=0;
byte dd =0;
void loop()
{
  unsigned long milliseconds = millis();
    Pulse[0].run(milliseconds);
    strip.set_all(track);
//    BatA.run(milliseconds);      

    
     strip.refresh();
}



