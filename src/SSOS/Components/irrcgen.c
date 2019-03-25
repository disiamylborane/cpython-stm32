
#include "irrc.h"
#include "irrcgen.h"

//============================================================================================
//============================================================================================
static uint16_t maxindex;

#define GEN_ASSERT(x) if(!(x)) return false;

bool IRGenerate(s_lirc_t data, s_ir_code* out, uint16_t *index, bool IsPulse)
{
	if(*index == maxindex)
		return false;
	if(data==0) return true;
	bool cond = ((*index)%2)==1;
	if(!IsPulse)cond=!cond;//Who knows how to xor them?
	if(cond)
		out[(*index)-1]+=data;
	else
		out[(*index)++]=data;
	return true;
}

#define IRGeneratePulse(data, out, index) GEN_ASSERT(IRGenerate(data, out, index, true))
#define IRGenerateSpace(data, out, index) GEN_ASSERT(IRGenerate(data, out, index, false))

bool IRGenerateHeader(const const struct IR_remote *rmt, s_ir_code* out, uint16_t *index)
{
	if(rmt->phead)
		IRGeneratePulse(rmt->phead, out,index);
	if(rmt->shead)
		IRGenerateSpace(rmt->shead, out,index);
	return true;
}
bool IRGenerateFoot(const const struct IR_remote *rmt, s_ir_code* out, uint16_t *index)
{
	if(rmt->pfoot)
		IRGeneratePulse(rmt->pfoot, out,index);
	if(rmt->sfoot)
		IRGenerateSpace(rmt->sfoot, out,index);
	return true;
}
#define  IRGenerateLead(rmt, out, index) if(rmt->plead) IRGeneratePulse(rmt->plead,  out, index);
#define IRGenerateTrail(rmt, out, index) if(rmt->ptrail)IRGeneratePulse(rmt->ptrail, out, index);

static inline int bits_set(s_ir_code data)
{
	int ret = 0;
	while(data)
	{
		if(data&1) ret++;
		data >>= 1;
	}
	return ret;
}
static inline s_ir_code reverse(s_ir_code data,int bits)
{
	int i;
	s_ir_code c;
	
	c=0;
	for(i=0;i<bits;i++)
	{
		c|=(s_ir_code) (((data & (((s_ir_code) 1)<<i)) ? 1:0))
						     << (bits-1-i);
	}
	return(c);
}

static inline int is_biphase(const const struct IR_remote *rmt)
{
	if(rmt->protocol == irp_RC5 || rmt->protocol == irp_RC6) return(1);
	else return(0);
}

