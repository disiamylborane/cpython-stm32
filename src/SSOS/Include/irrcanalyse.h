
#include "irrc.h"

enum IRAnalyze_Value
{
	IRAnalyze_TRUE=1,
	IRAnalyze_FALSE=0,
	IRAnalyze_MAYBE=2,
};

enum IRAnalyze_EqVal
{
	IRAnalyze_EqVal_Eq=0,
	IRAnalyze_EqVal_Hi=1,
	IRAnalyze_EqVal_Lo=-1,
};

extern enum IRAnalyze_Value IRAnalyze_Header(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255);
extern void IRAnalyze_Reset (void);
extern bool IRAnalyze_Foot(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255);
extern bool IRAnalyze_Lead(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255);
extern bool IRAnalyze_Data(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, uint8_t bits, s_ir_code *out);
extern bool IRAnalyze_Trail(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255);

extern bool IRAnalyze_Pre(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, s_ir_code *out);
extern bool IRAnalyze_Post(const struct IR_remote *rmt, s_ir_code* in, uint16_t *index, uint8_t pp255, s_ir_code *out);

extern bool IRAnalyze(const struct IR_remote *rmt, s_ir_code* in, uint8_t pp255, s_ir_code *pre, s_ir_code *data, s_ir_code *post);
