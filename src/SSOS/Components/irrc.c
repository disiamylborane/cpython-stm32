
#include "irrc.h"
#include "stm32f4xx.h"
#include "SSOS_fun.h"
#include <stdint.h>

#define bB(Peripherial_register) ((uint32_t *) (0x42000000 + (uint32_t)((uint32_t)(&Peripherial_register)-0x40000000)*0x20))
#define B8(b) ((unsigned char)B8__(HEX__(b)))
#define B16(b1,b0)  (((unsigned short)B8(b1)<<8) | B8(b0))
#define B32(b3,b2,b1,b0)    (((unsigned long)B8(b3)<<24) |                              ((unsigned long)B8(b2)<<16) |                              ((unsigned long)B8(b1)<< 8) |                                              B8(b0))
#define HEX__(n) 0x##n##UL
#define B8__(x) (((x&0x0000000FUL)?0x01:0) |                  ((x&0x000000F0UL)?0x02:0) |                  ((x&0x00000F00UL)?0x04:0) |                  ((x&0x0000F000UL)?0x08:0) |                  ((x&0x000F0000UL)?0x10:0) |                  ((x&0x00F00000UL)?0x20:0) |                  ((x&0x0F000000UL)?0x40:0) |                  ((x&0xF0000000UL)?0x80:0))

#define ADC1 NO_ADC_MUST_BE

static uint32_t IR_zeros_count_max;
static uint32_t *IR_PIN;


static uint32_t IR_GetTimerFrequency(){
	return 84000000UL;
}

void IR_Init_Setport(GPIO_TypeDef *GPIOin, int pin){
	IR_PIN = &(bB(GPIOin->IDR)[pin]);
}

/**
  init with PB4 as in, PB7 as out
	Timers 3,4 are used
	zeros_count_max default is 40
*/
void IR_Init(uint32_t zeros_count_max){
	IR_Init_Setport(GPIOG, 2);
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOBEN;
	GPIOB->MODER |= B32(00000000,00000000,10000000,00000000);
	GPIOB->MODER &= B32(11111111,11111111,10111111,11111111);
	GPIOB->AFR[0] |= B32(00100000,00000000,00000000,00000000);//PB7 = Timer4_pwm
	GPIOB->AFR[0] &= B32(00101111,11111111,11111111,11111111);//PB7 = Timer4_pwm
	
	GPIOG->MODER &= B32(11111111,11111111,11111111,11001111);//PG2 = in
	GPIOG->PUPDR |= B32(00000000,00000000,00000000,00100000);
	
	IR_zeros_count_max=zeros_count_max;
}

struct IR_DataSequence IR_ReceiveToRam(s_lirc_t* out, uint16_t maxcount, uint32_t maxlength_ms, bool wait)
{
	struct IR_DataSequence rtn;
	uint32_t MaxReceivedLength = maxlength_ms*(IR_GetTimerFrequency()/1000)/0xFFFF + 1;

	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	RCC->APB1RSTR = RCC_APB1RSTR_TIM3RST;
	RCC->APB1RSTR = 0;

	TIM3->CR1 = 0;
	TIM3->PSC = 0;
	TIM3->ARR = -1;
	TIM3->DIER = TIM_DIER_UIE;//Updatre event source
	TIM3->CNT = 0;
	TIM3->EGR = TIM_EGR_UG;
	TIM3->SR = 0;

	uint32_t TIMCNT = 0;
	uint32_t index = 0;
	
	uint32_t FullOnesLength = 0;
	uint32_t FullZerosLength = 0;
	uint32_t FullHalfImpulsesCount = 0;
	
	uint32_t ReceivedLength = 0;
	
	uint32_t HalfImpulsesCount = 0;
	uint32_t OnesLength = 0;
	uint32_t ZerosLength = 0;
	
	if(wait)
		while(*IR_PIN == 0);
	
	TIM3->CR1 = 1;//ENABLE	
	
