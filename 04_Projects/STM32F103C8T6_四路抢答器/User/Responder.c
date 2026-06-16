#include "stm32f10x.h"
#include "Responder.h"
#include "OLED.h"
#include "Key.h"
#include "LED.h"
#include "Buzzer.h"

#define RESPONDER_START_HALF_SECONDS    20
#define RESPONDER_HALF_SECOND_MS        500
#define RESPONDER_ALARM_BLINK_MS        250

typedef enum
{
	RESPONDER_STATE_IDLE = 0,
	RESPONDER_STATE_COUNTDOWN,
	RESPONDER_STATE_SUCCESS,
	RESPONDER_STATE_TIMEOUT
} ResponderState_t;

static ResponderState_t ResponderState;
static uint8_t CountdownHalfSeconds;
static uint8_t WinnerTeam;
static uint32_t LastCountdownTime;
static uint32_t LastBlinkTime;

static uint8_t Responder_TimeElapsed(uint32_t nowMs, uint32_t lastMs, uint32_t intervalMs)
{
	return ((uint32_t)(nowMs - lastMs) >= intervalMs) ? 1 : 0;
}

static void Responder_ShowLine(uint8_t line, const char *text)
{
	char buffer[17];
	uint8_t i;
	
	for (i = 0; i < 16 && text[i] != '\0'; i++)
	{
		buffer[i] = text[i];
	}
	while (i < 16)
	{
		buffer[i++] = ' ';
	}
	buffer[16] = '\0';
	
	OLED_ShowString(line, 1, buffer);
}

static void Responder_ShowIdle(void)
{
	OLED_Clear();
	Responder_ShowLine(1, "Responder");
	Responder_ShowLine(2, "Ready");
	Responder_ShowLine(3, "Press Start");
	Responder_ShowLine(4, "PA0:Start PA1:R");
}

static void Responder_MakeTimeLine(char line[17], uint8_t halfSeconds)
{
	uint8_t seconds;
	uint8_t decimal;
	uint8_t i;
	
	for (i = 0; i < 16; i++)
	{
		line[i] = ' ';
	}
	line[16] = '\0';
	
	seconds = halfSeconds / 2;
	decimal = (halfSeconds % 2) ? 5 : 0;
	
	line[0] = 'T';
	line[1] = 'i';
	line[2] = 'm';
	line[3] = 'e';
	line[4] = ':';
	line[5] = ' ';
	if (seconds >= 10)
	{
		line[6] = (char)('0' + (seconds / 10));
		line[7] = (char)('0' + (seconds % 10));
	}
	else
	{
		line[6] = ' ';
		line[7] = (char)('0' + seconds);
	}
	line[8] = '.';
	line[9] = (char)('0' + decimal);
	line[10] = ' ';
	line[11] = 's';
}

static void Responder_ShowCountdown(void)
{
	char line[17];
	
	Responder_ShowLine(1, "Please Answer");
	Responder_MakeTimeLine(line, CountdownHalfSeconds);
	Responder_ShowLine(2, line);
	Responder_ShowLine(3, "K1 K2 K3 K4");
	Responder_ShowLine(4, "Waiting...");
}

static void Responder_ShowSuccess(void)
{
	char line[17];
	uint8_t i;
	
	for (i = 0; i < 16; i++)
	{
		line[i] = ' ';
	}
	line[16] = '\0';
	
	line[0] = 'T';
	line[1] = 'e';
	line[2] = 'a';
	line[3] = 'm';
	line[4] = ' ';
	line[5] = (char)('0' + WinnerTeam);
	line[6] = ' ';
	line[7] = 'W';
	line[8] = 'i';
	line[9] = 'n';
	
	OLED_Clear();
	Responder_ShowLine(1, line);
	Responder_ShowLine(2, "Success");
	Responder_ShowLine(3, "Lock Others");
	Responder_ShowLine(4, "Press Reset");
}

