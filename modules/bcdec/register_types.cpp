/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "modules/image_block_decompressor.h"

#define BCDEC_IMPLEMENTATION
#include "thirdparty/misc/bcdec.h"

static void image_decompress_bptc_bcdec(Image *p_image) {
	switch (p_image->get_format()) {
		case Image::FORMAT_BPTC_RGBA:
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBA8, BCDEC_BC7_BLOCK_SIZE, 4,
					[](const void *p_block, void *p_pixels, int p_width) {
						bcdec_bc7(p_block, p_pixels, p_width * 4);
					});
			break;
		case Image::FORMAT_BPTC_RGBF:
		case Image::FORMAT_BPTC_RGBFU: {
			const bool signed_bc6 = p_image->get_format() == Image::FORMAT_BPTC_RGBF;
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBH, BCDEC_BC6H_BLOCK_SIZE, 6,
					[signed_bc6](const void *p_block, void *p_pixels, int p_width) {
						bcdec_bc6h_half(p_block, p_pixels, p_width * 3, signed_bc6);
					});
		} break;
		default:
			break;
	}
}

void register_bcdec_types() {
	Image::_image_decompress_bptc = image_decompress_bptc_bcdec;
}

void unregister_bcdec_types() {
	Image::_image_decompress_bptc = nullptr;
}
