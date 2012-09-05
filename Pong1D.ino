
#include "Arduino.h"
#include "EEPROM.h"
#include "Tone.h"
#include "sd600.h"
#include "HeartBeat.h"
#include "SevenSegs.h"

#define P_LIGHTS1  9
#define P_LIGHTS2  10

// heartbeat LED
#define P_LED 18
CHeartBeat HeartBeat(P_LED);

#define STRIP_LEN 50
sd600 strip(STRIP_LEN);

#define GAME_SERVE_VELOCITY 0.3
#define GAME_ACCELERATION 1.05
#define GAME_MAX_SCORE 15

byte gameScorePlayer1;
byte gameScorePlayer2;
int gameState;
byte gameCurrentServer;
byte gameStarted;
byte gameRallyLen;


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

int SOUND_SWING[] = {
  256, 10,
  512, 10,
  0
};
int SOUND_SERVE[] = {
  256, 10,
  512, 10,
  0
};
int SOUND_RETURN[] = {
  256, 10,
  512, 10,
  0
};
int SOUND_MISS[] = {
  100, 100,
  200, 50,
  100, 100,
  200, 50,
  100, 200,
  0
};
int SOUND_GAME_OVER[] = {
  500, 200,
  400, 200,
  300, 200,
  200, 200,
  100, 200,
  0
};

unsigned long soundNextEvent;
int *soundPos;

void soundInit() 
{
  soundNextEvent = 0;
  soundPos = NULL;
}

void soundStart(int* pos) 
{
    soundPos = pos;
    soundNextEvent = 0;
}

int soundRun(unsigned int milliseconds)
{
  if(milliseconds > soundNextEvent)
  {
    if(soundPos && *soundPos) 
    {
      speaker.play(*soundPos);
      ++soundPos;
      soundNextEvent = milliseconds + *soundPos;
      ++soundPos;
      return 0;
    }
    else if(soundPos)
    {
      speaker.stop();
      soundPos = NULL;
      return 1;
    }
  }
}


// define 7 segment displays
#define P_7SEGS_DAT0    6
#define P_7SEGS_DAT1    5
#define P_7SEGS_SHCLK   7
#define P_7SEGS_STCLK   8
C7Segs SevenSegs(P_7SEGS_SHCLK, P_7SEGS_STCLK, P_7SEGS_DAT0, P_7SEGS_DAT1);



// states of the game
enum {
  STATE_BEFORE_SERVE,
  STATE_ANIM_BEFORE_SERVE,
  STATE_WAIT_FOR_SERVE,
  STATE_PLAY,
  STATE_MISS,
  STATE_ANIM_MISS,
  STATE_GAME_OVER
};

enum {
  EVENT_NONE,
  EVENT_SWING,
  EVENT_HIT,
  EVENT_MISS
};



unsigned long track[STRIP_LEN];
byte pulseTrail[STRIP_LEN];

#define PULSE_TRAIL_INTENSITY 10
#define PULSE_TRAIL_FADE_MS 100
#define PULSE_RUN_MS 10
#define PULSATE_MS 1

float pulsePos;
float pulseVelocity;
unsigned long trailNextEvent;
unsigned long pulseNextEvent;
unsigned long pulseColour;
byte pulsateIntensity = 0;
void pulseInit()
{
  pulsePos = 0;
  pulseVelocity = 0;
  pulseNextEvent = 0;
  trailNextEvent = 0;
  pulseColour = RGB(0xff,0xff,0xff);
}

// Run the fading trail
void trailRun(unsigned long milliseconds)
{
  // fade the trail
  if(milliseconds >= trailNextEvent) 
  {
    for(int i=0; i<STRIP_LEN; ++i) 
      pulseTrail[i] /= 2;
    trailNextEvent = milliseconds + PULSE_TRAIL_FADE_MS;
  }
  for(int i=0; i<STRIP_LEN; ++i)
    track[i] = RGB(pulseTrail[i],0,0);  
}

