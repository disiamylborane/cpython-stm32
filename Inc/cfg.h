
#pragma once

#include <string.h>
#include "general"

constexpr char32_t CHAR_BTNHIDDEN_NULL          { '\0' };
constexpr char32_t CHAR_NEWCOLOR                { '\2' };
constexpr char32_t CHAR_OLDCOLOR                { '\3' };
constexpr char32_t CHAR_ENTER_NEWLINE_BTN0      { '\r' };
constexpr char32_t CHAR_ESCAPE_BTN1             {'\x1b'};

constexpr char32_t CHAR_ARROWRT_ENC0PLUS        {0x2192};
constexpr char32_t CHAR_ARROWLT_ENC0MINUS       {0x2190};
constexpr char32_t CHAR_ARROWUP_ENC1PLUS        {0x2191};
constexpr char32_t CHAR_ARROWDN_ENC1MINUS       {0x2193};

constexpr char32_t CHAR_BGARROWRT_ENC0PLUS_ALT  {  17  };
constexpr char32_t CHAR_BGARROWLT_ENC0MINUS_ALT {  18  };
constexpr char32_t CHAR_BGARROWUP_ENC1PLUS_ALT  {  30  };
constexpr char32_t CHAR_BGARROWDN_ENC1MINUS_ALT {  31  };

namespace canvas {
    enum Orientation {Portrait, Album};

    struct v16 {
        int16_t x;
        int16_t y;

        v16 operator + (const v16& oth) {return v16{x+oth.x, y+oth.y};}
        v16 operator - (const v16& oth) {return v16{x-oth.x, y-oth.y};}
    };
    struct rect16 {
        v16 p1;
        v16 p2;
    };
    typedef uint16_t internal_pixel_t;

    inline canvas::internal_pixel_t make_internal_pixel(uint32_t XRGB){
        uint16_t b = (XRGB >> 3) & 0b0000000000011111;
        uint16_t g = (XRGB >> 5) & 0b0000011111100000;
        uint16_t r = (XRGB >> 8) & 0b1111100000000000;
        return r|b|g;
    }
    inline uint32_t XRGB_from_internal_pixel(internal_pixel_t ip){
        uint32_t b = (ip & 0b0000000000011111) << 3;
        uint32_t g = (ip & 0b0000011111100000) << 5;
        uint32_t r = (ip & 0b1111100000000000) << 8;
        return r|b|g;
    }
    inline uint32_t ARGB_from_internal_pixel(internal_pixel_t ip){
        return XRGB_from_internal_pixel(ip) | 0xFF000000u;
    }

    constexpr int16_t LCD_DISPLAY_XSIZE = 238;
    constexpr int16_t LCD_DISPLAY_YSIZE = 320;

    class Picture;
    class Image {
    protected:
        virtual void draw_pixel_unsafe(int16_t X, int16_t Y, internal_pixel_t color) = 0;
        virtual void draw_hline_unsafe(v16 point, int16_t x2, internal_pixel_t colour) = 0;
        virtual void draw_vline_unsafe(v16 point, int16_t y2, internal_pixel_t colour) = 0;

        void reorient(int16_t &x, int16_t &y);
        void draw_line_unsafe(rect16 points, internal_pixel_t colour);

        internal_pixel_t brush_colour;
        internal_pixel_t pen_colour;
        uint8_t pen_width;

    public:
        virtual void clear(int32_t XRGB) = 0;
        virtual v16 size() const=0;
        virtual ~Image(){};

    public:
        void set_pen(uint32_t XRGB, uint8_t width);
        void set_brush(uint32_t XRGB);

        void picture(const Picture& pict, v16 pos);
        void line(rect16 points);
        void circle(int16_t x, int16_t y, uint16_t r, bool fill);
        void rectangle(rect16 rect, bool fill);
        void pixel(int16_t X, int16_t Y, int32_t XRGB);
        void symbol(char32_t symbol, v16 pos, v16 size);
        void text(const char* string, v16 pos, v16 size);
        void symbol_cached(char32_t symbol, v16 pos);
        void text_cached(const char* string, v16 pos);
    };

    class Display : public Image {
    protected:
        Orientation orient;
        virtual void draw_pixel_unsafe(int16_t X, int16_t Y, internal_pixel_t color) override;
        //virtual void draw_line_unsafe(rect16 points, internal_pixel_t colour) override;
        virtual void draw_hline_unsafe(v16 point, int16_t x2, internal_pixel_t colour) override;
        virtual void draw_vline_unsafe(v16 point, int16_t y2, internal_pixel_t colour) override;

    public:
        Display() : orient(Portrait) {};

        virtual void clear(int32_t XRGB) override;
        virtual ~Display(){};

        constexpr int16_t SMALLER_SIZE() const {return LCD_DISPLAY_XSIZE;};
        constexpr int16_t LARGER_SIZE() const {return LCD_DISPLAY_YSIZE;};
        void set_orientation(Orientation neworient);
        Orientation get_orientation() const {return orient;}
        inline int16_t XSIZE() const {return get_orientation()==Portrait ? SMALLER_SIZE() : LARGER_SIZE(); }
        inline int16_t YSIZE() const {return get_orientation()==Portrait ? LARGER_SIZE() : SMALLER_SIZE(); }
        virtual v16 size() const override {return {XSIZE(), YSIZE()};}

        //static Display instance;
    };

    inline Display lcd;

    class Picture : public Image {
    protected:
        unique_ptr<internal_pixel_t[]> buffer;
        v16 picsize;
        virtual void draw_pixel_unsafe(int16_t X, int16_t Y, internal_pixel_t color) override;
        //virtual void draw_line_unsafe(rect16 points, internal_pixel_t colour) override;
        virtual void draw_hline_unsafe(v16 point, int16_t x2, internal_pixel_t colour) override;
        virtual void draw_vline_unsafe(v16 point, int16_t y2, internal_pixel_t colour) override;

    public:
        virtual void clear(int32_t XRGB) override;
        virtual v16 size() const override {return picsize;};
        virtual ~Picture(){};

        size_t get_buffer_size() {return sizeof(internal_pixel_t) * picsize.x * picsize.y;};

        void resize(v16 newsize);
        void reset(v16 newsize);

        // TODO: remove error minimization
        internal_pixel_t get_pixel(v16 point) const {
            if(point.x>=0 and point.x<picsize.x and point.y>=0 and point.y<picsize.y) 
                return buffer[picsize.x * point.y + point.x];
            else
                return 0;
        }

        Picture(v16 _size);
    };

    void cache_font(v16 fsize);

}
