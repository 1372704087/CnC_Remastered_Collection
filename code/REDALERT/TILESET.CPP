/**
 * @file
 *
 * @author CCHyper
 * @author OmniBlade
 *
 * @brief Low level functions for loading and rendering C&C terrain tilesets.
 *
 * @copyright Chronoshift is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            2 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
 /*
 ============================

 This file has been heavily modified by IceColdDuke(Justin Marshall) to render with OpenGL.

 ============================
 */

#include "FUNCTION.H"
#include "gbuffer.h"
#include "Image.h"
#include <cstring>

using std::memcpy;

class GraphicViewPortClass;

#define TD_TILESET_CHECK 0x20

Image_t* tileset_icon_cache[4096];

/**
* @brief union is to handle the parts of the header which vary between TD and RA format tiles.
*/
struct IconControlType
{
	uint8_t* Get_Icon_Data()
	{
		if (td.icons == TD_TILESET_CHECK) {
			return reinterpret_cast<uint8_t*>(this) + td.icons;
		}
		else {
			return reinterpret_cast<uint8_t*>(this) + ra.icons;
		}
	}

	uint8_t* Get_Icon_Map()
	{
		if (td.icons == TD_TILESET_CHECK) {
			return reinterpret_cast<uint8_t*>(this) + td.map;
		}
		else {
			return reinterpret_cast<uint8_t*>(this) + ra.map;
		}
	}

	int16_t width; // always 24 (ICON_WIDTH)
	int16_t height; // always 24 (ICON_HEIGHT)
	int16_t count; // count of cells in set, not same as images
	int16_t allocated; // is treated like a bool, always 0 in the file?

	union
	{
		struct
		{
			int32_t size; // filesize
			int32_t icons; // always 0x00000020
			int32_t palettes; // seems to always be 0x00000000
			int32_t remaps; // unknown, bitfield?
			int32_t trans_flag; // array of images length, unknown
			int32_t map; // image index for each cell
		} td;

		struct
		{
			int16_t map_width; // tile width in cells
			int16_t map_height; // tile height in cells
			int32_t size; // filesize
			int32_t icons; // always 0x00000028
			int32_t palettes; // seems to always be 0x00000000
			int32_t remaps; // unknown, bitfield?
			int32_t trans_flag; // array of images length, unknown
			int32_t color_map; // terrain type index, ra only
			int32_t map; // image index for each cell
		} ra;
	};
};

static int IconEntry;
static void* IconData;
static IconControlType* LastIconset;
static uint8_t* StampPtr;
static uint8_t* TransFlagPtr;
static uint8_t* MapPtr;
static int IconWidth;
static int IconHeight;
static int IconSize;
static int IconCount;

void __cdecl Init_Stamps(IconControlType* iconset)
{
    memset(&tileset_icon_cache, 0, sizeof(tileset_icon_cache));
    if (iconset && LastIconset != iconset) {
        IconCount = (iconset->count);
        IconWidth = (iconset->width);
        IconHeight = (iconset->height);
        LastIconset = iconset;
        IconSize = IconWidth * IconHeight;

        // TD and RA tileset headers are slightly different, so check a constant that only exists in one type.
        if ((iconset->td.icons) == TD_TILESET_CHECK) {
            MapPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->td.map);
            StampPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->td.icons);
            TransFlagPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->td.trans_flag);
        }
        else {
            MapPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->ra.map);
            StampPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->ra.icons);
            TransFlagPtr = reinterpret_cast<uint8_t*>(iconset) + (iconset->ra.trans_flag);
        }
    }
}