int pulseRun(unsigned long milliseconds)
{

  int result = EVENT_NONE;        
  if(milliseconds >= pulseNextEvent) 
  {
    pulsePos += pulseVelocity;
    if(pulsePos < 0.0) 
    {
      pulsePos = 0.0;
      result = EVENT_MISS;
    }
    else if(pulsePos >= STRIP_LEN-1) 
    {
      pulsePos = STRIP_LEN - 1;
      result = EVENT_MISS;      
    }       
    pulseNextEvent = milliseconds + PULSE_RUN_MS;
  }        
  pulseTrail[(int)pulsePos] = PULSE_TRAIL_INTENSITY;
  track[(int)pulsePos] = pulseColour;        
  return result;
}

int pulsate(unsigned long milliseconds)
{
  if(milliseconds >= pulseNextEvent)  
  {
    track[(int)pulsePos] =  RGB(pulsateIntensity, pulsateIntensity, pulsateIntensity);        
    pulsateIntensity+=3;  
    pulseNextEvent = milliseconds + PULSATE_MS;
  }
}

#define BAT_SWITCH_A 14
#define BAT_SWITCH_B 15

class CBat
{
public:
  byte startPos;
  byte endPos;
  byte pos;
  byte state;
  byte inputPin;
  unsigned long nextEventTime;
  unsigned long colour;
  
public:
  CBat(byte start, byte end, byte pin, unsigned long col)
  {
    startPos = start;
    endPos = end;
    inputPin = pin;
    colour = col;
  }
};

CBat Player1(0,5,BAT_SWITCH_A,RGB(0,254,0));
CBat Player2(49,44,BAT_SWITCH_B,RGB(0,0,254));

enum {
   BAT_WAIT,
   BAT_RISE,
   BAT_FALL,
   BAT_RELEASE
};


#define BAT_RISE_DELAY 10
#define BAT_FALL_DELAY 50

#define BAT_SIZE 5


void batInit(class CBat& Bat)
{
    pinMode(Bat.inputPin, INPUT);
    digitalWrite(Bat.inputPin, HIGH);
    Bat.pos = Bat.startPos;
    Bat.state = BAT_WAIT;
    Bat.nextEventTime = 0;
}

int batRun(class CBat& Bat, unsigned long milliseconds)
{
  int result = EVENT_NONE;
  switch(Bat.state) 
  {
     case BAT_WAIT:
       if(digitalRead(Bat.inputPin) == HIGH)
         break;   
       result = EVENT_SWING;
       Bat.state = BAT_RISE;
       Bat.nextEventTime = 0;
     case BAT_RISE:
       if(milliseconds < Bat.nextEventTime)
         break;
       if(Bat.endPos > Bat.startPos) // Player 1
       {           
         Bat.pos++;
         if(pulsePos <= Bat.pos && pulseVelocity <= 0)
           result = EVENT_HIT;
       }
       else
       {
         Bat.pos--;
         if(pulsePos >= Bat.pos && pulseVelocity >= 0)
           result = EVENT_HIT;
       }
       Bat.nextEventTime = milliseconds + BAT_RISE_DELAY;
       if(Bat.pos != Bat.endPos)
         break;
       Bat.state = BAT_FALL;
     case BAT_FALL:
       if(milliseconds < Bat.nextEventTime)
         break;
       if(Bat.startPos < Bat.endPos) 
         Bat.pos--;
       else
         Bat.pos++;
       if(Bat.pos != Bat.startPos)
       {
         Bat.nextEventTime = milliseconds + BAT_FALL_DELAY;
         break;
       }
       Bat.state = BAT_RELEASE;           
    case BAT_RELEASE:
      if(digitalRead(Bat.inputPin) == HIGH)
        Bat.state = BAT_WAIT;      
  }
  
  int pos = Bat.startPos;
  for(;;) 
  {      
       track[pos] = Bat.colour;
       if(pos < Bat.pos) 
         pos++;
       else if(pos > Bat.pos)
         pos--;
       else
         break;
  }    
  return result;
}


