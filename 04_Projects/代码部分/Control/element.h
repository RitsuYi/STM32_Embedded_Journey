#ifndef _ELEMENT_H__
#define _ELEMENT_H__

void Element_Normal(void);
void Element_Process(void);
void Element_Ring(void);
void Element_Noline(void);
void Element_Stop(void);
void Element_Corner(void);
void Element_Ten(void);

extern float kL,kR;
extern uint8_t Element_Flag,Ring_Flag,Noline_Flag,Stop_Flag,Ten_Flag;
#endif