void __cdecl Buffer_Draw_Stamp2(GraphicViewPortClass& viewport, IconControlType* tileset, int icon, int x, int y, const void* remapper)
{
    if (!tileset) {
        return;
    }

    if (LastIconset != tileset) {
        Init_Stamps(tileset);
    }

    int32_t icon_index = MapPtr != nullptr ? MapPtr[icon] : icon;

    if (icon_index < IconCount) {

        int32_t fullpitch = viewport.Get_Full_Pitch(); //(viewport.Get_Pitch() + viewport.Get_XAdd() + viewport.Get_Width());
        uint8_t* dst = (x * 4) + (y * 4) * fullpitch + (uint8_t*)(viewport.Get_Offset());
        int32_t blitpitch = fullpitch - IconWidth;
        uint8_t* src = &StampPtr[IconSize * icon_index];

        if (remapper) {
            const uint8_t* remap = static_cast<const uint8_t*>(remapper);
            for (int i = 0; i < IconHeight; ++i) {
                for (int j = 0; j < IconWidth; ++j) {
                    uint8_t cur_byte = remap[*src++];

                    if (cur_byte) {
                        dst[0] = backbuffer_data_raw[(cur_byte * 3) + 0];
                        dst[1] = backbuffer_data_raw[(cur_byte * 3) + 1];
                        dst[2] = backbuffer_data_raw[(cur_byte * 3) + 2];
                        dst[3] = 255;
                    }

                    dst+=4;
                }

                dst += blitpitch;
            }

        }
        else if (TransFlagPtr[icon_index]) {
            for (int i = 0; i < IconHeight; ++i) {
                for (int j = 0; j < IconWidth; ++j) {
                    uint8_t cur_byte = *src++;

					if (cur_byte) {
						dst[0] = backbuffer_data_raw[(cur_byte * 3) + 0];
						dst[1] = backbuffer_data_raw[(cur_byte * 3) + 1];
						dst[2] = backbuffer_data_raw[(cur_byte * 3) + 2];
						dst[3] = 255;
					}

					dst += 4;
                }

                dst += blitpitch;
            }
        }
        else {
            for (int32_t i = 0; i < IconHeight; ++i) {
                FastScanlinePaletteBlit(dst, src, IconWidth);
                dst += fullpitch;
                src += IconWidth * 4;
            }
        }
    }
}

