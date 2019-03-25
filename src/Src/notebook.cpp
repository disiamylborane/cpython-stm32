// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "notebook.h"
#include "Python.h"
#include <algorithm>
#include "stm32f429i_discovery_lcd.h"

extern"C" char32_t symbol_input();

static size_t delete_utf8_char(string& str, size_t index){
    size_t strindex=0;
	size_t ret = 0;
	for (auto c : str) 
    {
        if(c == '\02' or c == '\03')
            continue;
		if ((c & 0b11000000) != 0b10000000) {
            if(ret==index) {
                str.erase(strindex, 1);
                while((str[strindex] & 0b10000000) and not (str[strindex] & 0b01000000))
                    str.erase(strindex, 1);
            }
            ret++;
        }

        strindex++;
	}
	return ret;
}

static size_t console_utf8_char_count(const string& str) {
	size_t ret = 0;
	for (auto c : str) 
    {
        if(c == '\02' or c == '\03')
            continue;
		if ((c & 0b11000000) != 0b10000000)
            ret++;
	}
	return ret;
}

int get_string_pos_from_utf8_pos(const string& str, int _pos){
    if(_pos < 0)
        return -1;
    int real_pos = 0;
    int i=0;
    while(real_pos<_pos){
        if(str[i] == '\02' or str[i] == '\03')
            continue;
        i++;
		while (((char32_t)(unsigned)str[i] & 0b11000000) == 0b10000000)
            i++;
        real_pos++;
        if(i>str.size())
            return -1;
    }

    return i;
}

void insert_utf_char_into_string(string& str, char32_t ch, size_t _pos) {
    auto i = get_string_pos_from_utf8_pos(str, _pos);
    if(i==-1) return;

	if(ch < 0x7F) {
        char c = ch;
        str.insert(i, 1, c);
    }
    else if(ch < 0x7FF) {
        char s2[2] = {
            char((ch >> 6) | 0b11000000),
            char((ch & 0b111111) | 0b10000000)
        };

        str.insert(i, s2, std::size(s2));
    }
    else if(ch < 0xFFFF) {
        char s3[3] = {
            char((ch >> 12) | 0b11100000),
            char(((ch >> 6) & 0b111111) | 0b10000000),
            char((ch & 0b111111) | 0b10000000),
        };

        str.insert(i, s3, std::size(s3));
    }
    else if(ch < 0x10FFFF) {
        char s4[4] = {
            char((ch >> 18) | 0b11110000),
            char(((ch >> 12) & 0b111111) | 0b10000000),
            char(((ch >> 6) & 0b111111) | 0b10000000),
            char((ch & 0b111111) | 0b10000000),
        };

        str.insert(i, s4, std::size(s4));
    }
    else
        abort();//throw std::runtime_error("UTF8 conversion error");
}

static canvas::v16 get_utf8_dimensions(const char* s, int string_xlen) {
    if(string_xlen <= 0)
        string_xlen = __INT_MAX__;

	canvas::v16 ret = {0, 1};
	int16_t curr_x = 0;
    
	for (size_t i = 0; s[i]; i++)
    {
        if(s[i] == '\02' or s[i] == '\03')
            continue;
        else if(s[i] == '\n')
        {
            ret.y++;
            ret.x = std::max(ret.x, curr_x);
            curr_x = 0;
            continue;
        }
		else if ((s[i] & 0b11000000) != 0b10000000)
        {
            if(curr_x >= string_xlen) {
                ret.x = string_xlen;
                curr_x=0;
                ret.y++;
            }
            curr_x++;
        }
	}
    ret.x = std::max(ret.x, curr_x);
    return ret;
}

