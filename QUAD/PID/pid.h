#ifndef __PID_H_
#define __PID_H_
#include "config.h"
extern pid_group_t pid;
extern float pid_roll_out;		//@HackOS: Roll��PID���ֵ

extern void PID_Roll(void);
extern void PID_Test(void);		//@HackOS: PIDЧ������
extern void PID_Show(void);		//@HackOS: PID��ʾ
#endif

