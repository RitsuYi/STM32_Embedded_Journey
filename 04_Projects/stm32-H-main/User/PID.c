#include "stm32f10x.h"                  // Device header
#include "PID.h"

void PID_Init(PID_t*p)
{
	p->Target=0;
	p->Actual=0;
	p->Out=0;
	p->Error0=0;
	p->Error1=0;
	p->ErrorInt=0;
	p->IntegralMax=0;
	p->IntegralMin=0;
	p->LastOut=0;
	p->OutMax=0;
	p->OutMin=0;
}


void PID_Update(PID_t *p)
{
	float errorIntCandidate;

	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;
	
	if (p->Ki != 0)
	{
		errorIntCandidate = p->ErrorInt + p->Error0;
		if (errorIntCandidate > p->IntegralMax) {errorIntCandidate = p->IntegralMax;}
		if (errorIntCandidate < p->IntegralMin) {errorIntCandidate = p->IntegralMin;}
		p->ErrorInt = errorIntCandidate;
	}
	else
	{
		p->ErrorInt = 0;
	}
	
	p->Out = p->Kp * p->Error0
		   + p->Ki * p->ErrorInt
		   + p->Kd * (p->Error0 - p->Error1);
	
	if (p->Out > p->OutMax)
	{
		p->Out = p->OutMax;
		if (p->Error0 > 0 && p->Ki != 0)
		{
			p->ErrorInt -= p->Error0;
		}
	}
	if (p->Out < p->OutMin)
	{
		p->Out = p->OutMin;
		if (p->Error0 < 0 && p->Ki != 0)
		{
			p->ErrorInt -= p->Error0;
		}
	}
	
	if (p->ErrorInt > p->IntegralMax) {p->ErrorInt = p->IntegralMax;}
	if (p->ErrorInt < p->IntegralMin) {p->ErrorInt = p->IntegralMin;}
	p->LastOut = p->Out;
}