byte animCount;
unsigned long nextAnimEvent;
void animBeforeServeInit(unsigned long milliseconds) 
{
}
int animBeforeServeRun(unsigned long milliseconds) 
{
  return 1;  
}
void animMissInit() 
{
  animCount = 0;
  nextAnimEvent = 0;
}
int animMissRun(unsigned long milliseconds) 
{
  if(milliseconds >= nextAnimEvent)
  {
    nextAnimEvent = milliseconds + 20;
    if(gameCurrentServer == 1)
    {
      digitalWrite(P_LIGHTS1, 0);
      digitalWrite(P_LIGHTS2, animCount & 8);
    }
    else
    {
      digitalWrite(P_LIGHTS1, animCount & 8);
      digitalWrite(P_LIGHTS2, 0);
    }
    ++animCount;
    if(animCount > 100) 
    {
      digitalWrite(P_LIGHTS1, 0);
      digitalWrite(P_LIGHTS2, 0);
      return 1;
    }
  }
  return 0;
}

void animGameOverInit() 
{
  animCount = 0;
  nextAnimEvent = 0;
}
int animGameOverRun(unsigned long milliseconds) 
{
  if(milliseconds >= nextAnimEvent)
  {
    nextAnimEvent = milliseconds + 1;
    if((animCount & 0xc0) == 0)
    {
      SevenSegs.showRaw(0, 0, 0);
      SevenSegs.showRaw(1, 0, 0);          
    }
    else
    {
      SevenSegs.showNumber(0, gameScorePlayer1);
      SevenSegs.showNumber(1, gameScorePlayer2);      
    }  
    digitalWrite(P_LIGHTS1, !(animCount & 0x10));
    digitalWrite(P_LIGHTS2, !!(animCount & 0x10));    
    animCount++;
  }
  return 0;
}


#define ATTRACT_CYCLE_MS 10
unsigned long attractNextEvent;
byte attractCount;
void attractInit() 
{
  attractNextEvent = 0;
  attractCount = 0;
}
void attractRun(unsigned long milliseconds) 
{
  if(milliseconds < attractNextEvent)
    return;
  attractNextEvent = milliseconds + ATTRACT_CYCLE_MS;
  if((attractCount & 0x80) == 0x00)
  {
    SevenSegs.showRaw(1, 0, SEG_B|SEG_C);
    SevenSegs.showRaw(0, SEG_B|SEG_C|SEG_D|SEG_E|SEG_G, 0);
  }
  else 
  if((attractCount & 0x80) == 0x80)
  {
    SevenSegs.showRaw(1, SEG_A|SEG_B|SEG_E|SEG_F|SEG_G, SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F);
    SevenSegs.showRaw(0, SEG_A|SEG_B|SEG_C|SEG_E|SEG_F, SEG_A|SEG_C|SEG_D|SEG_E|SEG_F);
  }
  analogWrite(P_LIGHTS1, attractCount<<3);
  analogWrite(P_LIGHTS2, attractCount<<3);
  attractCount++;  
}


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
// PLAYER 1
//      low numbered LED location
//      GREEN paddle
//      switch locatated
//
//
//
///////////////////////////////////////////////////////////////////////


void setup()
{
  
  strip.begin();
  speaker.begin(19);
  speaker.stop();
  soundInit() ;
  pulseInit() ;
  attractInit();
  batInit(Player1);
  batInit(Player2);
  speaker.play(1000, 5);

  pinMode(P_LIGHTS1, OUTPUT);
  pinMode(P_LIGHTS2, OUTPUT);  
  digitalWrite(P_LIGHTS1, LOW);
  digitalWrite(P_LIGHTS2, LOW);
  
  // ensure the random number generation does not 
  // repeat on every reset!
  randomSeed(EEPROM.read(0));
  EEPROM.write(0,random(256));  
  
  gameCurrentServer = (random(100)>=50) ? 2 : 1;    
  gameState = STATE_BEFORE_SERVE;
  gameScorePlayer1 = 0;
  gameScorePlayer2 = 0;  
  gameStarted = 0;

  
}