	while(1)
	{
		//Step 1. Receive ONE
		while(*IR_PIN){
			if(TIM3->SR & TIM_SR_UIF){
				TIM3->SR=0;
				TIMCNT+=1<<16;
				ReceivedLength++;
				if(ReceivedLength >= MaxReceivedLength)
					goto StopReceiving;
			}
		}
		
		//Step 2. Save received ONE data
		TIMCNT += TIM3->CNT;
		TIM3->CNT=0;
		TIM3->SR=0;
		HalfImpulsesCount++;
		OnesLength += TIMCNT;
		TIMCNT=0;
		
		//Step 3. Receive ZERO
		while(!(*IR_PIN)){
			if(TIM3->SR & TIM_SR_UIF){
				TIM3->SR=0;
				TIMCNT+=1<<16;
				ReceivedLength++;
				if(ReceivedLength >= MaxReceivedLength)
					goto StopReceiving;
			}
		}
		
		//Step 4. Save received ZERO data
		TIMCNT += TIM3->CNT;
		TIM3->CNT=0;
		TIM3->SR=0;
		
		//Step 5. Analyze ZERO data whether there is PULSE or SPACE
		if(TIMCNT > (OnesLength + ZerosLength)/HalfImpulsesCount*IR_zeros_count_max)
		{
			//It is SPACE. Let us put down the info both about the PULSE and SPACE
			if(index == maxcount-1) goto StopReceiving;
			out[index++] = ((OnesLength + ZerosLength))/(IR_GetTimerFrequency()/1000000);
			if(index == maxcount-1) goto StopReceiving;
			out[index++] = ((TIMCNT))/(IR_GetTimerFrequency()/1000000);

			FullOnesLength += OnesLength;
			FullZerosLength += ZerosLength;
			FullHalfImpulsesCount += HalfImpulsesCount;
			HalfImpulsesCount = OnesLength = ZerosLength = 0;
		}
		else
		{
			//It is still PULSE. Just collect the data and go ahead
			HalfImpulsesCount++;
			ZerosLength += TIMCNT;
		}
		TIMCNT = 0;
	}
	StopReceiving:
	out[index++] = 0;
	
	rtn.data = out;
	rtn.data_count = index;
	rtn.duty_cycle_per_cent = FullOnesLength*100/(FullOnesLength+FullZerosLength);
	rtn.freq = IR_GetTimerFrequency()/((FullOnesLength+FullZerosLength)/(FullHalfImpulsesCount>>1));
	
	return rtn;
}

void IRTransmitSeq(uint32_t carrfreq, s_lirc_t* Seq, uint16_t maxcount)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN|RCC_APB1ENR_TIM3EN;

	TIM4->CNT=0;
	TIM4->ARR = IR_GetTimerFrequency()/carrfreq - 1;
	TIM4->PSC = 0;
	TIM4->CR2 = B8(00000000);
	TIM4->CCMR1 = B16(01110000, 00000000);
	TIM4->CCR2 = TIM4->ARR>>1;
	TIM4->CCER = TIM_CCER_CC2E;
	TIM4->CR1 = TIM_CR1_ARPE;//YET DISABLED
	
	TIM3->CR1 = 0;
	TIM3->PSC=(IR_GetTimerFrequency()/1000000) - 1;
	TIM3->CNT = 0;
	
	bB(TIM4->CR1)[0] = 0;
	for(uint16_t i=0; i<maxcount; i++)
	{
		TIM3->SR=0;
		if(!Seq[i]) break;
		TIM3->ARR = Seq[i]-1;
		TIM3->EGR = TIM_EGR_UG;
		TIM3->CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
		bB(TIM4->CR1)[0] ^= 1;
		while((TIM3->CR1 & TIM_CR1_CEN) != 0){};
		TIM4->CNT = 0;
	}
	TIM3->CR1 = 0;
	TIM4->CR1 = 0;
	bB(TIM4->CR1)[0] = 0;
	RCC->APB1ENR &=~ (RCC_APB1ENR_TIM4EN|RCC_APB1ENR_TIM3EN);
}