char32_t get_unicode_from_utf8(const char* _ch) {
	for (size_t i = 0; ; i++)
    {
        char32_t ch = (unsigned char)_ch[i];
        if((ch & 0b10000000) == 0 and (ch | 0b01111111) == 0b01111111)
            return char32_t(ch);
        else if((ch & 0b11100000) == 0b11000000 and (ch | 0b00011111) == 0b11011111)
        {
            return ((char32_t)(ch & 0b00011111) << 6) | ((char32_t)_ch[i+1] & 0b00111111);
        }
        else if((ch & 0b11110000) == 0b11100000 and (ch | 0b00001111) == 0b11101111)
        {
            return ((char32_t)(ch & 0b00011111) << 12) | ((char32_t)(_ch[i+1] & 0b00111111) << 6) | ((char32_t)_ch[i+2] & 0b00111111);
        }
        else 
        {
            return ((char32_t)(ch & 0b00011111) << 18) | ((char32_t)(_ch[i+1] & 0b00111111) << 12)
                | ((char32_t)(_ch[i+2] & 0b00111111) << 6) | ((char32_t)_ch[i+3] & 0b00111111);
        }
	}
    return char32_t(-1);
}

canvas::Picture* DisplayLine::get_picture(const DisplayParameters& params) {
    if(!cache)
        update(params);
    return cache.get();
};

StringDisplayLine& StringDisplayLine::operator<<(const char* data) {
    value.append(data);
    return *this;
}

StringDisplayLine& StringDisplayLine::operator<<(Colour colour)
{
    for(uint8_t i = 0; i<(uint8_t)colour; i++)
        value.push_back('\02');
    return *this;
}

StringDisplayLine& StringDisplayLine::operator<<(__ResetColour__ _dummy)
{
    value.push_back('\03');
    return *this;
}

static const uint32_t string_colors[] = {0xFFFFFF, 0x70D0FF, 0xFF8050, 0x60FF60, 0xFF6060};

canvas::v16 StringDisplayLine::__rebuild(int __cursor_location, const DisplayParameters& params)
{
    size_t cursor_location = get_string_pos_from_utf8_pos(value, __cursor_location);
    int string_xlen = params.symbol_wrap ? (canvas::lcd.XSIZE()/params.fontsize.x) : -1;
    canvas::v16 newsize = get_utf8_dimensions(value.c_str(), string_xlen) + canvas::v16{1,0};
    canvas::v16 position = {0,0};
    canvas::v16 ret = {0, 0};

    if(!cache)
        cache = unique_ptr<canvas::Picture>(new canvas::Picture({newsize.x * params.fontsize.x, newsize.y * params.fontsize.y}));
    else
        cache->reset({newsize.x * params.fontsize.x, newsize.y * params.fontsize.y});

    rpixel_t color = canvas::make_internal_pixel(string_colors[0]);

    canvas::cache_font(params.fontsize);

    cache->set_pen(string_colors[0], 1);

    size_t i = 0;
	for (; value[i]; i++)
    {
        if(cursor_location == i)
            ret = position;
        if(value[i] == '\02'){
            int cadd = 1;
            for(; value[i+1] == '\02'; i++, cadd++);
            cache->set_pen(string_colors[cadd % std::size(string_colors)], 1);
            continue;
        }
        else if(value[i] == '\03') {
            cache->set_pen(string_colors[0], 1);
        }
        else if(value[i] == '\n'){
            position.x = 0;
            position.y+=params.fontsize.y;
        }
		else if ((value[i] & 0b11000000) == 0b10000000){
            continue;
        }
        else {
            cache->symbol_cached(get_unicode_from_utf8(&(value.c_str()[i])), {position.x,position.y});
            position.x += params.fontsize.x;
            if(params.symbol_wrap and position.x >= canvas::lcd.XSIZE()){
                position.x = 0;
                position.y += params.fontsize.y;
            }
        }
    }
    if(cursor_location == i)
        ret = position;

    return ret;
}

void StringDisplayLine::update(const DisplayParameters& params)
{
    __rebuild(-1, params);
}

void StringInputLine::update(const DisplayParameters& params){
    auto cur_disp = __rebuild(cursor_pos, params);
    if(cursor_pos>=0){
        cache->set_pen(0xFFFFFF,1);
        cache->line({cur_disp, {cur_disp.x, cur_disp.y + params.fontsize.y} });
    }
}

