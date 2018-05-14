#include "pwm.h"
#include "stm32f10x_tim.h"
#include "fastmath.h"
#include <math.h>

//@HackOS: ����ʱ��
//@HackOS: ��DTG[7:0]=200 {������1100 1000}ʱ,����ʱ��Ϊ(32+8)*Tdtg,Tdtg=8*Tdts=8*1/72,����DT=40*8/72=4.44us,�������100us����ʱ,ʵ�ʲ���96us
//@HackOS: ������ϱ��ӳ�2.22us,�±۳�ǰ2.22us  ����4.44usǰ��ʱ
//@HackOS: ������ϱ�����2.22us,�±�����2.22us  ����4.44us����ʱ
int timer_1_8_deadtime_register = 200; //this is not just a delay value, check CPU reference manual for TIMx_BDTR DTG bit 0-7
int timer_4_5_deadtime_delay = 80; // in 18MHz ticks

//@HackOS: �����������
float testPhase = -0.09;
//float testPhase = 5;

static int g_YawOff = 1;						//@HackOS: YAW���ñ�־
static int g_Roll[3], g_Pitch[3], g_Yaw[3];		//@HackOS: PWM���ֵ

int MaxCnt[NUMAXIS];
int MinCnt[NUMAXIS];
int IrqCnt[NUMAXIS];

////@HackOS: ����PWM,���ò��
//void SetPWMFastTable(int *pwm, float pid, int power)
//{
//	int pidInt;
//	int iPower;
//	//@HackOS: ������������
//    if (testPhase >= 0)
//    {
//        pid = testPhase;
//    }

//	//@HackOS: ��ȡPID��������Һ����е�X����ֵ(����)
//    pidInt = (int)Round(pid / M_TWOPI * SINARRAYSIZE);
//	//@HackOS: ����,ȡ��
//    pidInt = pidInt % SINARRAYSIZE;

//	//@HackOS: ��ֵ,�����ֵ
//    if (pidInt < 0)
//    {
//        pidInt = SINARRAYSIZE + pidInt;
//    }

//    iPower = 5 * power;
//    pwm[0] = (sinDataI16[pidInt                          % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
//    pwm[1] = (sinDataI16[(pidInt + 1 * SINARRAYSIZE / 3)     % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
//    pwm[2] = (sinDataI16[(pidInt + (2 * SINARRAYSIZE + 1) / 3) % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
////	printf("%d,%d,%d\r\n",pwm[0],pwm[1],pwm[2]);
//}

