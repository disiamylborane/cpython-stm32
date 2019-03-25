
#pragma once

#include <gsl/span>
#include "cfg.h"
using gsl::span;


using rpixel_t = canvas::internal_pixel_t; // rendered pixel type

struct DisplayParameters {
    canvas::v16 fontsize;
    bool symbol_wrap;
};

class DisplayLine
{
protected:
    unique_ptr<canvas::Picture> cache; // optional picture cache

public:
    //canvas::v16 get_cached_picture_size() {return cache->size();};
    //size_t get_cached_memory_size() {return cache->get_buffer_size();};

    canvas::Picture* get_picture(const DisplayParameters& params); // caches picture if not cached, than return a reference to it

    virtual void drop() {cache.reset(nullptr);}; // delete cache to free memory, can be overriden
    virtual void update(const DisplayParameters& params) = 0; // update cache

public:
    virtual ~DisplayLine(){};
    DisplayLine(): cache(nullptr) {};
    DisplayLine(DisplayLine&& dl): cache(std::move(dl.cache)) {};
};

class StringDisplayLine : public DisplayLine
{
protected:
    string value;
    //canvas::v16 __append(int cursor_location, const DisplayParameters& params);  // A helper for <update>
    canvas::v16 __rebuild(int cursor_location, const DisplayParameters& params);  // A helper for <update>

public:
    virtual void update(const DisplayParameters& params) override;

public: // StringDisplayLine specific functions
    enum Colour : uint8_t {Highlight1 = 1, Highlight2 = 2, RedAlert = 3, GreenAlert = 4};
    enum __ResetColour__ {ResetColour};

    void operator=(string &&data) {value = std::move(data);};
    StringDisplayLine& operator<<(const char* data);
    StringDisplayLine& operator<<(Colour colour);
    StringDisplayLine& operator<<(__ResetColour__ _dummy);
    
public:
    virtual ~StringDisplayLine() override {};
    StringDisplayLine(): DisplayLine(), value(){};
    StringDisplayLine(StringDisplayLine&& sdl): DisplayLine(std::move(sdl)), value(std::move(sdl.value)) {};

    StringDisplayLine(string&& data) : DisplayLine(), value(std::move(data)){};
};

class StringInputLine : public StringDisplayLine
{
    int cursor_pos;
    int utf8_string_size;
    canvas::v16 cursor_coords;

public:
    virtual void update(const DisplayParameters& params) override;

    // Automatically updates
    // Returns the cursor position vs the picture position (cached)
    canvas::v16 set_cursor_pos(int location, DisplayParameters& params);
    optional<canvas::v16> get_cursor_coords() {if(cursor_pos<0) return std::nullopt; else return cursor_coords;}
    int get_cursor_pos();
    int get_max_cursor_pos();

    // Automatically updates
    // Returns the cursor position vs the picture position (cached)
    canvas::v16 utilize_character(char32_t character, DisplayParameters& params);

    const char* get_value(){return value.c_str();};
    
public:
    virtual ~StringInputLine() override {};
    StringInputLine() : StringDisplayLine(), cursor_pos(0), utf8_string_size(0), cursor_coords({0,0}){};
    StringInputLine(StringInputLine&& sil): 
        StringDisplayLine((StringDisplayLine&&)sil),
        cursor_pos(sil.cursor_pos),
        utf8_string_size(sil.utf8_string_size),
        cursor_coords(sil.cursor_coords)
        {};
};

// Monochrome picture (foreground+background)
class PixelsDisplayLine : public DisplayLine
{
    vector<bool> value;
    uint8_t row_size;
    uint32_t front_color, back_color;
public:

    virtual void update(const DisplayParameters& params) override;

public:
    PixelsDisplayLine(canvas::v16 size);

    void set_pixel(uint16_t x, uint16_t y, bool on); // Return false if pixel doesn't exist

    void set_back_color(uint32_t XRGB) {back_color = XRGB;};
    void set_front_color(uint32_t XRGB) {front_color = XRGB;};

    virtual ~PixelsDisplayLine() override {};
};

class PictureDisplayLine : public DisplayLine
{
    //canvas::Picture pic;
public:
    PictureDisplayLine(canvas::Picture &&pic);

    virtual void update(const DisplayParameters& params) override;
    virtual void drop() override {};
    
    virtual ~PictureDisplayLine(){};
};

class Notebook
{
    struct ConsoleCell {
        struct Line {
            uint32_t cell_offset; // Y position relative to parent (ConsoleCell)
            unique_ptr<DisplayLine> ln;  // Line to display
        };

        uint32_t console_offset; // Y position relative to parent (Console)
        StringInputLine input;

        vector<Line> outputs;
        uint32_t next_subcell_offset; // Y position relative to parent (Console)

        unique_buffer result;

        ConsoleCell() : console_offset(0), input(), outputs(), next_subcell_offset(0), result(nullptr){};
    };

    vector<ConsoleCell> cells {1};

    uint32_t current_offset{0};
    size_t active_cell_index{0};

    uint16_t y_render_limit;

    void (*command_executer)(const char* command, Notebook& output);

    ConsoleCell& __active_cell() {return cells[active_cell_index];}

    DisplayParameters params;

    canvas::v16 pos {0,0}; // Current top left corner

    int search_render_first();
    void draw();

    void scroll_y(decltype(canvas::v16::y) Y);
    void scroll(canvas::v16 coord);

public:  // command_executer interface
    void set_current_command_result(unique_buffer result);
    void add_output_line(unique_ptr<DisplayLine> line);
    void* get_command_result(size_t cell_index);

public:  // user app interface
    Notebook(void (*_command_executer)(const char* command, Notebook& output), DisplayParameters _params, uint16_t y_render_limit);
    void interact();

    void set_y_render_limit(uint16_t limit) {y_render_limit=limit;};
};