canvas::v16 StringInputLine::set_cursor_pos(int location, DisplayParameters& params)
{
    if(location < 0)
        cursor_pos = -1;
    else {
        cursor_pos = std::min(location, utf8_string_size);
    }

    auto cur_disp = __rebuild(cursor_pos, params);
    if(cursor_pos>=0){
        cache->set_pen(0xFFFFFF,1);
        cache->line({cur_disp, {cur_disp.x, cur_disp.y + params.fontsize.y} });
    }
    cursor_coords = cur_disp;
    return cur_disp;
}

int StringInputLine::get_cursor_pos() {return cursor_pos;}

int StringInputLine::get_max_cursor_pos() {
    return console_utf8_char_count(value);
}

canvas::v16 StringInputLine::utilize_character(char32_t character, DisplayParameters& params){
    if(cursor_pos<0 or character == 0)
        return set_cursor_pos(console_utf8_char_count(value), params);

    if(character == 8){
        if(value.size() and cursor_pos){
            delete_utf8_char(value, cursor_pos-1);
            return set_cursor_pos(cursor_pos - 1, params);
        }

        return cursor_coords;
    }
    else {
        insert_utf_char_into_string(value, character, cursor_pos);
        utf8_string_size = console_utf8_char_count(value);
        return set_cursor_pos(cursor_pos + 1, params);
    }
}

PixelsDisplayLine::PixelsDisplayLine(canvas::v16 size)
: value(size.x * size.y), row_size(size.x), front_color(0xFFFFFF), back_color(0)
{
}

void PixelsDisplayLine::set_pixel(uint16_t x, uint16_t y, bool on)
{
    if(row_size == 0) return;

    if(x < row_size and y < value.size()/row_size) {
        value[row_size*y + x] = on;
    }
}

void PixelsDisplayLine::update(const DisplayParameters& params)
{
    if(row_size == 0) return;

    auto XX = row_size;
    auto YY = value.size()/row_size;

    cache = make_unique<canvas::Picture>(canvas::v16{row_size, YY});

    auto i=0;
    for(size_t y=0; y<YY; y++){
        for(size_t x=0; x<XX; x++){
            cache->pixel(x,y,value[i] ? front_color : back_color);
            i++;
        }
    }
}

void Notebook::set_current_command_result(unique_buffer result)
    {__active_cell().result = std::move(result);}

void* Notebook::get_command_result(size_t cell_index)
    {return cells[cell_index].result.get();}

void Notebook::add_output_line(unique_ptr<DisplayLine> line)
{
    // Insert the line into the cell
    auto &cell = __active_cell();
    cell.outputs.push_back({cell.next_subcell_offset, std::move(line)});
    ConsoleCell::Line &ln = cell.outputs.back();

    // Update line offsets
    auto pic = ln.ln->get_picture(params);
    auto linesize = pic->size().y;

    cell.next_subcell_offset += linesize;
    for(auto it = cells.begin()+active_cell_index+1; it < cells.end(); it++){
        it->console_offset += linesize;
    }

    current_offset += linesize;
}

int Notebook::search_render_first()
{
    if(cells.size() == 0)
        return 0;
    
    int from = 0;
    int to = cells.size()-1;
    
    while(1) {
        if(to-from <= 1)
            return from;
        int center = (from+to) / 2;
        if(cells[center].console_offset > pos.y)
            to = center;
        else if(cells[center].console_offset < pos.y)
            from = center;
        else if(cells[center].console_offset == pos.y)
            return center;
    }
}

void Notebook::draw()
{
    using canvas::lcd;
    auto height = y_render_limit;

    lcd.clear(0x000000);

    int render_first = search_render_first();

    for(auto cell = cells.begin() + render_first; cell<cells.end(); cell++) {
        // Draw input line
        {
            signed screenoffset = (signed)cell->console_offset - pos.y;
            if(screenoffset > height)
                return;

            auto inputpic = cell->input.get_picture(params);
            if(inputpic){
                lcd.picture(*inputpic, {-pos.x, (signed)cell->console_offset - pos.y});
            }
        }

        // Draw output lines
        for(auto &out: cell->outputs) {
            signed screenoffset = (signed)cell->console_offset + (signed)out.cell_offset - pos.y;
            if(screenoffset > height)
                return;

            auto outpic = out.ln->get_picture(params);
            if(outpic){
                lcd.picture(*outpic, {-pos.x, screenoffset});
            }
        }
    }
}

