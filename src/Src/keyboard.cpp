
#include <algorithm>
#include "keyboard.h"
#include "stm32f4xx.h"
#include "stm32f429i_discovery_ts.h"

constexpr auto SCREEN_AREA_DIV = 3;
constexpr auto ROW_COUNT = 3;
constexpr auto COL_COUNT = 5;
struct ShiftRelation {char16_t parent, child;};
struct MicroKeyboardLayout
{
    char16_t main_row[ROW_COUNT][COL_COUNT + 2*(COL_COUNT-1)];

    span<const ShiftRelation> shift_table;
};

struct PressCell {
    uint8_t row;
    uint8_t col;
};

typedef optional<PressCell> KeyboardPress;

static tuple<int16_t, int16_t, int16_t> _kbd_draw_layout(canvas::v16 screensize){
    int16_t cellsize_x = screensize.x / COL_COUNT;
    int16_t cellsize_y = std::min(cellsize_x, int16_t(screensize.y/SCREEN_AREA_DIV/ROW_COUNT));
    int16_t y_start = screensize.y - ROW_COUNT*cellsize_y;

    return {cellsize_x,cellsize_y,y_start};
}

static KeyboardPress touch_to_keybutton(canvas::v16 screensize, canvas::v16 touch){
    auto [cellsize_x, cellsize_y, y_start] = _kbd_draw_layout(screensize);

    if(touch.y < y_start)
        return std::nullopt;

    uint8_t row = (touch.y - y_start) / cellsize_y;
    uint8_t col = (touch.x) / cellsize_x;

    return PressCell {row,col};
};


constexpr char16_t upper_row_main[3][COL_COUNT + 2*(COL_COUNT-1)] = {
    {u'1',u'2',u'3',u'4',u'5',u'6',u'7',u'8',u'9',u'0',u'-',u'=',8},
    {u'!',u'@',u'#',u'$',u'%',u'^',u'&',u'*',u'(',u')',u'_',u'+',8},
    {u'`',u'~',u' ',u' ',u'+',u'-',u'*',u'/',u'=',u' ',u' ',u' ',8},
};

constexpr ShiftRelation standard_english_layout_shifts[] {
    {'[','{'}, {']','}'}, {'\\','|'}, {'\'','"'},
    {';',':'}, {',','<'}, {'.','>'},  {'/','?'},
};

constexpr ShiftRelation standard_greek_layout_shifts[] {
    {u'[',u'{'}, {u']',u'}'}, {u'\\','|'}, {u'\'','"'},
    {u'´',u'¨'}, {u'σ',u'Σ'}, {u'ς',u'Σ'},  {u';',u':'},
    {u',',u'<'}, {u'.',u'>'},  {u'/',u'?'},
};
const MicroKeyboardLayout layouts[] {
    {
        {
            {'q','w','e','r','t','y','u','i','o','p','[',']','\\',},
            {'a','s','d','f','g','h','j','k','l',';','\'',CHAR_ARROWUP_ENC1PLUS,CHAR_ENTER_NEWLINE_BTN0,},
            {'z','x','c','v','b','n','m',',','.','/',CHAR_ARROWLT_ENC0MINUS,CHAR_ARROWDN_ENC1MINUS,CHAR_ARROWRT_ENC0PLUS,},
        },

        standard_english_layout_shifts
    },
    {
        {
            {u'й',u'ц',u'у',u'к',u'е',u'н',u'г',u'ш',u'щ',u'з',u'х',u'ъ',u'ё',},
            {u'ф',u'ы',u'в',u'а',u'п',u'р',u'о',u'л',u'д',u'ж',u'э',CHAR_ARROWUP_ENC1PLUS,CHAR_ENTER_NEWLINE_BTN0,},
            {u'я',u'ч',u'с',u'м',u'и',u'т',u'ь',u'б',u'ю',u'.',CHAR_ARROWLT_ENC0MINUS,CHAR_ARROWDN_ENC1MINUS,CHAR_ARROWRT_ENC0PLUS,},
        },

        standard_english_layout_shifts
    },
    {
        {
            {u';',u'ς',u'ε',u'ρ',u'τ',u'υ',u'θ',u'ι',u'ο',u'π',u'[',u']',u'\\',},
            {u'α',u'σ',u'δ',u'φ',u'γ',u'η',u'ξ',u'κ',u'λ',u'´',u'\'',CHAR_ARROWUP_ENC1PLUS,CHAR_ENTER_NEWLINE_BTN0,},
            {u'ζ',u'χ',u'ψ',u'ω',u'β',u'ν',u'μ',u',',u'.',u'/',CHAR_ARROWLT_ENC0MINUS,CHAR_ARROWDN_ENC1MINUS,CHAR_ARROWRT_ENC0PLUS,},
        },

        standard_greek_layout_shifts
    }
};