void __cdecl Buffer_Draw_Stamp_Clip2(GraphicViewPortClass& viewport, const void *icondata, int icon, int x, int y, const void* remapper, int left, int top, int right, int bottom)
{
    const TemplateTypeClass* ttype = (TemplateTypeClass const*)icondata;
    IconControlType* tileset = (IconControlType * )ttype->Get_Image_Data();

    // This is a awful hack, but need type info to generate unique id's for tileset generation.
    if (icondata == DisplayClass::TransIconset) {
        tileset = (IconControlType*)DisplayClass::TransIconset;
        ttype = NULL;
    }

    if (!tileset) {
        return;
    }

    if (LastIconset != tileset) {
        Init_Stamps(tileset);
    }

    int icon_index = MapPtr != nullptr ? MapPtr[icon] : icon;
    
    if (icon_index < IconCount) {
        int blit_height = IconHeight;
        int blit_width = IconWidth;
        uint8_t* src = &StampPtr[IconSize * icon_index];
        int width = left + right;
        int xstart = left + x;
        int height = top + bottom;
        int ystart = top + y;

		//
		if (!tileset_icon_cache[icon_index]) {
			char tmp[512];
            if (ttype != NULL) {
                sprintf(tmp, "icon_%d_%d", ttype->Type, icon_index);
            }
            else {
                sprintf(tmp, "icon_%d_%d", icondata, icon_index);
            }
            tileset_icon_cache[icon_index] = Image_CreateImageFrom8Bit(tmp, IconWidth, IconHeight, (unsigned char *)src);
		}
    
        if (xstart < width && ystart < height && IconHeight + ystart > top && IconWidth + xstart > left) {
            if (xstart < left) {
                src += left - xstart;
                blit_width -= left - xstart;
                xstart = left;
            }
    
            int src_pitch = IconWidth - blit_width;
    
            if (blit_width + xstart > width) {
                src_pitch += blit_width - (width - xstart);
                blit_width = width - xstart;
            }
    
            if (top > ystart) {
                blit_height = IconHeight - (top - ystart);
                src += IconWidth * (top - ystart);
                ystart = top;
            }
    
            if (blit_height + ystart > height) {
                blit_height = height - ystart;
            }
    
            int full_pitch = viewport.Get_Full_Pitch(); //(viewport.Get_Pitch() + viewport.Get_XAdd() + viewport.Get_Width());
            uint8_t* dst = (xstart * 4) + (ystart * 4) * full_pitch + (uint8_t*)(viewport.Get_Offset());
            int dst_pitch = full_pitch - blit_width;
    
            //if (remapper) {
            //    const uint8_t* remap = static_cast<const uint8_t*>(remapper);
            //    for (int i = 0; i < blit_height; ++i) {
            //        for (int j = 0; j < blit_width; ++j) {
            //            uint8_t cur_byte = remap[*src++];
			//			if (cur_byte) {
			//				dst[0] = backbuffer_data_raw[(cur_byte * 3) + 0];
			//				dst[1] = backbuffer_data_raw[(cur_byte * 3) + 1];
			//				dst[2] = backbuffer_data_raw[(cur_byte * 3) + 2];
			//				dst[3] = 255;
			//			}
            //
			//			dst += 4;
            //        }
            //        dst += dst_pitch * 4;
            //    }
            //
            //}
            //else if (TransFlagPtr[icon_index]) {
            //    for (int i = 0; i < blit_height; ++i) {
            //        for (int j = 0; j < blit_width; ++j) {
            //            uint8_t cur_byte = *src++;
			//			if (cur_byte) {
			//				dst[0] = backbuffer_data_raw[(cur_byte * 3) + 0];
			//				dst[1] = backbuffer_data_raw[(cur_byte * 3) + 1];
			//				dst[2] = backbuffer_data_raw[(cur_byte * 3) + 2];
			//				dst[3] = 255;
			//			}
            //
			//			dst += 4;
            //        }
            //        src += src_pitch;
            //        dst += dst_pitch * 4;
            //    }
            //
            //}
            //else {
            {
                //for (int i = 0; i < blit_height; ++i) {
                //    FastScanlinePaletteBlit(dst, src, blit_width);
                //    dst += full_pitch * 4;
                //    src += IconWidth;
                //}

               GL_SetClipRect(xstart, ystart, blit_width, blit_height);
                GL_RenderImage(tileset_icon_cache[icon_index], xstart, ystart, blit_width, blit_height);
               GL_ResetClipRect();
            }
        }
    }
}

extern "C" void __cdecl Buffer_Draw_Stamp_Clip(void const* this_object, void const* icondata, int icon, int x_pixel, int y_pixel, void const* remap, int min_x, int min_y, int max_x, int max_y) {
    if (icondata == NULL)
        return;

	Buffer_Draw_Stamp_Clip2(*((GraphicViewPortClass*)this_object), icondata, icon, x_pixel, y_pixel, remap, min_x, min_y, max_x, max_y);
}

extern "C" void __cdecl Buffer_Draw_Stamp(void const* thisptr, void const* icondata, int icon, int x_pixel, int y_pixel, void const* remap) {
    Buffer_Draw_Stamp2(*((GraphicViewPortClass*)thisptr), (IconControlType*)icondata, icon, x_pixel, y_pixel, remap);
}

uint8_t* Get_Icon_Set_Map(void* temp)
{
    if (temp != nullptr) {
        if ((static_cast<IconControlType*>(temp)->td.icons) == TD_TILESET_CHECK) {
            return static_cast<uint8_t*>(temp) + (static_cast<IconControlType*>(temp)->td.icons);
        }
        else {
            return static_cast<uint8_t*>(temp) + (static_cast<IconControlType*>(temp)->ra.icons);
        }
    }

    return nullptr;
}