Notebook::Notebook(void (*_command_executer)(const char* command, Notebook& output), DisplayParameters _params, uint16_t _y_render_limit) 
    : command_executer(_command_executer), params(_params), y_render_limit(_y_render_limit)
    {
        auto csize = cells[0].input.get_picture(params)->size().y;
        cells[0].next_subcell_offset = csize;
        current_offset = csize;

        draw();
    }

void Notebook::scroll_y(decltype(canvas::v16::y) Y)
{
    if(pos.y+y_render_limit-params.fontsize.y < Y) {
        pos.y = Y - y_render_limit + params.fontsize.y;
    }
    if(pos.y > Y) {
        pos.y = Y;
    }
}
void Notebook::scroll(canvas::v16 coord)
{
    scroll_y(coord.y);
    if(pos.x+canvas::lcd.XSIZE() < coord.x) {
        pos.x = coord.x - canvas::lcd.XSIZE() + params.fontsize.x;
    }
    if(pos.x > coord.x) {
        pos.x = coord.x;
    }
}

void Notebook::interact()
{
    if(cells.size() == 0)
    	abort();//throw "The notebook must have a line at init, but it doesn't"s;

    while(true) {
        auto old_ysize = cells[active_cell_index].input.get_picture(params)->size().y;
        char32_t input = symbol_input();

        switch(input)
        {
            case CHAR_ENTER_NEWLINE_BTN0:{
                auto &currcell = cells[active_cell_index];
                currcell.input.set_cursor_pos(-1, params);

                auto output_count = currcell.outputs.size();
                int delta = output_count ? currcell.next_subcell_offset - currcell.outputs[0].cell_offset : 0;
                currcell.next_subcell_offset -= delta;
                for(auto it = cells.begin() + active_cell_index + 1; it < cells.end(); it++){
                    it->console_offset -= delta;
                }
                current_offset -= delta;
                currcell.outputs.clear();

                command_executer(currcell.input.get_value(), *this);
                if(active_cell_index == cells.size()-1){
                    cells.emplace_back();
                    cells.back().console_offset = current_offset;
                    cells.back().next_subcell_offset = cells.back().input.get_picture(params)->size().y;
                    current_offset += cells.back().input.get_picture(params)->size().y;
                }
                else {
                    cells[active_cell_index+1].input.utilize_character(0,params);
                }
                active_cell_index++;

                scroll({0, cells[active_cell_index].console_offset});

                draw();
                break;
            }
            case CHAR_BTNHIDDEN_NULL:{
                return;
                break;
            }
            case CHAR_BGARROWRT_ENC0PLUS_ALT:{
                if(pos.x < INT16_MAX - params.fontsize.x){
                    pos.x += params.fontsize.x;
                    draw();
                }
                break;
            }
            case CHAR_BGARROWLT_ENC0MINUS_ALT:{
                if(pos.x >= params.fontsize.x){
                    pos.x -= params.fontsize.x;
                    draw();
                }
                break;
            }
            case CHAR_BGARROWUP_ENC1PLUS_ALT:{
                if(pos.y >= params.fontsize.y){
                    pos.y -= params.fontsize.y;
                    draw();
                }
                break;
            }
            case CHAR_BGARROWDN_ENC1MINUS_ALT:{
                if(pos.y < INT16_MAX - params.fontsize.y){
                    pos.y += params.fontsize.y;
                    draw();
                }
                break;
            }
            case CHAR_ARROWRT_ENC0PLUS:{
                auto &ln = cells[active_cell_index].input;
                auto show = ln.set_cursor_pos(std::min(ln.get_max_cursor_pos(), ln.get_cursor_pos()+1), params);
                scroll(show + canvas::v16{0,cells[active_cell_index].console_offset});
                draw();
                break;
            }
            case CHAR_ARROWLT_ENC0MINUS:{
                auto &ln = cells[active_cell_index].input;
                auto show = ln.set_cursor_pos(std::max(0, ln.get_cursor_pos()-1), params);
                scroll(show + canvas::v16{0,cells[active_cell_index].console_offset});
                draw();
                break;
            }
            case CHAR_ARROWUP_ENC1PLUS:{
                if(active_cell_index == 0)
                    continue;

                cells[active_cell_index].input.set_cursor_pos(-1, params);
                active_cell_index--;
                cells[active_cell_index].input.utilize_character(0, params);
                auto show = cells[active_cell_index].input.get_cursor_coords();
                if(show)
                    scroll(*show + canvas::v16{0,cells[active_cell_index].console_offset});
                draw();
                break;
            }
            case CHAR_ARROWDN_ENC1MINUS:{       
                if(active_cell_index == cells.size()-1)
                    continue;

                cells[active_cell_index].input.set_cursor_pos(-1, params);
                active_cell_index++;
                cells[active_cell_index].input.utilize_character(0, params);
                auto show = cells[active_cell_index].input.get_cursor_coords();
                if(show)
                    scroll(*show + canvas::v16{0,cells[active_cell_index].console_offset});
                draw();
                break;
            }
        
            default: {
                auto &input_line = cells[active_cell_index].input;
                auto cursor_pos = input_line.utilize_character(input, params);
                int16_t new_ysize = input_line.get_picture(params)->size().y;
                if(new_ysize != old_ysize){
                    auto delta = new_ysize - old_ysize;
                    for(auto &ln : cells[active_cell_index].outputs){
                        ln.cell_offset += delta;
                    }
                    cells[active_cell_index].next_subcell_offset += delta;
                    for(auto it = cells.begin() + active_cell_index + 1; it < cells.end(); it++){
                        it->console_offset += delta;
                    }
                    current_offset += delta;
                    old_ysize = new_ysize;
                }

                scroll(cursor_pos + canvas::v16{0, cells[active_cell_index].console_offset});

                draw();
                break;
            }
        }
    };
}