//#define send_pulse(x) IRGeneratePulse(x,out,index)
//#define send_space(x) IRGenerateSpace(x,out,index)
bool IRGenerateData(const const struct IR_remote *rmt, s_ir_code data,int bits,int done, s_ir_code* out, uint16_t *index)
{
	int i;
	int all_bits = rmt->pre_data_bits +	rmt->bits +	rmt->post_data_bits;;
	int toggle_bit_mask_bits = bits_set(rmt->toggle_bit_mask);
	s_ir_code mask;
	
	if(!rmt->flags.irfREVERSE)
		data=reverse(data,bits);
	if(rmt->protocol == irp_RCMM)
	{
		mask=(s_ir_code)1<<(all_bits-1-done);
		if(bits%2 || done%2)
		{
			return true;
		}
		for(i=0;i<bits;i+=2,mask>>=2)
		{
			switch(data&3)
			{
			case 0:
				IRGeneratePulse(rmt->pzero,out,index);
				IRGenerateSpace(rmt->szero,out,index);
				break;
			/* 2 and 1 swapped due to reverse() */
			case 2:
				IRGeneratePulse(rmt->pone,out,index);
				IRGenerateSpace(rmt->sone,out,index);
				break;
			case 1:
				IRGeneratePulse(rmt->ptwo,out,index);
				IRGenerateSpace(rmt->stwo,out,index);
				break;
			case 3:
				IRGeneratePulse(rmt->pthree,out,index);
				IRGenerateSpace(rmt->sthree,out,index);
				break;
			}
			data=data>>2;
		}
		return true;
	}
	else if(rmt->protocol == irp_XMP)
	{
		if(bits%4 || done%4)
		{
			return true;
		}
		for(i = 0; i < bits; i += 4)
		{
			s_ir_code nibble;

			nibble = reverse(data & 0xf, 4);
			IRGeneratePulse(rmt->pzero,out,index);
			IRGenerateSpace((unsigned long)(rmt->szero + nibble*rmt->sone),out,index);
			data >>= 4;
		}
		return true;
	}

	mask=((s_ir_code) 1)<<(all_bits-1-done);
	for(i=0;i<bits;i++,mask>>=1)
	{
		if((rmt->toggle_bit_mask>0) && mask&rmt->toggle_bit_mask)
		{
			if(toggle_bit_mask_bits == 1)
			{
				/* backwards compatibility */
				data &= ~((s_ir_code) 1);
				if(rmt->toggle_bit_mask_state&mask)
				{
					data |= (s_ir_code) 1;
				}
			}
			else
			{
				if(rmt->toggle_bit_mask_state&mask)
				{
					data ^= (s_ir_code) 1;
				}
			}
		}
		if(rmt->toggle_mask>0 &&
		   mask&rmt->toggle_mask &&
		   rmt->toggle_mask_state%2)
		{
			data ^= 1;
		}
		if(data&1)
		{
			if(is_biphase(rmt))
			{
				
				if(mask&rmt->rc6_mask)
				{
					IRGenerateSpace(2*rmt->sone,out,index);
					IRGeneratePulse(2*rmt->pone,out,index);
				}
				else
				{
					IRGenerateSpace(rmt->sone,out,index);
					IRGeneratePulse(rmt->pone,out,index);
				}
			}
			else if(rmt->flags.irfSPACE_FIRST)
			{
				IRGenerateSpace(rmt->sone,out,index);
				IRGeneratePulse(rmt->pone,out,index);
			}
			else
			{
				IRGeneratePulse(rmt->pone,out,index);
				IRGenerateSpace(rmt->sone,out,index);
			}
		}
		else
		{
			if(mask&rmt->rc6_mask)
			{
				IRGeneratePulse(2*rmt->pzero,out,index);
				IRGenerateSpace(2*rmt->szero,out,index);
			}
			else if(rmt->flags.irfSPACE_FIRST)
			{
				IRGenerateSpace(rmt->szero,out,index);
				IRGeneratePulse(rmt->pzero,out,index);
			}
			else
			{
				IRGeneratePulse(rmt->pzero,out,index);
				IRGenerateSpace(rmt->szero,out,index);
			}
		}
		data=data>>1;
	}
	return true;
}

bool IRGeneratePre(const struct IR_remote *rmt, s_ir_code* out, uint16_t *index)
{
	if(rmt->pre_data_bits>0)
	{
		IRGenerateData(rmt,rmt->pre_data,rmt->pre_data_bits,0,out,index);
		if(rmt->ppre>0 && rmt->spre>0)
		{
			IRGeneratePulse(rmt->ppre,out,index);
			IRGenerateSpace(rmt->spre,out,index);
		}
	}
	return true;
}

bool IRGeneratePost(const struct IR_remote *rmt, s_ir_code* out, uint16_t *index)
{
	if(rmt->post_data_bits>0)
	{
		if(rmt->ppost>0 && rmt->spost>0)
		{
			IRGeneratePulse(rmt->ppost,out,index);
			IRGenerateSpace(rmt->spost,out,index);
		}
		IRGenerateData(rmt,rmt->post_data,rmt->post_data_bits,0,out,index);
	}
	return true;
}

bool IRGenerateRepeat(const struct IR_remote *rmt, s_ir_code* out, uint16_t *index)
{
	IRGenerateLead(rmt,out,index);
	IRGeneratePulse(rmt->prepeat,out,index);
	IRGenerateSpace(rmt->srepeat,out,index);
	IRGenerateTrail(rmt,out,index);
	return true;
}

bool IRGenerateCode(const struct IR_remote *rmt, s_ir_code code, int repeat, s_ir_code* out, uint16_t maxcount)
{
	maxindex = maxcount-1;
	uint16_t index=0;
	if(!repeat || !(rmt->flags.irfNO_HEAD_REP))
		IRGenerateHeader(rmt,out,&index);
	IRGenerateLead(rmt,out,&index);
	IRGeneratePre (rmt,out,&index);
	IRGenerateData(rmt,code,rmt->bits,rmt->pre_data_bits,out,&index);
	IRGeneratePost(rmt,out,&index);
	IRGenerateTrail(rmt,out,&index);
	if(!repeat || !(rmt->flags.irfNO_FOOT_REP))
		IRGenerateFoot(rmt,out,&index);
	return true;
}
