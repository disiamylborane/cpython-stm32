// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "cfg.h"
#include "stm32f429i_discovery_lcd.h"

static void draw_pixel_unsafe(int16_t X, int16_t Y, canvas::internal_pixel_t ip)
{
    BSP_LCD_DrawPixel(X,Y,canvas::ARGB_from_internal_pixel(ip));
}

static void draw_hline_unsafe(int16_t X1, int16_t X2, int16_t Y, canvas::internal_pixel_t ip)
{
    if(X1>X2) std::swap(X1,X2);
    for(int x = X1; x<=X2; x++)
    	draw_pixel_unsafe(x,Y,ip);
}
static void draw_vline_unsafe(int16_t Y1, int16_t Y2, int16_t X, canvas::internal_pixel_t ip)
{
    if(Y1>Y2) std::swap(Y1,Y2);
    for(int y = Y1; y<=Y2; y++)
        draw_pixel_unsafe(X,y,ip);
}
/*void draw_sym(int16_t X, int16_t Y, char c, canvas::v16 size, sf::Color Colour)
{
    sf::Text txt;
    txt.setFont(font);
    txt.setCharacterSize(size.y);
    txt.setFillColor(Colour);

    char sdd[2] = {c,0};
    txt.setString(sdd);
    txt.setPosition(X, Y);
    img.draw(txt);
}*/

namespace canvas {
    v16 system_font_cache = {8,12};
    void cache_font(v16 fsize)
    {
        system_font_cache = fsize;
    }

    // unsafe: check X < XSIZE and Y < YSIZE, check X and Y swapped due to orientation
    void Display::draw_pixel_unsafe(int16_t X, int16_t Y, internal_pixel_t color)
    {
        if(orient == Album){
            swap(X,Y);
        }
        if(X >= SMALLER_SIZE() or Y >= LARGER_SIZE()) return;
        if(X < 0 or Y < 0 ) return;
        ::draw_pixel_unsafe(X, Y, color);
    }

    void Display::clear(int32_t XRGB)
    {
        auto c = make_internal_pixel(XRGB);
        auto opc =pen_colour;
        auto obc =brush_colour;
        pen_colour = c;
        brush_colour = c;
        rectangle(rect16{0,0,XSIZE(),YSIZE()}, true);
        pen_colour=opc;
        brush_colour=obc;
    }

    // unsafe: check X < XSIZE and Y < YSIZE, check X and Y swapped due to orientation
    void Image::draw_line_unsafe(rect16 points, internal_pixel_t colour) 
    {
        const int deltax = points.p2.x - points.p1.x;
        const int deltay = points.p2.y - points.p1.y;
    
        if(deltax==0) return draw_vline_unsafe(points.p1, points.p2.y, colour);
        if(deltay==0) return draw_hline_unsafe(points.p1, points.p2.x, colour);

        const int deltaerr = deltay * (1<<4) / deltax;
        const int sign_deltay = deltay > 0 ? 1 : -1;

        int error = 0;
        int y = points.p1.y;
        for(int x = points.p1.x; x<=points.p2.x; x++) {
            draw_pixel_unsafe(x, y, colour);
            error += deltaerr;
            while(error > (1<<4)/2){
                y += std::abs(deltay);
                error--;
            }
        }
    }
    // unsafe: check X < XSIZE and Y < YSIZE, check X and Y swapped due to orientation
    void Display::draw_hline_unsafe(v16 point, int16_t x2, internal_pixel_t colour)
    {
        point.x = std::max((int16_t)0, std::min(point.x, (int16_t)(XSIZE()-1)));
        point.y = std::max((int16_t)0, std::min(point.y, (int16_t)(YSIZE()-1)));
        x2 = std::max((int16_t)0, std::min(x2, (int16_t)(XSIZE()-1)));

        if(orient == Portrait)
            ::draw_hline_unsafe(point.x, x2, point.y, colour);
        else
            ::draw_vline_unsafe(point.x, x2, point.y, colour);
    }
    // unsafe: check X < XSIZE and Y < YSIZE, check X and Y swapped due to orientation
    void Display::draw_vline_unsafe(v16 point, int16_t y2, internal_pixel_t colour)
    {
        point.x = std::max((int16_t)0, std::min(point.x, (int16_t)(XSIZE()-1)));
        point.y = std::max((int16_t)0, std::min(point.y, (int16_t)(YSIZE()-1)));
        y2 = std::max((int16_t)0, std::min(y2, (int16_t)(YSIZE()-1)));

        if(orient == Portrait)
            ::draw_vline_unsafe(point.y, y2, point.x, colour);
        else
            ::draw_hline_unsafe(point.y, y2, point.x, colour);
    }