void loop()
{
  unsigned long milliseconds = millis();
  int player1event;
  int player2event;
  int pulseEvent;
  
  memset(track, 0, sizeof(track));
  
  switch(gameState)
  {  
    case STATE_BEFORE_SERVE:
      memset(pulseTrail, 0, sizeof(pulseTrail));
      if(gameStarted) 
      {
        SevenSegs.showNumber(0, gameScorePlayer1);
        SevenSegs.showNumber(1, gameScorePlayer2);
      }
      pulsePos = (gameCurrentServer == 1)? 1 : 48;
      pulseVelocity = 0;
      animBeforeServeInit(milliseconds);
      gameState = STATE_ANIM_BEFORE_SERVE;
      // fallthru
    case STATE_ANIM_BEFORE_SERVE:
      if(!animBeforeServeRun(milliseconds))
        break;
      pulsateIntensity = 0;
      gameState = STATE_WAIT_FOR_SERVE;
      // fallthru
    case STATE_WAIT_FOR_SERVE:
      if(!gameStarted)
        attractRun(milliseconds);
      pulsate(milliseconds);
      player1event = batRun(Player1, milliseconds);
      player2event = batRun(Player2, milliseconds);
      if(player1event != EVENT_HIT && player2event != EVENT_HIT)
        break;
      pulseVelocity = (gameCurrentServer == 1)? GAME_SERVE_VELOCITY : -GAME_SERVE_VELOCITY;
      soundStart(SOUND_SERVE);
      gameState = STATE_PLAY;
      gameStarted = 1;
      gameRallyLen = 0;
      digitalWrite(P_LIGHTS1, LOW);
      digitalWrite(P_LIGHTS2, LOW);
      SevenSegs.showNumber(0, gameScorePlayer1);
      SevenSegs.showNumber(1, gameScorePlayer2);      
      // fallthru
    case STATE_PLAY:    
      trailRun(milliseconds);
      player1event = batRun(Player1, milliseconds);
      player2event = batRun(Player2, milliseconds);
      if(player1event == EVENT_HIT || player2event == EVENT_HIT)
      {
        gameRallyLen++;
        SevenSegs.showNumberDash(1, gameRallyLen/10);
        SevenSegs.showNumberDash(0, gameRallyLen%10);      
        soundStart(SOUND_RETURN);  
        pulseVelocity = -GAME_ACCELERATION * pulseVelocity;
      }
      pulseEvent = pulseRun(milliseconds);
      if(pulseEvent != EVENT_MISS)
      {
        if(player1event == EVENT_SWING || player2event == EVENT_SWING) 
          soundStart(SOUND_SWING);  
        break;
      }
      soundStart(SOUND_MISS);  
      gameState = STATE_MISS;
      // fallthru
    case STATE_MISS:
      animMissInit();
      gameState = STATE_ANIM_MISS;
      // fallthru
    case STATE_ANIM_MISS:
      if(!animMissRun(milliseconds))
        break;
      SevenSegs.showNumber(0, gameScorePlayer1);
      SevenSegs.showNumber(1, gameScorePlayer2);
      delay(200);
      if(pulseVelocity < 0)
      {
        ++gameScorePlayer2;
        SevenSegs.showNumber(1, gameScorePlayer2);
        gameCurrentServer = 2;
      }
      else
      {
        ++gameScorePlayer1;
        SevenSegs.showNumber(0, gameScorePlayer1);
        gameCurrentServer = 1;
      }                
      if(gameScorePlayer2 < GAME_MAX_SCORE && gameScorePlayer1 < GAME_MAX_SCORE)
      {
        gameState = STATE_BEFORE_SERVE;
        break;
      }
      gameState = STATE_GAME_OVER;
      soundStart(SOUND_GAME_OVER);        
      animGameOverInit();
      // fallthru
    case STATE_GAME_OVER:
      animGameOverRun(milliseconds);
      break;
  }
  
  soundRun(milliseconds);
  HeartBeat.run(milliseconds);    
  strip.set_all(track);
  strip.refresh();
}