//@HackOS: ����PWM,���ò��
void SetPWMFastTable(int *pwm, float out, int power)
{
	int pidInt;
	int iPower;

	//@HackOS: ��ȡPID��������Һ����е�X����ֵ(����)
    pidInt = (int)Round(out / M_TWOPI * SINARRAYSIZE);
	//@HackOS: ����,ȡ��
    pidInt = pidInt % SINARRAYSIZE;

	//@HackOS: ��ֵ,�����ֵ
    if (pidInt < 0)
    {
        pidInt = SINARRAYSIZE + pidInt;
    }

    iPower = 5 * power;
	
	pwm[0] = (sinDataI16[pidInt                          % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
    pwm[1] = (sinDataI16[(pidInt + 1 * SINARRAYSIZE / 3)     % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
    pwm[2] = (sinDataI16[(pidInt + (2 * SINARRAYSIZE + 1) / 3) % SINARRAYSIZE] * iPower + SINARRAYSCALE / 2) / SINARRAYSCALE + (PWM_PERIODE / 2);
	
//	printf("%d,%d,%d\r\n",pwm[0],pwm[1],pwm[2]);
}

void SetPWMOrg(int *pwm, float output, int level)
{
    pwm[0] = (sin(output)        * 5 * level) + (PWM_PERIODE / 2);
    pwm[1] = (sin(output + 2.09) * 5 * level) + (PWM_PERIODE / 2);
    pwm[2] = (sin(output + 4.19) * 5 * level) + (PWM_PERIODE / 2);
}

//@HackOS: ����PWM
void SetPWM(int *pwm, float pid, int power)
{
	SetPWMFastTable(pwm, pid, power);
}

//@HackOS: ���PWM����
void SetPWMData(int *target, int *pwm)
{
    __disable_irq();

    target[0] = pwm[0];
    target[1] = pwm[1];
    target[2] = pwm[2];

    __enable_irq();
}



//@HackOS: �����ж�
void ActivateIRQ(TIM_TypeDef *tim)
{
    __disable_irq();
	//@HackOS: ��������ж�
    tim->SR &= ~TIM_SR_UIF;   // clear UIF flag
    //@HackOS: ���ø����ж�
	tim->DIER = TIM_DIER_UIE; // Enable update interrupt
    __enable_irq();
}

//@HackOS: ����PWM�ر�
void PWMOff(void)
{
	int pwm[3];
	pwm[0] = pwm[1] = pwm[2] = 0;
	SetPWMData(g_Roll, pwm);
	SetPWMData(g_Pitch, pwm);

	g_YawOff = 1;
	TIM5->DIER = TIM_DIER_UIE; // Enable update interrupt
	TIM1->DIER = TIM_DIER_UIE; // Enable update interrupt
	TIM8->DIER = TIM_DIER_UIE; // Enable update interrupt
}

//@HackOS: ����Pitch���ֵ
//@HackOS: �������:PID���,POWER
void SetPitchMotor(float pid, int power)
{
	int pwm[3];
	SetPWM(pwm, pid, power);
	SetPWMData(g_Pitch, pwm);
	ActivateIRQ(TIM8);
}

//@HackOS: ����Roll���ֵ
//@HackOS: �������:PID���,POWER
void SetRollMotor(float pid, int power)
{
	int pwm[3];
	//@HackOS: ����PWM����,����������,�����pwm
	SetPWM(pwm, pid, power);
	//@HackOS: ���PWM����,�����g_Roll
	SetPWMData(g_Roll, pwm);
	//@HackOS: EVVGC��������,ROLLӦ����TIM1������TIM8,EVVGC��pitch��roll����
	ActivateIRQ(TIM1);
//	TIM1->CCR1 = g_Roll[0];
//	TIM1->CCR2 = g_Roll[1];
//	TIM1->CCR3 = g_Roll[2];
//	printf("%d,%d,%d\r\n",pwm[0],pwm[1],pwm[2]);
}

//@HackOS: YAW�޷�
void LimitYawPWM(int *pwm)
{
	//@HackOS: ���PWMֵ
    int maxVal = PWM_PERIODE - 2 * timer_4_5_deadtime_delay;
	int i;
    for (i = 0; i < 3; i++)
    {
        pwm[i] -= timer_4_5_deadtime_delay;

		//@HackOS: �޷�
        if (pwm[i] >= maxVal)
        {
            pwm[i] = maxVal;
        }

		//@HackOS: �޷�
        if (pwm[i] < timer_4_5_deadtime_delay)
        {
            pwm[i] = 0;
        }
    }
}

//@HackOS: ����Yaw���ֵ
//@HackOS: �������:PID���,POWER
void SetYawMotor(float pid, int power)
{
    int pwm[3];
    SetPWM(pwm, pid, power);
    LimitYawPWM(pwm);
    SetPWMData(g_Yaw, pwm);
    g_YawOff = 0;
    ActivateIRQ(TIM5);
}

//@HackOS: wait
//void UpdateCounter(tAxis channel, int value)
void UpdateCounter(tAxis channel, int value)
{
    IrqCnt[channel]++;

	//@HackOS: ȡ���ֵ
    if (value > MaxCnt[channel])
    {
        MaxCnt[channel] = value;
    }

	//@HackOS: ȥ��Сֵ
    if (value < MinCnt[channel])
    {
        MinCnt[channel] = value;
    }
}


void TIM5_IRQHandler(void) // yaw axis
{
	uint16_t cnt;
	//@HackOS: �����жϱ��
    if (TIM5->SR & TIM_SR_UIF) // if UIF flag is set
    {
		//@HackOS: ��������жϱ��
        TIM5->SR &= ~TIM_SR_UIF; // clear UIF flag

        __disable_irq();
        cnt = TIM5->CNT;
        UpdateCounter(YAW, cnt);

		//@HackOS: ȷ�����㹻��ʱ��ȥ�ı�����
        if (cnt < MAX_CNT)  // make sure there is enough time to make all changes
        {
			//@HackOS: YAW�ر���,������������PWM
            if (g_YawOff)
            {
                TIM4->CCR1 = PWM_PERIODE + 1;
                TIM4->CCR2 = PWM_PERIODE + 1;
                TIM4->CCR3 = PWM_PERIODE + 1;

                TIM5->CCR1 = 0;
                TIM5->CCR2 = 0;
                TIM5->CCR3 = 0;
            }
			//@HackOS: ����ģʽ
            else
            {
				//@HackOS: TIM4 5�ֶ���������ʱ��
                int deadTime = 2 * timer_4_5_deadtime_delay;
                TIM4->CCR1 = g_Yaw[0] + deadTime;
                TIM4->CCR2 = g_Yaw[1] + deadTime;
                TIM4->CCR3 = g_Yaw[2] + deadTime;

                TIM5->CCR1 = g_Yaw[0];
                TIM5->CCR2 = g_Yaw[1];
                TIM5->CCR3 = g_Yaw[2];
            }

            TIM5->DIER &= ~TIM_DIER_UIE;  // disable update interrupt
        }

        __enable_irq();
    }
}

void TIM1_UP_IRQHandler(void) // pitch axis
{
	uint16_t cnt;
    TIM1->SR &= ~TIM_SR_UIF; // clear UIF flag

    __disable_irq();
    cnt = TIM1->CNT;
    UpdateCounter(ROLL, cnt);

    if (cnt < MAX_CNT)  // make sure there is enough time to make all changes
    {
        TIM1->CCR1 = g_Roll[0];
        TIM1->CCR2 = g_Roll[1];
        TIM1->CCR3 = g_Roll[2];

        TIM1->DIER &= ~TIM_DIER_UIE; // disable update interrupt
    }

    __enable_irq();
}

void TIM8_UP_IRQHandler(void) // roll axis
{
	uint16_t cnt;
    TIM8->SR &= ~TIM_SR_UIF; // clear UIF flag

    __disable_irq();
    cnt = TIM8->CNT;
    UpdateCounter(PITCH, cnt);

    if (cnt < MAX_CNT)  // make sure there is enough time to make all changes
    {
        TIM8->CCR1 = g_Pitch[0];
        TIM8->CCR2 = g_Pitch[1];
        TIM8->CCR3 = g_Pitch[2];

        TIM8->DIER &= ~TIM_DIER_UIE; // disable update interrupt
    }

    __enable_irq();
}


static void Timer_Channel_Config(TIM_TypeDef *tim, TIM_OCInitTypeDef *OCInitStructure)
{
    TIM_OC1Init(tim, OCInitStructure);
    TIM_OC2Init(tim, OCInitStructure);
    TIM_OC3Init(tim, OCInitStructure);

    TIM_OC1PreloadConfig(tim, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(tim, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(tim, TIM_OCPreload_Enable);
}

//@HackOS: TIM1 8 �߼���ʱ������,�������Ļ���PWM
static void Timer_PWM_Advanced_Config(TIM_TypeDef *tim)
{
	TIM_TimeBaseInitTypeDef     TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef           TIM_OCInitStructure;
	TIM_BDTRInitTypeDef         TIM_BDTRInitStructure;

	//@HackOS: ��ʱ����������
	//Time Base configuration
	TIM_TimeBaseInitStructure.TIM_Prescaler = 3; // 18MHz		//@HackOS: Ԥ��Ƶ
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	TIM_TimeBaseInitStructure.TIM_Period = PWM_PERIODE;			//@HackOS: ����ֵ1000 18KHZ
	
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;			//@HackOS: ʱ�ӷ�Ƶ����
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(tim, &TIM_TimeBaseInitStructure);

	//@HackOS: ��������/ɲ���ܸ߼�����
	//Automatic Output enable, Break, dead time and lock configuration
	TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
	TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
	TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_1;				//@HackOS: �Ĵ��������ȼ�
	
	TIM_BDTRInitStructure.TIM_DeadTime = timer_1_8_deadtime_register;	//@HackOS: ����ʱ��,������������ʱ��,��Ҫ���ݲο��ֲ�V1.0_P233����ó�
	
	TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;				//@HackOS: ����ɲ�������ź�
	TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_High;	//@HackOS: ɲ�������źż���
	TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
	TIM_BDTRConfig(tim, &TIM_BDTRInitStructure);

	//@HackOS: ����PWMģʽ
	//Configuration in PWM mode
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1 ;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;		//@HackOS: �Ƚ����ʹ��
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;		//@HackOS: �����Ƚ����ʹ��
	TIM_OCInitStructure.TIM_Pulse = 0;									//@HackOS: TIMX->CCRx��ֵ
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCPolarity_High;			//@HackOS: ���������Ч����
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

	Timer_Channel_Config(tim, &TIM_OCInitStructure);
}

//@HackOS: ͨ�ö�ʱ��TIM4 5 ���� 6mhz
static void Timer_PWM_General_Config(TIM_TypeDef *tim, int polarity)
{
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef           TIM_OCInitStructure;

    TIM_TimeBaseInitStructure.TIM_Prescaler = 3; // 18MHz
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = PWM_PERIODE;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV4;		//@HackOS: �ķ�Ƶ
    TIM_TimeBaseInit(tim, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1 ;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = polarity;

    Timer_Channel_Config(tim, &TIM_OCInitStructure);
}


//@HackOS: ����PWM�ж����ȼ�
static void SetupPWMIrq(uint8_t irq)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    NVIC_InitStructure.NVIC_IRQChannel = irq;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;//Preemption Priority
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

//@HackOS: ����PWM_GPIO
static void PWM_GPIO_Config(void)
{
	
    GPIO_InitTypeDef    GPIO_InitStructure;
    //TIMER1 pin config//////////////////////////////////////////////////////////
	//@HackOS: ROLL�ϱ�-->TIM1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

	//@HackOS: ROLL�±�-->TIM1
    //TIMER1 pin config
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

	//@HackOS: PITCH�ϱ�-->TIM8
    //Timer8 pin config/////////////////////////////////////////////////////////////
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

	//@HackOS: PITCH�±�-->TIM8
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

	//@HackOS: YAW�ϱ�-->TIM5
    //Timer5 pin config//////////////////////////////////////////////////////////////
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


	//@HackOS: YAW�±�-->TIM5
    //Timer4 pin config/////////////////////////////////////////////////////////////
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_7 | GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    //////////////////////////////////////////////////////////////////////////////
}

//@HackOS: ����PWM����
static void PWM_Mode_Config(void)
{
	//@HackOS: TIM1 8 �߼���ʱ������,�������Ļ���PWM
    //rewrite that thing;
    Timer_PWM_Advanced_Config(TIM1);
    Timer_PWM_Advanced_Config(TIM8);

	//@HackOS: TIM4 5ͨ�ö�ʱ������
    Timer_PWM_General_Config(TIM5, TIM_OCPolarity_High);		//@HackOS: YAW�ϱ�,��Ч���Ը�
    Timer_PWM_General_Config(TIM4, TIM_OCPolarity_Low);			//@HackOS: YAW�±�,��Ч���Ե�

	//@HackOS: ��ʼ������ֵ,��,Ϊ�˷�ֹ�жϳ�ͻ?
    TIM4->CNT = timer_4_5_deadtime_delay;
    TIM1->CNT = timer_4_5_deadtime_delay + 3 + PWM_PERIODE / 3;
    TIM8->CNT = timer_4_5_deadtime_delay + 5 + PWM_PERIODE * 2 / 3;

	//@HackOS: ���ȼ�����,������ȼ�
    SetupPWMIrq(TIM5_IRQn);    // yaw
    SetupPWMIrq(TIM1_UP_IRQn); // pitch
    SetupPWMIrq(TIM8_UP_IRQn); // roll

	//@HackOS: �Ƚ���ȫ���ж�
    __disable_irq();
    {
		//@HackOS: ʹ�ܶ�ʱ��
		TIM_Cmd(TIM5, ENABLE);
		TIM_Cmd(TIM4, ENABLE);
		TIM_Cmd(TIM1, ENABLE);
        TIM_Cmd(TIM8, ENABLE);
    }

	//@HackOS: ʹ�ܶ�ʱ��PWM���
    TIM_CtrlPWMOutputs(TIM5, ENABLE);
    TIM_CtrlPWMOutputs(TIM4, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM8, ENABLE);
    //@HackOS: ȫ��������������������ж�
	__enable_irq();
}

void PWM_Init(void)
{
	//@HackOS: ����ʱ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_TIM1  | RCC_APB2Periph_TIM8, ENABLE);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4  | RCC_APB1Periph_TIM5, ENABLE);
	
	//@HackOS: ����GPIO
	PWM_GPIO_Config();
	
	//����PWM��ʱ������
	PWM_Mode_Config();
	
}