    void Image::pixel(int16_t X, int16_t Y, int32_t XRGB)
    {
        draw_pixel_unsafe(X,Y,make_internal_pixel(XRGB));
    }

    void Image::circle(int16_t x0, int16_t y0, uint16_t r, bool fill)
    {
        int16_t x = r-1;
        int16_t y = 0;
        int16_t dx = 1;
        int16_t dy = 1;
        int16_t error = dx - r*2;

        while (x >= y) {
            if(fill) {
                draw_line_unsafe({x0 + y, y0 + x, x0, y0}, brush_colour);
                draw_line_unsafe({x0 - y, y0 + x, x0, y0}, brush_colour);
                draw_line_unsafe({x0 - x, y0 + y, x0, y0}, brush_colour);
                draw_line_unsafe({x0 - x, y0 - y, x0, y0}, brush_colour);
                draw_line_unsafe({x0 - y, y0 - x, x0, y0}, brush_colour);
                draw_line_unsafe({x0 + y, y0 - x, x0, y0}, brush_colour);
                draw_line_unsafe({x0 + x, y0 - y, x0, y0}, brush_colour);
            }
            draw_pixel_unsafe(x0 + x, y0 + y, pen_colour);
            draw_pixel_unsafe(x0 + y, y0 + x, pen_colour);
            draw_pixel_unsafe(x0 - y, y0 + x, pen_colour);
            draw_pixel_unsafe(x0 - x, y0 + y, pen_colour);
            draw_pixel_unsafe(x0 - x, y0 - y, pen_colour);
            draw_pixel_unsafe(x0 - y, y0 - x, pen_colour);
            draw_pixel_unsafe(x0 + y, y0 - x, pen_colour);
            draw_pixel_unsafe(x0 + x, y0 - y, pen_colour);
            if (error <= 0) {
                y++;
                error += dy;
                dy += 2;
            }
            
            if (error > 0) {
                x--;
                dx += 2;
                error += dx - r*2;
            }
        }
    }

    void Image::set_pen(uint32_t XRGB, uint8_t width)
    {
        pen_colour = make_internal_pixel(XRGB);
        pen_width = width;
    }

    void Image::set_brush(uint32_t XRGB)
    {
        brush_colour = make_internal_pixel(XRGB);
    }

    void Image::line(rect16 points)
    {
        // unsafe: check X < XSIZE and Y < YSIZE, check X and Y swapped due to orientation
        draw_line_unsafe(points, pen_colour);
    }

    void Image::rectangle(rect16 rect, bool fill){        
        if(rect.p1.x > rect.p2.x)
            std::swap(rect.p1.x, rect.p2.x);
        if(rect.p1.y > rect.p2.y)
            std::swap(rect.p1.y, rect.p2.y);

        if(fill)
            for(int16_t y = rect.p1.y+1; y < rect.p2.y; y++)
                draw_hline_unsafe({rect.p1.x + 1, y}, rect.p2.x-1, brush_colour);

        draw_hline_unsafe({rect.p1.x, rect.p1.y}, rect.p2.x, pen_colour);
        draw_hline_unsafe({rect.p1.x, rect.p2.y}, rect.p2.x, pen_colour);
        draw_vline_unsafe({rect.p1.x, rect.p1.y}, rect.p2.y, pen_colour);
        draw_vline_unsafe({rect.p2.x, rect.p1.y}, rect.p2.y, pen_colour);
    }

    void Image::symbol(char32_t symbol, v16 pos, v16 size)
    {
    	if(symbol < 0x20 or symbol > 0x7F){
    		return;
    	}

		uint32_t sline=0;
		sFONT * f = ((sFONT *)BSP_LCD_GetFont());

		const uint8_t *data = &f->table[(symbol-' ') * f->Height * ((f->Width + 7) / 8)];

		uint8_t height = f->Height;
		uint8_t width  = f->Width;

		uint8_t offset = 8 *((width + 7)/8) -  width ;
		for(int y = 0; y < height; y++)
		{
			const uint8_t* pchar = ((const uint8_t *)data + (width + 7)/8 * y);
			switch(((width + 7)/8))
			{
			case 1:
				sline =  pchar[0];
				break;

			case 2:
				sline =  (pchar[0]<< 8) | pchar[1];
				break;

			case 3:
			default:
				sline =  (pchar[0]<< 16) | (pchar[1]<< 8) | pchar[2];
				break;
			}

			for (int x = 0; x < width; x++)
			{
				if(sline & (1 << (width - x + offset - 1)))
				{
					draw_pixel_unsafe((pos.x + x), pos.y, pen_colour);
				}
				else
				{
					draw_pixel_unsafe((pos.x + x), pos.y, 0);
				}
			}

			pos.y++;
		}
    }

