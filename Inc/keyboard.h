
#include "cfg.h"

canvas::v16 wait_for_touch();
bool alt_pressed();

extern "C"
void initialize_keyboard();


char16_t get_symbol_from_keyboard();

extern "C"
void release_keyboard();