static Notebook* nb;
PyObject *mod, *globals, *code;

void executer(const char* command, Notebook& output)
{
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);

    bool expr;

    code = Py_CompileStringExFlags(command, "<unb>", Py_eval_input, 0, 0);
    if (code == NULL) {
        PyErr_Restore(type, value, traceback);
        code = Py_CompileStringExFlags(command, "<unb>", Py_file_input, 0, 0);
        expr = false;
    }
    else {
        PyErr_Restore(type, value, traceback);
        expr = true;
    }

    if (code == NULL) {
        PyErr_Print();
    }
    else {
        PyObject* ret = PyEval_EvalCode(code, globals, globals);
        if (ret == NULL) {
            PyErr_Print();
        }
        else if(ret == Py_None){
            Py_DECREF(ret);
        }
        else {
            PyObject* repr = PyObject_Repr(ret);
            if(!repr){
                PyErr_Print();
            }
            else{
                const char* srepr = PyUnicode_AsUTF8(repr);

                auto ln = unique_ptr<StringDisplayLine>(new StringDisplayLine( "\2"s + string(srepr) ));
                nb->add_output_line((unique_ptr<DisplayLine>&&)(ln));

                Py_DECREF(ret);
                Py_XDECREF(repr);
            }
        }
    }
}


extern "C"
void init_notebook()
{
	nb = new Notebook(executer, DisplayParameters{canvas::v16{17,24}, true}, 210);
}
extern "C"
void free_notebook()
{
	delete nb;
}

extern "C"
int write_screen(const void * buffer, size_t count)
{
	auto ln = unique_ptr<StringDisplayLine>(new StringDisplayLine(string((char*)buffer, count)));
	nb->add_output_line((unique_ptr<DisplayLine>&&)(ln));
	return count;
}

extern "C"
void main_cycle()
{
	mod = PyImport_AddModule("__main__");
	if (mod == NULL)
		return;
	globals = PyModule_GetDict(mod);

	nb->interact();
}