static void Responder_ShowTimeout(void)
{
	OLED_Clear();
	Responder_ShowLine(1, "Game over!");
	Responder_ShowLine(2, "No Answer");
	Responder_ShowLine(3, "Timeout");
	Responder_ShowLine(4, "Press Reset");
}

static uint8_t Responder_GetTeamEvent(uint8_t events)
{
	if (events & KEY_EVENT_TEAM1)
	{
		return 1;
	}
	if (events & KEY_EVENT_TEAM2)
	{
		return 2;
	}
	if (events & KEY_EVENT_TEAM3)
	{
		return 3;
	}
	if (events & KEY_EVENT_TEAM4)
	{
		return 4;
	}
	
	return 0;
}

static void Responder_EnterIdle(void)
{
	ResponderState = RESPONDER_STATE_IDLE;
	CountdownHalfSeconds = 0;
	WinnerTeam = 0;
	Buzzer_Stop();
	LED_AllOff();
	Responder_ShowIdle();
}

static void Responder_EnterCountdown(uint32_t nowMs)
{
	ResponderState = RESPONDER_STATE_COUNTDOWN;
	CountdownHalfSeconds = RESPONDER_START_HALF_SECONDS;
	WinnerTeam = 0;
	LastCountdownTime = nowMs;
	LED_AllOff();
	LED_RunOn();
	Responder_ShowCountdown();
}

static void Responder_EnterSuccess(uint8_t team)
{
	ResponderState = RESPONDER_STATE_SUCCESS;
	WinnerTeam = team;
	LED_RunOff();
	LED_SetWinner(team);
	Buzzer_PlaySuccess();
	Responder_ShowSuccess();
}

static void Responder_EnterTimeout(uint32_t nowMs)
{
	ResponderState = RESPONDER_STATE_TIMEOUT;
	WinnerTeam = 0;
	LastBlinkTime = nowMs;
	LED_RunOff();
	LED_TeamAllOff();
	LED_AlarmOn();
	Buzzer_PlayAlarm();
	Responder_ShowTimeout();
}

static void Responder_HandleCountdown(uint32_t nowMs, uint8_t events)
{
	uint8_t team;
	
	team = Responder_GetTeamEvent(events);
	if (team != 0 && CountdownHalfSeconds > 0)
	{
		Responder_EnterSuccess(team);
		return;
	}
	
	while (ResponderState == RESPONDER_STATE_COUNTDOWN &&
	       Responder_TimeElapsed(nowMs, LastCountdownTime, RESPONDER_HALF_SECOND_MS))
	{
		LastCountdownTime += RESPONDER_HALF_SECOND_MS;
		if (CountdownHalfSeconds > 0)
		{
			CountdownHalfSeconds--;
		}
		
		if (CountdownHalfSeconds == 0)
		{
			Responder_EnterTimeout(nowMs);
		}
		else
		{
			Responder_ShowCountdown();
		}
	}
}

void Responder_Init(void)
{
	Responder_EnterIdle();
}

void Responder_Update(uint32_t nowMs)
{
	uint8_t events;
	
	events = Key_GetEvents();
	
	if (events & KEY_EVENT_RESET)
	{
		Responder_EnterIdle();
		return;
	}
	
	switch (ResponderState)
	{
		case RESPONDER_STATE_IDLE:
			if (events & KEY_EVENT_START)
			{
				Responder_EnterCountdown(nowMs);
			}
			break;
		
		case RESPONDER_STATE_COUNTDOWN:
			Responder_HandleCountdown(nowMs, events);
			break;
		
		case RESPONDER_STATE_SUCCESS:
			break;
		
		case RESPONDER_STATE_TIMEOUT:
			if (Responder_TimeElapsed(nowMs, LastBlinkTime, RESPONDER_ALARM_BLINK_MS))
			{
				LastBlinkTime = nowMs;
				LED_AlarmToggle();
			}
			break;
		
		default:
			Responder_EnterIdle();
			break;
	}
}

