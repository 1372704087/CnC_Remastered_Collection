/**
 * @file
 *
 * @author CCHyper
 * @author OmniBlade
 *
 * @brief Low level bitmap font string blitter.
 *
 * @copyright Chronoshift is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            2 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "FUNCTION.H"
#include <algorithm>
#include <cstring>

using std::memcpy;
using std::memset;

#define le16toh(x) x

#ifdef GAME_DLL
extern char*& g_fontWidthBlockPtr;
#else
uint8_t g_ColorXlat[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
uint8_t byte_608988[] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    15, 0, 0, 0 };

char* g_FontPtr = nullptr;
char* g_fontWidthBlockPtr = nullptr;
uint8_t g_FontHeight = 8;
uint8_t g_FontWidth = 8;
int g_FontXSpacing;
int g_FontYSpacing;
#endif

struct FontHeader
{
	uint16_t file_size; // Size of the file
	uint8_t file_version; // file version? 0==EOB? 1==RA? 2==TS? what about RA2?
	uint8_t unknown; // 5==TD/RA 0==TS? unused in RA
	uint16_t start; // offset to start of font description Pad, always 0x0010
	uint16_t data_offset_list; // offset to bitmap data offset list, always 0x0014 as follows header
	uint16_t width_list; // points to list of widths
	uint16_t data; // points to graphics data, unused by RA code
	uint16_t line_list; // points to list of heights (or lines)
	uint16_t unknown_const; // Unknown entry (always 0x1012 or 0x1011) unused by RA
	uint8_t pad; // padding?
	uint8_t number_of_chars; // Number of chars in font - 1
	uint8_t max_height; // Max char height
	uint8_t max_width; // Max char width
};

extern "C" void __cdecl Set_Font_Palette_Range(const void* colors, int first, int last)
{
    byte* pal = (byte*)colors;
    first &= 15;
    last &= 15;

    for (int i = first; i < last + 1; ++i) {
        g_ColorXlat[i] = *pal++;
    }
}

void* __cdecl Set_Font(void const* fontptr)
{
    FontHeader* header;

    header = (FontHeader*)(fontptr);

    if (fontptr) {
        g_FontPtr = (char*)(fontptr);
        g_fontWidthBlockPtr = (char*)(fontptr) + le16toh(header->width_list);
		FontHeight = g_FontHeight = le16toh(header->max_height);
        FontWidth = g_FontWidth = le16toh(header->max_width);
    }

    return header;
}

int __cdecl Char_Pixel_Width(char letter)
{
    // Cast to unsigned as all blocks index from zero to NumberOfChars + 1
    // Original watcom compiler had char as an unsigned type by default.
    return g_fontWidthBlockPtr[(uint8_t)letter] + g_FontXSpacing;
}

unsigned int __cdecl String_Pixel_Width(const char* string)
{
    uint16_t width;
    uint16_t linewidth = 0;

    if (string != nullptr) {
        width = 0;

        while (*string != '\0') {
            if (*string == '\r') {
                ++string;
                linewidth = max(linewidth, width);
                width = 0;
            }
            else {
                width += Char_Pixel_Width(*string++);
            }
        }

        return max(linewidth, width);
    }

    return 0;
}

int __cdecl Buffer_Print2(GraphicViewPortClass& vp, const char* string, int x, int y, uint8_t fground, uint8_t bground)
{
	FontHeader* fntheader = reinterpret_cast<FontHeader*>(g_FontPtr);
	int pitch = vp.Get_Full_Pitch();
	uint8_t* offset = y * pitch + (uint8_t*)(vp.Get_Offset());
	uint8_t* dst = x + offset;
	int char_width = 0;
	int base_x = x;

	if (g_FontPtr != nullptr) {
		uint16_t* datalist =
			reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(g_FontPtr) + le16toh(fntheader->data_offset_list));
		uint8_t* widthlist = reinterpret_cast<uint8_t*>(g_FontPtr) + le16toh(fntheader->width_list);
		uint16_t* linelist =
			reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(g_FontPtr) + le16toh(fntheader->line_list));

		int fntheight = fntheader->max_height;
		int ydisplace = g_FontYSpacing + fntheight;

		// Check if we are drawing in bounds, we don't draw clipped text
		if (y + fntheight <= vp.Get_Height()) {
			int fntbottom = y + fntheight;
			// Set colors to draw with
			g_ColorXlat[1] = fground;
			g_ColorXlat[0] = bground;

			while (true) {
				// Handle a new line
				uint8_t char_num;
				uint8_t* char_dst;
				while (true) {
					char_num = uint8_t(*string);

					if (char_num == '\0') {
						return 0;
					}

					char_dst = dst;
					++string;

					if (char_num != '\n' && char_num != '\r') {
						break;
					}

					// We don't handle clipping text, it either draws or it doesn't
					if (ydisplace + fntbottom > vp.Get_Height()) {
						return 0;
					}

					// If its a new line, we are just going to set the start to an increased y displacement, hence x_pos
					// becomes 0
					x = char_num == '\n' ? 0 : base_x;
					dst = ydisplace * pitch + offset + x;
					offset += ydisplace * pitch;
					fntbottom += ydisplace;
				}

				// Move to the start of the next char
				char_width = widthlist[char_num];
				dst += g_FontXSpacing + char_width;

				// Handle text wrapping for long strings
				if (g_FontXSpacing + char_width + x > vp.Get_Width()) {
					--string;
					char_num = '\0';

					// We don't handle clipping text, it either draws or it doesn't
					if (ydisplace + fntbottom > vp.Get_Height()) {
						return 0;
					}

					// If its a new line, we are just going to set the start to an increased y displacement, hence x_pos
					// becomes 0
					x = char_num == '\n' ? 0 : base_x;
					dst = ydisplace * pitch + offset + x;
					offset += ydisplace * pitch;
					fntbottom += ydisplace;

					continue;
				}

				// Prepare variables for drawing
				x += g_FontXSpacing + char_width;
				int next_line = pitch - char_width;
				uint8_t* char_data = reinterpret_cast<uint8_t*>(g_FontPtr) + datalist[char_num];
				int char_lle = le16toh(linelist[char_num]);
				int char_ypos = char_lle & 0xFF;
				int char_lines = char_lle >> 8;
				int char_height = fntheight - (char_ypos + char_lines);
				int blit_width = widthlist[char_num];

				// Fill unused lines if we have a color other than 0
				if (char_ypos) {
					uint8_t color = g_ColorXlat[0];
					if (color) {
						for (int i = char_ypos; i; --i) {
							memset(char_dst, color, blit_width);
							char_dst += pitch;
						}
					}
					else {
						char_dst += pitch * char_ypos;
					}
				}

				// Draw the character
				if (char_lines) {
					for (int i = 0; i < char_lines; ++i) {
						int width_todraw = blit_width;

						while (width_todraw) {
							uint8_t color_packed;
							color_packed = *char_data++;
							uint8_t color = g_ColorXlat[color_packed & 0x0F];

							if (color) {
								*char_dst = color;
							}

							++char_dst;
							--width_todraw;

							if (width_todraw == 0) {
								break;
							}

							color = g_ColorXlat[color_packed >> 4];

							if (color) {
								*char_dst = color;
							}

							++char_dst;
							--width_todraw;
						}

						char_dst += next_line;
					}

					// Fill any remaining unused lines.
					if (char_height) {
						uint8_t color = g_ColorXlat[0];

						if (color) {
							for (int i = char_height; i; --i) {
								memset(char_dst, color, blit_width);
								char_dst += pitch;
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

extern "C" LONG __cdecl Buffer_Print(void* thisptr, const char* str, int x, int y, int fcolor, int bcolor)
{
	//Buffer_Print2(*(GraphicViewPortClass*)thisptr, str, x, y, fcolor, bcolor);
	GL_DrawText(fcolor, x, y, (char *)str);
    return 0;
}
