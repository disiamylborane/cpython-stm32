
#include <string.h>

struct {
	int (*si_TIM2)();
	int (*si_TIM3)();
	int (*si_TIM4)();
	int (*si_TIM6_DAC)();
	int (*si_TIM7)();
	int (*si_ADC)();
	int (*si_EXT0)();
	int (*si_EXT1)();
	int (*si_EXT2)();
	int (*si_EXT3)();
	int (*si_EXT4)();
}ssos_it_vectors;

void SSOS_IT_Reset()
{
	memset(&ssos_it_vectors,0,sizeof ssos_it_vectors);
}

int TIM2_IRQHandler()
	{return ssos_it_vectors.si_TIM2();}
int TIM3_IRQHandler()
	{return ssos_it_vectors.si_TIM3();}
int TIM4_IRQHandler()
	{return ssos_it_vectors.si_TIM4();}
int TIM6_DAC_IRQHandler()
	{return ssos_it_vectors.si_TIM6_DAC();}
int TIM7_IRQHandler()
	{return ssos_it_vectors.si_TIM7();}
