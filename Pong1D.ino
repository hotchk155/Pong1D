#include "sd600.h"
#include "Tone.h"

void gameEvent(int event);



#define STRIP_LEN 50
#define POS_A 0
#define POS_B 49
sd600 strip(STRIP_LEN);
unsigned long track[STRIP_LEN];

#define SERVE_VELOCITY  0.2

enum {
  GAME_BEGIN,
  GAME_SERVEA,
  GAME_SERVEB,
  GAME_RUNNING,
  GAME_OVER
};

enum {
  EVT_SWINGA,
  EVT_SWINGB,
  EVT_HITA,
  EVT_HITB,
  EVT_MISSA,
  EVT_MISSB
};
///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// SEVEN SEGMENT DISPLAYS
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////

#define P_7SEGS_DAT0    6
#define P_7SEGS_DAT1    5
#define P_7SEGS_SHCLK   7
#define P_7SEGS_STCLK   8

/*
   +-a-+
  f|   |b
   +-g-+
  e|   |c
   +-d-+.dp
*/
#define SEG_A    0x40
#define SEG_B    0x80
#define SEG_C    0x04
#define SEG_D    0x02
#define SEG_E    0x01
#define SEG_F    0x10
#define SEG_G    0x08
#define SEG_DP   0x20

#define DIGIT_0 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F)
#define DIGIT_1 (SEG_B|SEG_C)
#define DIGIT_2 (SEG_A|SEG_B|SEG_G|SEG_E|SEG_D)
#define DIGIT_3 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_G)
#define DIGIT_4 (SEG_B|SEG_C|SEG_F|SEG_G)
#define DIGIT_5 (SEG_A|SEG_C|SEG_D|SEG_F|SEG_G)
#define DIGIT_6 (SEG_A|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G)
#define DIGIT_7 (SEG_A|SEG_B|SEG_C)
#define DIGIT_8 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G)
#define DIGIT_9 (SEG_A|SEG_B|SEG_C|SEG_D|SEG_F|SEG_G)

class C7Segs
{
  byte m_shclk;
  byte m_stclk; 
  byte m_dat0;
  byte m_dat1;
  
  unsigned int  m_leds0;
  unsigned int  m_leds1;
public:  
  C7Segs(byte shclk, byte stclk, byte dat0, byte dat1) {
    m_shclk = shclk;
    m_stclk = stclk;
    m_dat0 = dat0;
    m_dat1 = dat1;
    
    pinMode(shclk, OUTPUT);
    pinMode(stclk, OUTPUT);
    pinMode(dat0, OUTPUT);
    pinMode(dat1, OUTPUT);
    
    m_leds0 = 0;
    m_leds1 = 0;    
    refresh();
  }
  
  void refresh() {
    digitalWrite(m_stclk, LOW);  
    unsigned int  m = 0x8000;
    while(m > 0)
    {
      digitalWrite(m_shclk, LOW);  
      digitalWrite(m_dat0, !!(m_leds0 & m));  
      digitalWrite(m_dat1, !!(m_leds1 & m));  
      digitalWrite(m_shclk, HIGH);        
      m>>=1;
    }
    digitalWrite(m_stclk, HIGH);  
  }
  
  void showRaw(byte which, byte left, byte right) {
    unsigned int value = ((unsigned int)left<<8)|right;
    if(which) 
      m_leds1 = value;
    else
      m_leds0 = value;
    refresh();
  }
  
  byte digitLookup(byte digit) {
    switch(digit) {
      case 0: return DIGIT_0;
      case 1: return DIGIT_1;
      case 2: return DIGIT_2;
      case 3: return DIGIT_3;
      case 4: return DIGIT_4;
      case 5: return DIGIT_5;
      case 6: return DIGIT_6;
      case 7: return DIGIT_7;
      case 8: return DIGIT_8;
      case 9: return DIGIT_9;
    }
    return 0;
  }
  void showNumber(byte which, int value) {
    value %= 100;
    showRaw(which, digitLookup(value/10), digitLookup(value%10));
  } 
};
C7Segs SevenSegs(P_7SEGS_SHCLK, P_7SEGS_STCLK, P_7SEGS_DAT0, P_7SEGS_DAT1);


///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// SOUNDS
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////
Tone speaker;

int sound1[] = {
  256, 100,
  512, 100,
  256, 100,
  512, 100,
  256, 100,
  512, 100,
  256, 100,
  512, 100,
  0
};

int sound_bat[] = {
  256, 10,
  512, 10,
  0
};

int sound_die[] = {
  500, 200,
  400, 200,
  300, 200,
  200, 200,
  100, 200,
  0
};

