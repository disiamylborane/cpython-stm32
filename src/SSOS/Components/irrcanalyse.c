
#include "irrcanalyse.h"
#include "stm32f4xx.h"

static uint32_t IRAnalyze_Checkval_EatenValue = 0;

void IRAnalyze_Reset ()
{
	IRAnalyze_Checkval_EatenValue = 0;
}

int8_t IRAnalyze_ApproxEq (uint32_t var, uint32_t val, uint8_t pp255)
{
	uint32_t temp = (var<16777216) ? (var*pp255/255) : (var/255)*pp255;
	if (var-temp > val)
		return IRAnalyze_EqVal_Hi;
	else if (var+temp < val)
		return IRAnalyze_EqVal_Lo;
	else
		return IRAnalyze_EqVal_Eq;
}

int8_t IRAnalyze_ApproxEqEaten (uint32_t var, uint32_t val, uint8_t pp255, uint32_t eaten)
{
    if(0 == eaten)
        return IRAnalyze_ApproxEq(var, val, pp255);

	uint32_t eatenpc = (eaten<16777216) ? (eaten*pp255/255) : (eaten/255)*pp255;

	if(IRAnalyze_EqVal_Hi == IRAnalyze_ApproxEq(var - eaten - eatenpc, val, pp255))
		return IRAnalyze_EqVal_Hi;

	if(IRAnalyze_EqVal_Lo == IRAnalyze_ApproxEq(var - eaten + eatenpc, val, pp255))
		return IRAnalyze_EqVal_Lo;

	else
		return IRAnalyze_EqVal_Eq;
}

bool IRAnalyze_CheckVal(s_ir_code* in, uint16_t *index, uint8_t pp255, s_lirc_t val)
{
	register int8_t temp;

	if(val)
	{
		temp = IRAnalyze_ApproxEqEaten(in[*index], val, pp255, IRAnalyze_Checkval_EatenValue);
		if(temp == IRAnalyze_EqVal_Lo)
			return false;
		else if (temp == IRAnalyze_EqVal_Eq)
		{
			(*index)++;
			IRAnalyze_Checkval_EatenValue = 0;
		}
		else
			IRAnalyze_Checkval_EatenValue += val;
	}
	return true;
}

bool IRAnalyze_CheckPulse(s_ir_code* in, uint16_t *index, uint8_t pp255, s_lirc_t pval)
{
	if((*index)%2)
			return false;
	return IRAnalyze_CheckVal(in, index, pp255, pval);
}

bool IRAnalyze_CheckSpace(s_ir_code* in, uint16_t *index, uint8_t pp255, s_lirc_t pval)
{
	if(!((*index)%2))
			return false;
	return IRAnalyze_CheckVal(in, index, pp255, pval);
}

enum IRAnalyze_Value IRAnalyze_Header(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	register bool maybe_flag = 0;
	if(rmt->phead)
	{
		if(!IRAnalyze_CheckPulse(in, index, pp255, rmt->phead))
			maybe_flag = 1;
	}
	if(rmt->shead)
	{
		if(!IRAnalyze_CheckSpace(in, index, pp255, rmt->shead))
			return IRAnalyze_FALSE;
	}
	return maybe_flag ? IRAnalyze_MAYBE : IRAnalyze_TRUE;
}


bool IRAnalyze_Foot(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	if(rmt->pfoot)
	{
		if(!IRAnalyze_CheckPulse(in, index, pp255, rmt->pfoot))
			return false;
	}

	if(rmt->sfoot)
	{
		return IRAnalyze_CheckSpace(in, index, pp255, rmt->pfoot);
	}

	return true;
}


bool IRAnalyze_Lead(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	if(rmt->plead)
	{
		return IRAnalyze_CheckPulse(in, index, pp255, rmt->plead);
	}
	return true;
}

bool IRAnalyze_Trail(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	if(rmt->ptrail)
	{
		return IRAnalyze_CheckPulse(in, index, pp255, rmt->ptrail);
	}
	return true;
}

bool IRAnalyze_IsSpaceEncVal(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, s_lirc_t val_P, s_lirc_t val_S)
{	
	if(rmt->flags.irfSPACE_FIRST)
	{
		if(IRAnalyze_CheckSpace(in,index,pp255,val_S))
			return IRAnalyze_CheckPulse(in,index,pp255,val_P);
	}
	else
	{
		if(IRAnalyze_CheckPulse(in,index,pp255,val_P))
			return IRAnalyze_CheckSpace(in,index,pp255,val_S);
	}
	return false;
}

bool IRAnalyze_IsSpaceEncThree(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	return IRAnalyze_IsSpaceEncVal(rmt, in ,index, pp255, rmt->pthree, rmt->sthree);
}

