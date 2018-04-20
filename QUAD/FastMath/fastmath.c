#include "fastmath.h"
#include "math.h"


//@HackOS: ���Һ������
short int sinDataI16[SINARRAYSIZE];

float Rad2Deg(float x)
{
    return x * (180.0F / M_PI);
}

float Deg2Rad(float x)
{
    return x * (M_PI / 180.0F);
}

float Round(float x)
{
    if (x >= 0)
    {
        return x + 0.5F;
    }
    else
    {
        return x - 0.5F;
    }
}

//@HackOS: ��ʼ�����Һ�������
void InitSinArray(void)
{
	int i;
    for (i = 0; i < SINARRAYSIZE; i++)
    {
        float x = i * M_TWOPI / SINARRAYSIZE;
        sinDataI16[i] = (short int)Round(sinf(x) * SINARRAYSCALE);
        //print("i %3d  x %f  sin %d\r\n", i, x, (int)sinDataI16[i]);
    }
}

//@HackOS: ������Һ���
float fastSin(float x)
{
    if (x >= 0)
    {
        int ix = ((int)(x / M_TWOPI * (float)SINARRAYSIZE)) % SINARRAYSIZE;
        return sinDataI16[ix] / (float)SINARRAYSCALE;
    }
    else
    {
        int ix = ((int)(-x / M_TWOPI * (float)SINARRAYSIZE)) % SINARRAYSIZE;
        return -sinDataI16[ix] / (float)SINARRAYSCALE;
    }
}


