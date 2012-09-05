#ifndef __SEVENSEGS_H__
#define __SEVENSEGS_H__

// 
//    +-a-+
//   f|   |b
//    +-g-+
//   e|   |c
//    +-d-+.dp
// 
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
  C7Segs(byte shclk, byte stclk, byte dat0, byte dat1) 
  {
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
  
  void refresh() 
  {
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
  
  void showRaw(byte which, byte left, byte right) 
  {
    unsigned int value = ((unsigned int)left<<8)|right;
    if(which) 
      m_leds1 = value;
    else
      m_leds0 = value;
    refresh();
  }
  
  byte digitLookup(byte digit) 
  {
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
  void showNumber(byte which, int value) 
  {
    value %= 100;
    showRaw(which, digitLookup(value/10), digitLookup(value%10));
  } 
  void showNumberDash(byte which, int value) 
  {
    byte c1 = value/10;
    byte c2 = value%10;
    if(c2) c2 = digitLookup(c2); else c2 = c1? DIGIT_0 : SEG_G;
    if(c1) c1 = digitLookup(c1); else c1 = SEG_G;
    showRaw(which, c1, c2);
  } 
};

#endif //  __SEVENSEGS_H__