    void Image::symbol_cached(char32_t symbol, v16 pos)
    {
        this->symbol(symbol,pos,system_font_cache);
    }

    void Image::text_cached(const char* string, v16 pos)
    {
        for(int i=0; *string; string++, i+=system_font_cache.x) {
            if(i>size().x)
                return;
            symbol_cached(*string, {pos.x + i, pos.y});
        }
    }
        
    void Image::text(const char* string, v16 pos, v16 _fontsize)
    {
        for(int i=0; *string; string++, i+=pos.x) {
            if(i>size().x)
                return;
            symbol(*string, {pos.x + i, pos.y}, _fontsize);
        }
    }
        
    void Image::picture(const Picture& pict, v16 pos)
    {
        v16 sz = size();
        v16 picsz = pict.size();
        
        for(int x = 0; x < picsz.x; x++)
        {
            for(int y = 0; y < picsz.y; y++)
            {
                int xpix = x + pos.x;
                int ypix = y + pos.y;

                if(xpix >= 0 and ypix >= 0 and xpix < sz.x and ypix < sz.y){
                    draw_pixel_unsafe(xpix, ypix, pict.get_pixel({x, y}));
                }
            }
        }
    }

    Picture::Picture(v16 _size)
    {
        picsize = _size;
        buffer = std::make_unique<internal_pixel_t[]>(_size.x * _size.y);
        memset(buffer.get(), 0, _size.x * _size.y);
    }

    void Picture::draw_pixel_unsafe(int16_t X, int16_t Y, internal_pixel_t colour)
    {
        if(X < picsize.x and Y < picsize.y)
            if(X >= 0 and Y >= 0)
                buffer[picsize.x * Y + X] = colour;
    }
    void Picture::draw_hline_unsafe(v16 point, int16_t x2, internal_pixel_t colour)
    {
        uint16_t x0 = std::max((int16_t)0, std::min(x2, std::min(point.x, (int16_t)(picsize.x-1u))));
        uint16_t x1 = std::max((int16_t)0, std::min(std::max(x2, point.x), (int16_t)(picsize.x-1u)));

        for(int x = x0; x <= x1; x++)
        {
            buffer[picsize.x * point.y + x] = colour;
        }
    }
    void Picture::draw_vline_unsafe(v16 point, int16_t y2, internal_pixel_t colour)
    {
        uint16_t y0 = std::max((int16_t)0, std::min(y2, std::min(point.y, (int16_t)(picsize.y-1u))));
        uint16_t y1 = std::max((int16_t)0, std::min(std::max(y2, point.y), (int16_t)(picsize.y-1u)));

        for(int y = y0; y <= y1; y++)
        {
            buffer[picsize.x * y + point.x] = colour;
        }
    }

    void Picture::clear(int32_t XRGB)
    {
        auto clr = make_internal_pixel(XRGB);
        for(int i=0; i<picsize.y*picsize.x; i++)
            buffer[i] = clr;
    }

    void Picture::resize(v16 newsize)
    {
        auto oldsize = picsize;
        picsize = newsize;
        auto oldbuffer = std::move(buffer);
        buffer = unique_ptr<canvas::internal_pixel_t []>(new internal_pixel_t[newsize.x*newsize.y]);

        for(int i=0; i<picsize.y; i++){
            for(int j=0; i<picsize.x; i++){
                buffer[i*picsize.x + j] = (i<oldsize.y and j<oldsize.x) ? oldbuffer[i*oldsize.x + j] : 0;
            }
        }
    }

    void Picture::reset(v16 newsize)
    {
        picsize = newsize;
        buffer = unique_ptr<canvas::internal_pixel_t []>(new internal_pixel_t[newsize.x*newsize.y]);
        clear(0);
    }

}