bool IRAnalyze_IsSpaceEncTwo(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	return IRAnalyze_IsSpaceEncVal(rmt, in ,index, pp255, rmt->ptwo, rmt->stwo);
}

bool IRAnalyze_IsSpaceEncOne(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	return IRAnalyze_IsSpaceEncVal(rmt, in ,index, pp255, rmt->pone, rmt->sone);
}

bool IRAnalyze_IsSpaceEncZero(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	return IRAnalyze_IsSpaceEncVal(rmt, in ,index, pp255, rmt->pzero, rmt->szero);
}

bool IRAnalyze_IsShiftEncZero(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	if(IRAnalyze_CheckPulse(in,index,pp255,rmt->pzero))
		return IRAnalyze_CheckSpace(in,index,pp255,rmt->szero);
	return false;
}

bool IRAnalyze_IsShiftEncOne(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255)
{
	if(IRAnalyze_CheckSpace(in,index,pp255,rmt->sone))
		return IRAnalyze_CheckPulse(in,index,pp255,rmt->pone);
	return false;
}

bool IRAnalyze_Data(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, uint8_t bits, s_ir_code *out)
{
	s_ir_code code=0;

	if(rmt->protocol == irp_RC5)
	{
		for(uint16_t i=0;i<bits;i++)
		{
			if(IRAnalyze_IsShiftEncOne(rmt, in, index, pp255))
				code |= 1<< (rmt->flags.irfREVERSE ? i: bits-i-1);
			else if(!IRAnalyze_IsShiftEncZero(rmt, in, index, pp255))
				return false;
		}
		*out = code;
		return true;
	}
	else if(rmt->protocol == irp_SPACE_ENC)
	{
		for(uint16_t i=0;i<bits;i++)
		{
			if(IRAnalyze_IsSpaceEncOne(rmt, in, index, pp255))
				code |= 1<< (rmt->flags.irfREVERSE ? i: bits-i-1);
			else if(!IRAnalyze_IsSpaceEncZero(rmt, in, index, pp255))
				return false;
		}
		*out = code;
		return true;
	}
	else if(rmt->protocol == irp_GOLDSTAR)
	{
		bool (*gfun)(const struct IR_remote*, s_ir_code*, uint16_t*, uint8_t);
		for(uint16_t i=0;i<bits;i++)
		{
			if(i%2)
				gfun=IRAnalyze_IsSpaceEncTwo;
			else
				gfun=IRAnalyze_IsSpaceEncThree;
			if(gfun(rmt, in, index, pp255))
				code |= 1<< (rmt->flags.irfREVERSE ? i: bits-i-1);
			else if(!IRAnalyze_IsSpaceEncZero(rmt, in, index, pp255))
				return false;
		}
		*out = code;
		return true;
	}
	else
		return false;
}

bool IRAnalyze_Pre(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, s_ir_code *out)
{
	if(rmt->pre_data_bits>0)
	{
		if(!IRAnalyze_Data(rmt,in,index,pp255,rmt->pre_data_bits, out))
			return false;
		if(rmt->ppre>0 && rmt->spre>0)
		{
			IRAnalyze_CheckPulse(in,index,pp255,rmt->ppre);
			return IRAnalyze_CheckSpace(in,index,pp255,rmt->ppre);
		}
	}
	else return true;
}

bool IRAnalyze_Post(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, s_ir_code *out)
{
	if(rmt->post_data_bits>0)
	{
		if(rmt->ppost>0 && rmt->spost>0)
		{
			IRAnalyze_CheckPulse(in,index,pp255,rmt->ppost);
			return IRAnalyze_CheckSpace(in,index,pp255,rmt->spost);
		}
		if(!IRAnalyze_Data(rmt,in,index,pp255,rmt->post_data_bits, out))
			return false;
	}
	else return true;
}

bool IRAnalyze(const struct IR_remote *rmt, s_ir_code* in, uint8_t pp255, s_ir_code *pre, s_ir_code *data, s_ir_code *post)
{
	uint16_t index=0;
	IRAnalyze_Reset();
	IRAnalyze_Header(rmt, in, &index, pp255);
	IRAnalyze_Lead(rmt, in, &index, pp255);
	IRAnalyze_Pre (rmt, in, &index, pp255, pre);
	IRAnalyze_Data(rmt, in, &index, pp255, rmt->bits, data);
	IRAnalyze_Post (rmt, in, &index, pp255, post);
	IRAnalyze_Trail(rmt, in, &index, pp255);
	IRAnalyze_Foot(rmt, in, &index, pp255);
}


