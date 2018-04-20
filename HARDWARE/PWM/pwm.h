#ifndef __PWM_H_
#define __PWM_H_

#define PWM_PERIODE 1000

//@HackOS: ���ʱ��Ϊ0.8����PWM_PERIODE
//@HackOS: ȷ�����㹻��ʱ�����ı�����
#define MAX_CNT (PWM_PERIODE * 8 / 10)

typedef enum
{
    ROLL,
    PITCH,
    YAW,
    NUMAXIS
}tAxis;

extern int MaxCnt[NUMAXIS];
extern int MinCnt[NUMAXIS];
extern int IrqCnt[NUMAXIS];


extern void PWM_Init(void);


#endif

