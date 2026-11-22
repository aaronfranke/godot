/**************************************************************************/
/*  image_block_decompressor.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/image.h"

class ImageBlockDecompressor {
public:
	template <typename Decode>
	static void decompress(Image *p_image, Image::Format p_format, int p_block_size, int p_pixel_size, Decode p_decode) {
		const int width = p_image->get_width();
		const int height = p_image->get_height();
		const bool mipmaps = p_image->has_mipmaps();
		const int mipmap_count = p_image->get_mipmap_count();
		PoolVector<uint8_t> data;
		ERR_FAIL_COND(data.resize(Image::get_image_data_size(width, height, p_format, mipmaps)) != OK);
		{
			const PoolVector<uint8_t>::Read source = p_image->get_data().read();
			const PoolVector<uint8_t>::Write destination = data.write();
			const uint8_t *block = source.ptr();
			uint8_t *level = destination.ptr();
			int mip_width = width;
			int mip_height = height;
			// A partial block still decodes all 16 texels. RGBH needs the most space at 6 bytes per texel.
			alignas(uint32_t) uint8_t edge[4 * 4 * 6];
			const int block_pitch = 4 * p_pixel_size;
			for (int mip = 0; mip <= mipmap_count; mip++) {
				const int pitch = mip_width * p_pixel_size;
				for (int y = 0; y < mip_height; y += 4) {
					const int rows = MIN(4, mip_height - y);
					for (int x = 0; x < mip_width; x += 4) {
						const int columns = MIN(4, mip_width - x);
						uint8_t *pixel = level + y * pitch + x * p_pixel_size;
						if (rows == 4 && columns == 4) {
							p_decode(block, pixel, mip_width);
						} else {
							p_decode(block, edge, 4);
							for (int row = 0; row < rows; row++) {
								memcpy(pixel + row * pitch, edge + row * block_pitch, columns * p_pixel_size);
							}
						}
						block += p_block_size;
					}
				}
				level += pitch * mip_height;
				mip_width = MAX(1, mip_width >> 1);
				mip_height = MAX(1, mip_height >> 1);
			}
		}
		p_image->create(width, height, mipmaps, p_format, data);
	}
};