class CSound 
{
  unsigned long nextEvent;
  int *pos;
 public:
  CSound() {
    pos = 0;
    nextEvent = 0;
  }
  void begin(int* sound) {
    pos = sound;
    nextEvent = 0;
  }
  void run(unsigned int milliseconds)
  {
    if(milliseconds > nextEvent)
    {
      if(pos && *pos) 
      {
        speaker.play(*pos);
        ++pos;
        nextEvent = milliseconds + *pos;
        ++pos;
      }
      else if(pos)
      {
        speaker.stop();
        pos = 0;
      }
    }
  }   
};
CSound Sounds;


///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// PULSE
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////
#define FADE_PERIOD 100
class CPulse 
{
  byte trail[STRIP_LEN];
  
  public:
    float m_pos;
    float m_velocity;
    unsigned long m_nextFadeTime;
    
    CPulse() {
      m_pos = 0;
      m_velocity = 0;
      memset(trail,0,sizeof(trail));
      m_nextFadeTime = 0;
    }
    void init(byte newPos, float newVelocity) 
    {
        memset(trail, 0, sizeof(trail));
        m_pos = newPos;
        m_velocity = newVelocity;      
    }
    void reverse(float factor) 
    {
      m_velocity *= factor;
    }
    void pulsate(byte pos, unsigned long milliseconds)
    {
        if(milliseconds > m_nextFadeTime) 
        {
          byte q = 128.0 + 126.0 * sin(m_pos);
          track[(int)pos] = RGB(q,q,q);              
          m_pos += 0.1;
          m_nextFadeTime = milliseconds + 10;
        }
    }
    void run(unsigned long milliseconds)
    {
        if(milliseconds > m_nextFadeTime) 
        {
          for(int i=0; i<STRIP_LEN; ++i) {
            trail[i] /= 2;
          }
          m_nextFadeTime = milliseconds + FADE_PERIOD;
        }
        
        int oldPos = (m_pos + 0.5);
        m_pos += m_velocity;
        if(m_pos < 0) {
reverse(-1);
//          gameEvent(EVT_MISSB);
          m_pos = 0;
        }
        else if(m_pos >= STRIP_LEN-1) {
          gameEvent(EVT_MISSA);
          m_pos = STRIP_LEN - 1;
        }       
        
        for(int i=0; i<STRIP_LEN; ++i)
          track[i] = RGB(trail[i],0,0);
        trail[(int)m_pos] = 10;
        track[(int)m_pos] = 0xfefefeL;        
    }

    int intPos() {
      return (int)(m_pos + 0.5);
    }    
};
CPulse Pulse;

///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// BATS
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////

enum {
   BAT_WAIT,
   BAT_RISE,
   BAT_FALL,
   BAT_RELEASE
};

#define SWITCH_A 14
#define SWITCH_B 15

#define BAT_RISE_DELAY 10
#define BAT_FALL_DELAY 50

#define BAT_SIZE 5

class CBat {
  byte m_start;
  byte m_end;
  byte m_state;
  byte m_pin;
  byte m_pos;
  unsigned long m_nextEvent;
  unsigned long m_colour;
public:
  CBat(byte startPos, byte endPos, byte inputPin, unsigned long colour)
  {
    m_start = startPos;
    m_end = endPos;
    m_pos = startPos;
    m_state = BAT_WAIT;
    m_pin = inputPin;
    m_nextEvent = 0;
    m_colour = colour;
    pinMode(m_pin, INPUT);
    digitalWrite(m_pin, HIGH);
  }
  
  void run(byte which, unsigned long milliseconds, CPulse *pPulse)
  {
    int pulsePos = pPulse->intPos();
    switch(m_state) 
    {
       case BAT_WAIT:
         if(digitalRead(m_pin) == HIGH)
           break;   
         gameEvent(which? EVT_SWINGA : EVT_SWINGB);
         m_state = BAT_RISE;
       case BAT_RISE:
         if(milliseconds < m_nextEvent)
           break;
         if(m_end > m_pos) {
           m_pos++;
         } else {
           m_pos--;
         }
         if(which)
         {
           if(pulsePos <= m_end)         
             gameEvent(EVT_HITB);
         }
         else 
         {
           if(pulsePos >= m_end)         
             gameEvent(EVT_HITA);
         }
         if(m_pos == m_end)
           m_state = BAT_FALL;           
         m_nextEvent = milliseconds + BAT_RISE_DELAY;
         break;
       case BAT_FALL:
         if(milliseconds < m_nextEvent)
           break;
         if(m_start < m_pos) {
           m_pos--;
         } else {
           m_pos++;
         }
         if(m_pos == m_start)
           m_state = BAT_RELEASE;           
         m_nextEvent = milliseconds + BAT_FALL_DELAY;
         break;
      case BAT_RELEASE:
        if(digitalRead(m_pin) == HIGH)
          m_state = BAT_WAIT;      
    }
    
    int pos = m_start;
    for(;;) {      
         track[pos] = m_colour;
         if(pos < m_pos) 
           pos++;
         else if(pos > m_pos)
           pos--;
         else
           break;
    }
  }
};
CBat Bat0(0,5,SWITCH_A,RGB(0,254,0));
CBat Bat1(49,44,SWITCH_B,RGB(0,0,254));

