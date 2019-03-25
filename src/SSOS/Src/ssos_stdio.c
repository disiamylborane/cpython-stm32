
#include <stdint.h>
#include <string.h>
#include "stm32f429i_discovery_lcd.h"
#include "irrc.h"
#include "irrcanalyse.h"

#define bB(Peripherial_register) ((uint32_t *) (0x42000000 + (uint32_t)((uint32_t)(&Peripherial_register)-0x40000000)*0x20))


__FILE __stdin__ = {0};
__FILE __stdout__ = {1};
__FILE __stderr__ = {2};
__FILE* stdin = &__stdin__;
__FILE* stdout = &__stdout__;
__FILE* stderr = &__stderr__;


#define ERROR_ON bB(GPIOG->ODR)[14] = 1
#define ERROR_OFF bB(GPIOG->ODR)[14] = 0

#define INDICATOR_ON bB(GPIOG->ODR)[13] = 1
#define INDICATOR_OFF bB(GPIOG->ODR)[13] = 0
/*
int write_screen(const void * buffer, size_t count, size_t RedIndex)
{
	uint16_t w = ((sFONT *)BSP_LCD_GetFont())->Width;
	uint16_t h = ((sFONT *)BSP_LCD_GetFont())->Height;
	BSP_LCD_Clear(LCD_COLOR_BLACK);
	
	uint16_t x=0;
	uint16_t y=0;
	uint16_t screen_x=240/w;
	uint16_t screen_y=320/h;
	for(size_t i = 0; i < count; i++){
		char c = ((uint8_t*)buffer)[i];
		if(c == 0) 
			return i;
		else if(c=='\n'){
			x=0;
			y++;
			continue;
		}
		else if(c=='\r'){
			continue;
		}
		else if((c<' ') || (c>'~'))c='?';
		
		if(i != RedIndex)
			BSP_LCD_SetBackColor(0xFF101010);
		else
			BSP_LCD_SetBackColor(0xFF400000);

		BSP_LCD_DisplayChar(x*w, y*h, c);

		x++;
		if(x==screen_x)
		{
			y++;
			x=0;
		}
		if(y==screen_y)
			break;
	}
	return count;
}
void *copy_back(void *dest, const void *src, size_t n)
{
    char *dp = dest;
    const char *sp = src;
    while (n--)
        *dp++ = *sp++;
    return dest;
}

extern const struct IR_remote Sony;
static s_lirc_t seq[40];

static char letter(char l, char is_upper)
{
	return is_upper? l : l+('a' - 'A');
}
*/

/*
int read_irrc(char * buf, size_t count)
{
	char *buffer=(char *)buf;
	signed int pos=0;
	char is_upper=1;
	char is_number=0;
	s_ir_code pre, data, post;

	BSP_LCD_SetTextColor(0xFF00C000);
	
	memset(buffer, 0x20, count);
	//buffer[count-1]=0;
	for (;;)
	{
		write_screen(buffer,count,pos);
		
		HAL_Delay(200);
		
		INDICATOR_ON;
		IR_ReceiveToRam(seq, 40, 200, 1);
		INDICATOR_OFF;
		
		bool res=IRAnalyze(&Sony, seq, 40, &pre, &data, &post);
		if(res)
		{
			ERROR_OFF;
			if(data >= 0x80 && data <= 0x89){
				if(is_number){
					if(data==0x89)
						buffer[pos] = '0';
					else
						buffer[pos] = '1'+data-0x80;
				}
				else{
					if(data >= 0x81 && data <= 0x85)
						buffer[pos] = letter('A' + 3*(data-0x81),is_upper);
					else if(data == 0x86) // 7
						buffer[pos] = letter('P',is_upper);
					else if(data == 0x87) // 8
						buffer[pos] = letter('T',is_upper);
					else if(data == 0x88) // 9
						buffer[pos] = letter('W',is_upper);
					else if(data == 0x80) // 1
						buffer[pos] = 0;
					else if(data == 0x89) // 0
						buffer[pos] = '+';
				}
			}
			else if(data == 244) //Up ++
				buffer[pos]++;
			else if(data == 245) //Down --
				buffer[pos]--;
			else if(data == 180) //Left <-
				pos--;
			else if(data == 179) //Right ->
				pos++;
			else if(data == 0x92) //VUp [
				buffer[pos] = '[';
			else if(data == 0x93) //VDown ]
				buffer[pos] = ']';
			else if(data == 0x90) //PUp (
				buffer[pos] = '(';
			else if(data == 0x91) //PDown )
				buffer[pos] = ')';
			else if(data == 3415) //TVRadio
				buffer[pos] = '.';
			else if(data == 191) //Teletext ,
				buffer[pos] = ',';
			else if(data == 2969) //Pause "
				buffer[pos] = '"';
			else if(data == 2971) //<
				buffer[pos] = '<';
			else if(data == 2970) //=
				buffer[pos] = '=';
			else if(data == 2972) //>
				buffer[pos] = '>';
			else if(data == 2984) //... space
				buffer[pos] = ' ';
			else if(data == 3444) //Social View Aa
				is_upper = !is_upper;
			else if(data == 3446) //Football ABC123
				is_number = !is_number;
			else if(data == 165) //AV(Near the green Power) Backspace
				copy_back(&(buffer[pos]), &(buffer[pos+1]), count-pos);
			else if(data == 0x95) //Power Enter
			{
				char* i=&(buffer[count-2]);
				while(*i==0x20)
				{
					*i=0;
					i--;
				}
				return strlen(buffer);
			}
			
			if(pos >= count-1)
				pos = count-1;
			if(pos < 0)
				pos = 0;
		}
		else
			ERROR_ON;
	}
}
*/
int write_screen(const void * buffer, size_t count);

int write_screen_out(const void * buffer, size_t count)
{
	//BSP_LCD_SetTextColor(0xFFD0D0D0);
	return write_screen(buffer, count);
}

int write_screen_err(const void * buffer, size_t count)
{
	//BSP_LCD_SetTextColor(0xFFB00000);
	return write_screen(buffer, count);
}


