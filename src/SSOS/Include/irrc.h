
#ifndef _SSOS_IRRC_H
#define _SSOS_IRRC_H

#include "stdint.h"

#define IR_DatablockName 20
#define IR_RemoteName 20
#define IR_RawblockCount 0x40


#ifndef __cplusplus
  #define bool char
  #define true 1
  #define false 0
#endif

typedef uint16_t s_lirc_t;
typedef uint16_t s_ir_code;


struct IR_DataSequence
{
	uint32_t freq;
	uint32_t duty_cycle_per_cent;
	uint16_t data_count;
	s_lirc_t *data;
};

struct IR_Rawblock
{
    char Name[4];
    s_ir_code *times;
};

struct IR_RCFlags
{
    unsigned irfSPACE_FIRST	    :1		;
    unsigned irfREVERSE			:1		;
    unsigned irfNO_HEAD_REP		:1		;
    unsigned irfNO_FOOT_REP		:1		;
    unsigned irfCONST_LENGTH    	:1		;
    unsigned irfREPEAT_HEADER   	:1		;
    unsigned irfCOMPAT_REVERSE  :1	;
}__attribute__((packed));

enum IR_Protocol {
    irp_RAW_CODES,
    irp_RC5,
    irp_RC6,
    irp_RCMM,
    irp_SPACE_ENC,
    irp_GOLDSTAR,
    irp_GRUNDIG,
    irp_BO,
    irp_SERIAL,
    irp_XMP
};

struct IR_remote
{
    uint16_t bits;
    enum IR_Protocol protocol;
    struct IR_RCFlags flags;

    s_ir_code pre_data;
    s_ir_code post_data;
    s_ir_code toggle_bit_mask;
    s_ir_code toggle_mask;
    s_ir_code rc6_mask;
 // s_ir_code toggle_bit_mask_state;
    uint32_t freq;
    uint16_t duty_cycle;

    uint16_t eps;
    uint16_t aeps;
    s_lirc_t phead,shead;
    s_lirc_t plead;
    uint16_t pre_data_bits;
    s_lirc_t ppre,spre;

    s_lirc_t pthree,sthree;
    s_lirc_t ptwo,stwo;
    s_lirc_t pone,sone;
    s_lirc_t pzero,szero;

    s_lirc_t ppost, spost;
    uint16_t post_data_bits;
    s_lirc_t ptrail;
    s_lirc_t pfoot,sfoot;

    s_lirc_t prepeat,srepeat;

    s_lirc_t gap;
    s_lirc_t repeat_gap;
    uint16_t toggle_bit;
    uint16_t min_repeat;
    uint16_t min_code_repeat;
    int toggle_mask_state;
	s_ir_code toggle_bit_mask_state;
} __attribute__((packed));

extern void IR_Init(uint32_t zeros_count_max);
extern struct IR_DataSequence IR_ReceiveToRam(s_lirc_t* out, uint16_t maxcount, uint32_t maxbits, bool wait);
extern void IRTransmitSeq(uint32_t carrfreq, s_lirc_t* Seq, uint16_t maxcount);


#endif