///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// HEARTBEAT
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////
#define P_LED 18
class CHeartBeat {
  byte m_pin;
  byte m_state;
  unsigned long m_nextUpdate;
public:
  CHeartBeat(byte pin) {
    m_pin = pin;
    m_state = LOW;
    m_nextUpdate = 0;
    pinMode(pin,OUTPUT);
  }
  void run(unsigned long milliseconds) {
    if(milliseconds > m_nextUpdate) {
      m_nextUpdate = milliseconds + 500;
      m_state = !m_state;
      digitalWrite(m_pin,m_state);
    }
  }
};
CHeartBeat HeartBeat(P_LED);


///////////////////////////////////////////////////////////////////////
//
//
//
//
//
//
// ENTRY POINTS
//
//
//
//
//
//
///////////////////////////////////////////////////////////////////////


#define MAX_SCORE 15
byte Player1Score = 0;
byte Player2Score = 0;

int i= 0;
int d = 1;
int q=0;
byte dd =0;

int gameState = 0;

void refreshScores()
{
  SevenSegs.showNumber(0, Player1Score);
  SevenSegs.showNumber(1, Player2Score);
}

void setup()
{
  pinMode(SWITCH_A,INPUT);
  digitalWrite(SWITCH_A,HIGH);
  pinMode(P_LED,OUTPUT);
  digitalWrite(P_LED,LOW);  
  delay(10);
  strip.begin();
  speaker.begin(19);
}

#define MAX_SCORE 15
unsigned long gameDelay = 0;

void gameEvent(int event)
{
  switch(event) {
    case EVT_SWINGA:
      if(gameState == GAME_SERVEA) {
        Sounds.begin(sound_bat);
        Pulse.init(48, -SERVE_VELOCITY);
        gameState = GAME_RUNNING;
      }
      break;
    case EVT_SWINGB:
      if(gameState == GAME_SERVEB) {
        Sounds.begin(sound_bat);
        Pulse.init(1, SERVE_VELOCITY);
        gameState = GAME_RUNNING;
      }      
      break;
    case EVT_HITA:
    case EVT_HITB:
      Sounds.begin(sound_bat);
      Pulse.reverse(1.05);
      break;
    case EVT_MISSA:
        Player2Score++;    
        refreshScores();
        if(Player2Score >= MAX_SCORE)
          gameState = GAME_OVER;
        else      
          gameState = GAME_SERVEA;//B
        Sounds.begin(sound_die);
        break;
    case EVT_MISSB:
        Player1Score++;    
        refreshScores();
        if(Player1Score >= MAX_SCORE)
          gameState = GAME_OVER;
        else      
          gameState = GAME_SERVEA;
        Sounds.begin(sound_die);
        break;
  }
}

void loop()
{
  memset(track, 0, sizeof(track));
  unsigned long milliseconds = millis();

  if(milliseconds > gameDelay)
  {
    switch(gameState)
    {
      // initial state on reset
      case GAME_BEGIN:
        Player1Score = 0;
        Player2Score = 0;
        refreshScores();
        gameState = GAME_SERVEA;        
        break;
      
      // state just before play starts
      case GAME_SERVEA:
        Bat0.run(0, milliseconds, &Pulse);      
        Bat1.run(1, milliseconds, &Pulse);      
        Pulse.pulsate(48, milliseconds);
        break;        
      case GAME_SERVEB:
        Bat0.run(0, milliseconds, &Pulse);      
        Bat1.run(1, milliseconds, &Pulse);      
        Pulse.pulsate(1, milliseconds);
        break;        
      case GAME_RUNNING:
        Pulse.run(milliseconds);
        Bat0.run(0, milliseconds, &Pulse);      
        Bat1.run(1, milliseconds, &Pulse);      
        break;
      case GAME_OVER:
        break;
    }
  }

  Sounds.run(milliseconds);
  HeartBeat.run(milliseconds);    
  strip.set_all(track);
  strip.refresh();
}



