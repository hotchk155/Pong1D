#ifndef __HEARTBEAT_H__
#define __HEARTBEAT_H__

class CHeartBeat {
  byte m_pin;
  byte m_state;
  unsigned long m_nextUpdate;
public:
  CHeartBeat(byte pin) 
  {
    m_pin = pin;
    m_state = LOW;
    m_nextUpdate = 0;
    pinMode(pin,OUTPUT);
  }
  void run(unsigned long milliseconds) 
  {
    if(milliseconds > m_nextUpdate) 
    {
      m_nextUpdate = milliseconds + 500;
      m_state = !m_state;
      digitalWrite(m_pin,m_state);
    }
  }
};
#endif // __HEARTBEAT_H__
