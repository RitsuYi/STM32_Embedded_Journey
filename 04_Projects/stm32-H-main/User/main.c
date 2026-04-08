#include "stm32f10x.h"
#include "Delay.h"
#include "Motor.h"
#include "Encoder.h"
#include "Timer.h"
#include "PID.h"

#define SPEED_LOOP_MS        10U
#define TARGET_SPEED_COUNT   18.0f

#define LEFT_START_PWM       20
#define RIGHT_START_PWM      18
#define LEFT_BIAS_PWM        4
#define RIGHT_BIAS_PWM       0

#define PID_OUT_MAX          80.0f
#define PID_OUT_MIN          0.0f
#define PID_INT_MAX          120.0f
#define PID_INT_MIN          -120.0f

static PID_t LeftPID;
static PID_t RightPID;

static volatile uint8_t RunFlag = 0;
static volatile uint8_t SpeedLoopDivider = 0;

static int16_t ClampPwm(int16_t pwm)
{
    if (pwm > 100)
    {
        pwm = 100;
    }
    if (pwm < 0)
    {
        pwm = 0;
    }

    return pwm;
}

static float AbsCountToSpeed(int16_t count)
{
    if (count < 0)
    {
        count = -count;
    }

    return (float)count;
}

static int16_t ApplyStartCompensation(float pidOut, int16_t startPwm, int16_t biasPwm)
{
    int16_t pwm;

    if (pidOut <= 0.0f)
    {
        return 0;
    }

    pwm = (int16_t)(pidOut + 0.5f);
    pwm += startPwm + biasPwm;

    return ClampPwm(pwm);
}

static void StraightPid_Init(void)
{
    PID_Init(&LeftPID);
    LeftPID.Kp = 3.0f;
    LeftPID.Ki = 0.35f;
    LeftPID.Kd = 0.0f;
    LeftPID.OutMax = PID_OUT_MAX;
    LeftPID.OutMin = PID_OUT_MIN;
    LeftPID.IntegralMax = PID_INT_MAX;
    LeftPID.IntegralMin = PID_INT_MIN;

    PID_Init(&RightPID);
    RightPID.Kp = 3.0f;
    RightPID.Ki = 0.35f;
    RightPID.Kd = 0.0f;
    RightPID.OutMax = PID_OUT_MAX;
    RightPID.OutMin = PID_OUT_MIN;
    RightPID.IntegralMax = PID_INT_MAX;
    RightPID.IntegralMin = PID_INT_MIN;

    Encoder_Get(1);
    Encoder_Get(2);
}

int main(void)
{
    Motor_Init();
    Encoder_Init();
    StraightPid_Init();
    Timer_Init();

    Delay_ms(500);
    RunFlag = 1;

    while (1)
    {
    }
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

        if (RunFlag == 0)
        {
            Motor_SetPWM(1, 0);
            Motor_SetPWM(2, 0);
            return;
        }

        SpeedLoopDivider++;
        if (SpeedLoopDivider >= SPEED_LOOP_MS)
        {
            float leftSpeed;
            float rightSpeed;
            int16_t leftPwm;
            int16_t rightPwm;

            SpeedLoopDivider = 0;

            leftSpeed = AbsCountToSpeed(Encoder_Get(1));
            rightSpeed = AbsCountToSpeed(Encoder_Get(2));

            LeftPID.Target = TARGET_SPEED_COUNT;
            LeftPID.Actual = leftSpeed;
            PID_Update(&LeftPID);

            RightPID.Target = TARGET_SPEED_COUNT;
            RightPID.Actual = rightSpeed;
            PID_Update(&RightPID);

            leftPwm = ApplyStartCompensation(LeftPID.Out, LEFT_START_PWM, LEFT_BIAS_PWM);
            rightPwm = ApplyStartCompensation(RightPID.Out, RIGHT_START_PWM, RIGHT_BIAS_PWM);

            Motor_SetPWM(1, (int8_t)leftPwm);
            Motor_SetPWM(2, (int8_t)rightPwm);
        }
    }
}
