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

#include <DecodeRGB.hpp>

static void image_decompress_etc_etcpak(Image *p_image) {
	switch (p_image->get_format()) {
		case Image::FORMAT_ETC:
		case Image::FORMAT_ETC2_RGB8:
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBA8, 8, 4, DecodeRGBBlock);
			break;
		case Image::FORMAT_ETC2_RGBA8:
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBA8, 16, 4, DecodeRGBABlock);
			break;
		case Image::FORMAT_ETC2_R11:
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBA8, 8, 4, DecodeRBlock);
			break;
		case Image::FORMAT_ETC2_RG11:
			ImageBlockDecompressor::decompress(p_image, Image::FORMAT_RGBA8, 16, 4, DecodeRGBlock);
			break;
		default:
			// Signed EAC and RGB8A1 are outside the etcpak decoder's supported formats.
			break;
	}
}

void register_etcpak_types() {
	Image::_image_decompress_etc1 = image_decompress_etc_etcpak;
	Image::_image_decompress_etc2 = image_decompress_etc_etcpak;
}

void unregister_etcpak_types() {
	Image::_image_decompress_etc1 = nullptr;
	Image::_image_decompress_etc2 = nullptr;
}