static char32_t apply_shift(span<const ShiftRelation> shrel, char32_t sym) {
    if(sym>=u'a' and sym <= u'z')
        return sym + u'A' - u'a';

    if(sym>=u'а' and sym <= u'я')
        return sym + u'А' - u'а';

    if(sym>=u'α' and sym <= u'ω')
        return sym + u'Ω' - u'ω';
    
    auto f = std::find_if(shrel.begin(), shrel.end(), [sym](ShiftRelation a)->bool{return a.parent==sym;});
    if(f != shrel.end()){
        return f->child;
    }

    return sym;
}
// --- Keyboard state ---

// --- Keyboard state ---
struct KbdStateDefault {};
struct KbdStateMainRow {int8_t col;};
struct KbdStateUpperRow {int8_t col;};
struct KbdStatePreset {};

variant<
    KbdStateDefault,
    KbdStateMainRow,
    KbdStateUpperRow,
    KbdStatePreset
> kbd_state;

static int8_t layout_index;
static int8_t preset_category;
static int8_t preset_page;
static const char* kbd_fifo_state; // null: a normal state; non-null: the next symbol from buffer

static vector<string> *presets[COL_COUNT-1];

static void fill_presets()
{
    presets[0]->emplace_back("import ");
    presets[0]->emplace_back("from  import *" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" );
    presets[0]->emplace_back("def ():" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90");
    presets[0]->emplace_back("def (self):" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90");
    presets[0]->emplace_back("class :" "\xE2\x86\x90");
    presets[0]->emplace_back("for  in :" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90");
    presets[0]->emplace_back("print(");
    presets[0]->emplace_back("dir(");

    presets[1]->emplace_back("sin(");
    presets[1]->emplace_back("asin(");
    presets[1]->emplace_back("cos(");
    presets[1]->emplace_back("acos(");
    presets[1]->emplace_back("tan(");
    presets[1]->emplace_back("atan(");
    presets[1]->emplace_back("erf(");
    presets[1]->emplace_back("gamma(");

    presets[2]->emplace_back( "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90" "\xE2\x86\x90");
    presets[2]->emplace_back( "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92" "\xE2\x86\x92");
}

extern "C"
void initialize_keyboard(){
    kbd_state = KbdStateDefault {};
    layout_index = 0;
    preset_category = 0;
    preset_page = 0;
    kbd_fifo_state = nullptr;

    for(auto& preset: presets)
        preset = new vector<string>();
    fill_presets();
}

extern "C"
void release_keyboard() {
    for(auto& preset: presets)
    	delete preset;
};

char32_t kbd_press(KeyboardPress __press, bool shift)
{
    if(!__press){
        kbd_state = KbdStateDefault {};
        return 0;
    }

    PressCell press = *__press;

    if(std::holds_alternative<KbdStateDefault>(kbd_state))
    switch(press.row){
        case ROW_COUNT-1:{
            switch(press.col){
                case 0: return shift ? CHAR_BGARROWLT_ENC0MINUS_ALT : CHAR_ARROWLT_ENC0MINUS;
                case 4: return shift ? CHAR_BGARROWRT_ENC0PLUS_ALT : CHAR_ARROWRT_ENC0PLUS;
                case 1: layout_index = (layout_index+1)%std::size(layouts); break;
                case 2: return shift ? '\n' : ' ';
                case 3: kbd_state=KbdStatePreset{}; break;
                default: break;
            }
        }
        break;
        case 0:{
            kbd_state = KbdStateUpperRow{press.col};
        }
        break;
        default:{
            kbd_state = KbdStateMainRow{press.col};
        }
        break;
    }

    else if (std::holds_alternative<KbdStateMainRow>(kbd_state)){
        auto col = (int)press.col + 2*(int)std::get<KbdStateMainRow>(kbd_state).col;
        auto row = press.row;

        auto ret = layouts[layout_index].main_row[row][col];

        kbd_state = KbdStateDefault {};

        if(shift) return apply_shift(layouts[layout_index].shift_table, ret);
        else return ret;
    }

    else if (std::holds_alternative<KbdStateUpperRow>(kbd_state)){
        auto col = (int)press.col + 2*(int)std::get<KbdStateUpperRow>(kbd_state).col;
        auto row = press.row;

        kbd_state = KbdStateDefault {};

        return upper_row_main[row][col];
    }

    else if (std::holds_alternative<KbdStatePreset>(kbd_state)){
        auto col = press.col;
        auto row = press.row;

        auto area = (COL_COUNT-1)*(ROW_COUNT-1);

        if(col == COL_COUNT-1 and row == ROW_COUNT-1) {
            kbd_state = KbdStateDefault {};
            return 0;
        }

        else if (row == ROW_COUNT-1){
            preset_category = col;
            return 0;
        }

        else if (col == COL_COUNT-1){
            preset_page += row==0 ? -1 : 1;

            auto lastpage = ( presets[preset_category]->size()+(area-1) ) / area;
            if(preset_page > lastpage)
                preset_page=lastpage;
            if(preset_page < 0)
                preset_page=0;

            return 0;
        }

        auto index = preset_page*area + col*(ROW_COUNT-1) + row;

        if(presets[preset_category]->size() > index)
            kbd_fifo_state = presets[preset_category]->operator[](index).c_str();

        return 0;
    }

    else kbd_state = KbdStateDefault {};

    return 0;
}

void draw_kbd(canvas::v16 screensize, bool shift){
    auto [cellsize_x, cellsize_y, y_start] = _kbd_draw_layout(screensize);

    // draw lines
    canvas::lcd.set_pen(0, 1);
    canvas::lcd.set_brush(0);

    canvas::lcd.rectangle({{0,y_start},{screensize.x, screensize.y}}, true);

    canvas::lcd.set_pen(0xFFFFFF, 1);
    for(int16_t x=0; x<screensize.x; x+=cellsize_x){
        canvas::lcd.line({{x, y_start} , {x, screensize.y}});
    }
    for(int16_t y=y_start; y<screensize.y; y+=cellsize_y){
        canvas::lcd.line({{0, y} , {screensize.x, y}});
    }

    // draw symbols
    if(std::holds_alternative<KbdStateMainRow>(kbd_state)){
        int16_t col = 2*std::get<KbdStateMainRow>(kbd_state).col;

        for(int16_t drow=0; drow<ROW_COUNT; drow++){
            for(int16_t dcol=0; dcol<COL_COUNT; dcol++){
                auto s = layouts[layout_index].main_row[drow][col+dcol];
                if(shift) s = apply_shift(layouts[layout_index].shift_table, s);
                canvas::lcd.symbol_cached(s, {cellsize_x*dcol+cellsize_x/4, y_start+cellsize_y*drow+cellsize_y/4});
            }
        }
    }
    else if (std::holds_alternative<KbdStateUpperRow>(kbd_state)){
        int16_t col = 2*(int)std::get<KbdStateUpperRow>(kbd_state).col;

        for(int16_t drow=0; drow<ROW_COUNT; drow++){
            for(int16_t dcol=0; dcol<COL_COUNT; dcol++){
                canvas::lcd.symbol_cached(upper_row_main[drow][col+dcol], {cellsize_x*dcol+cellsize_x/4, y_start+cellsize_y*drow+cellsize_y/4});
            }
        }
    }

    else if (std::holds_alternative<KbdStatePreset>(kbd_state)){
        for(int16_t dcol=0; dcol<COL_COUNT-1; dcol++){
            int16_t drow=ROW_COUNT-1;
            canvas::lcd.symbol_cached('1'+dcol, {cellsize_x*dcol+cellsize_x/4, y_start+cellsize_y*drow+cellsize_y/4});
        }

        auto area = (COL_COUNT-1)*(ROW_COUNT-1);

        for(int16_t drow=0; drow<ROW_COUNT-1; drow++){
            for(int16_t dcol=0; dcol<COL_COUNT-1; dcol++){
                auto index = preset_page*area + dcol*(ROW_COUNT-1) + drow;

                if(presets[preset_category]->size() > index){
                    auto sym = presets[preset_category]->operator[](index).c_str()[0];
                    canvas::lcd.symbol_cached(sym, {cellsize_x*dcol+cellsize_x/4, y_start+cellsize_y*drow+cellsize_y/4});
                }
            }
        }
    }
}

char32_t get_unicode_from_utf8(const char* _ch);

char16_t get_symbol_from_keyboard() {
    while(true){
        if(kbd_fifo_state){
            auto ret = get_unicode_from_utf8(kbd_fifo_state);
            kbd_fifo_state++;
            while((*kbd_fifo_state & 0b11000000) == 0b10000000)
                kbd_fifo_state++;
            if(!*kbd_fifo_state)
                kbd_fifo_state = nullptr;
            return ret;
        }

        canvas::v16 LCDSIZE = {canvas::lcd.XSIZE(), canvas::lcd.YSIZE()};

        draw_kbd(LCDSIZE, alt_pressed());

        auto touch = wait_for_touch();
        auto press = touch_to_keybutton(LCDSIZE, touch);

        char32_t ret = kbd_press(press, alt_pressed());
        if(ret)
            return ret;
    }
}

canvas::v16 wait_for_touch(){
	TS_StateTypeDef TS_State;
	canvas::v16 press_crd;

	bool touched = 0;
    while(1){
		BSP_TS_GetState(&TS_State);
		HAL_Delay(50);

		if (TS_State.TouchDetected) {
			touched = 1;
			press_crd = {int16_t(TS_State.X), int16_t(TS_State.Y)};
		}

		if (touched and not TS_State.TouchDetected) {
			return press_crd;
		}
    }
}

bool alt_pressed(){
	return GPIOA->IDR & 1;
}

extern "C"
char32_t symbol_input()
{
    /*if(kbd_fifo_state){
        auto ret = *(kbd_fifo_state++);
        if(!*kbd_fifo_state)
            kbd_fifo_state = nullptr;
        return ret;
    }

    while(true){
        canvas::v16 LCDSIZE = {canvas::lcd.XSIZE(), canvas::lcd.YSIZE()};

        draw_kbd(LCDSIZE, alt_pressed());

        auto touch = wait_for_touch();
        auto press = touch_to_keybutton(LCDSIZE, touch);

        char32_t ret = kbd_press(press, alt_pressed());
        if(ret)
            return ret;
    }*/
	get_symbol_from_keyboard();
}

