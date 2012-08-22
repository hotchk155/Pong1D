#include "sd600.h"
#include "Tone.h"

void gameEvent(int event);



#define STRIP_LEN 50
sd600 strip(STRIP_LEN);
unsigned long track[STRIP_LEN];

#define VEL_1  0.2
#define VEL_2  0.4
#define VEL_3  0.6
#define VEL_4  0.8
#define VEL_5  1.0

enum {
  GAME_BEGIN,
  GAME_STARTROUND,
  GAME_PLAYING,
  GAME_PLAYER1DIE,
  GAME_PLAYER2DIE,
  GAME_WON,
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
    float pos;
    float velocity;
    unsigned long colour;
    unsigned long nextFadeTime;
    
    CPulse() {
      pos = 0;
      velocity = VEL_1;
      colour = 0xfffffeL;
      memset(trail,0,sizeof(trail));
      reset(random(10) > random(10));
    }
    void reset(boolean atEnd) {
      if(atEnd)
      {
        pos = 49;
        velocity = -VEL_1;      
      }
      else
      {
        pos = 0;
        velocity = VEL_1;      
      }
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
          gameEvent(GAME_PLAYER1DIE);
          velocity=-velocity;
          pos = 0;
        }
        else if(pos >= STRIP_LEN-1) {
          gameEvent(GAME_PLAYER2DIE);
          pos = STRIP_LEN - 1;
          velocity=-velocity;
        }       
        
        for(int i=0; i<STRIP_LEN; ++i)
          track[i] = RGB(trail[i],0,0);
        trail[(int)pos] = 10;
        track[(int)pos] = 0xfefefe;        
    }

    int intPos() {
      return (int)(pos + 0.5);
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
   B_NONE,
   B_1UP,
   B_2UP,
   B_3UP,
   B_4UP,
   B_5UP,
   B_5DN,
   B_4DN,
   B_3DN,
   B_2DN,
   B_1DN,
   B_0DN
};

#define SWITCH_A 15
#define SWITCH_B 14

#define BAT_RISE_DELAY 10
#define BAT_FALL_DELAY 50

class CBat {
  byte m_pos1;
  byte m_pos2;
  byte m_pos3;
  byte m_pos4;
  byte m_pos5;
  byte m_state;
  byte m_pin;
  unsigned long m_nextEvent;
  unsigned long m_colour;
public:
  CBat(byte pos1, byte pos2, byte pos3, byte pos4, byte pos5, byte pin, unsigned long colour)
  {
    m_pos1 = pos1;
    m_pos2 = pos2;
    m_pos3 = pos3;
    m_pos4 = pos4;
    m_pos5 = pos5;
    m_state = B_NONE;
    m_pin = pin;
    m_nextEvent = 0;
    m_colour = colour;
    pinMode(m_pin, INPUT);
    digitalWrite(m_pin, HIGH);
  }
  
  void run(unsigned long milliseconds, CPulse *pPulse)
  {
    int pos;
    switch(m_state) 
    {
       case B_NONE:
         if(digitalRead(m_pin) == HIGH)
           break;           
         pos = pPulse->intPos();
         
         if(pos == m_pos1 || pos == m_pos2 || pos == m_pos3 || pos == m_pos4 || pos == m_pos5)
         {
           Sounds.begin(sound_bat);
           pPulse->velocity *= -1.05;
         }
         m_nextEvent = 0;
       case B_1UP:
       case B_2UP:
       case B_3UP:
       case B_4UP:
       case B_5UP:
         if(milliseconds < m_nextEvent)
           break;
         m_nextEvent = milliseconds + BAT_RISE_DELAY;
         m_state++;
         break;                 
       case B_5DN:
       case B_4DN:
       case B_3DN:
       case B_2DN:
       case B_1DN:
         if(milliseconds < m_nextEvent)
           break;
         m_nextEvent = milliseconds + BAT_FALL_DELAY;
         ++m_state;
         break;
       case B_0DN:
         if(digitalRead(m_pin) == LOW)
           break;           
         m_state = B_NONE;      

    }
    
    switch(m_state)
    {
       case B_5UP:
       case B_5DN:
         track[m_pos5] = m_colour;
       case B_4UP:
       case B_4DN:
         track[m_pos4] = m_colour;
       case B_3UP:
       case B_3DN:
         track[m_pos3] = m_colour;
       case B_2UP:
       case B_2DN:
         track[m_pos2] = m_colour;
       case B_1UP:
       case B_1DN:
         track[m_pos1] = m_colour;
    }
  }
};
CBat BatA(49,48,47,46,45,SWITCH_A,RGB(0,0,254));
CBat BatB(0,1,2,3,4,SWITCH_B,RGB(0,254,0));

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
void gameEvent(int event)
{
  gameState = event;
}

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
//  Serial.begin(9600);
//  SD600.setLed(0, RGB(255,0,0));

int q = 0;
/*for(;;)
{
speaker.play(1400,50);
  SevenSegs.showNumber(0, q++);
  SevenSegs.showNumber(1, 99-q);
  delay(1000);  
}*/
  Player1Score = 0;
  Player2Score = 0;
  refreshScores();
//Sounds.begin(sound1);
}

#define MAX_SCORE 15
unsigned long gameDelay = 0;
void loop()
{
  memset(track, 0, sizeof(track));
  unsigned long milliseconds = millis();

  if(milliseconds > gameDelay)
  {
    switch(gameState)
    {
      case GAME_BEGIN:
        gameState = GAME_STARTROUND;
        break;
      case GAME_STARTROUND:
        gameState = GAME_PLAYING;
        break;
      case GAME_PLAYING:
        Pulse.run(milliseconds);
        BatA.run(milliseconds, &Pulse);      
        BatB.run(milliseconds, &Pulse);      
        break;
      case GAME_PLAYER1DIE:
        Player2Score++;    
        refreshScores();
        if(Player2Score >= MAX_SCORE)
          gameState = GAME_WON;
        else      
          gameState = GAME_STARTROUND;
        Sounds.begin(sound_die);
        gameDelay = milliseconds + 2000;
        Pulse.reset(true);
        break;
      case GAME_PLAYER2DIE:
        Player1Score++;    
        refreshScores();
        if(Player1Score >= MAX_SCORE)
          gameState = GAME_WON;
        else      
          gameState = GAME_STARTROUND;
        Sounds.begin(sound_die);
        gameDelay = milliseconds + 2000;
        Pulse.reset(false);
        break;
      case GAME_WON:
        break;
    }
  }

  Sounds.run(milliseconds);
  HeartBeat.run(milliseconds);    
  strip.set_all(track);
  strip.refresh();
}



