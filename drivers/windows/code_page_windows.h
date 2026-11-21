/**************************************************************************/
/*  code_page_windows.h                                                   */
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

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

class WindowsCodePages {
public:
	static void populate(HashMap<String, int> &r_encodings) {
		static constexpr struct {
			const char *name;
			int code_page;
		} ENCODINGS[] = {
			{ "", 0 },
			{ "CP_ACP", 0 },
			{ "CP_OEMCP", 1 },
			{ "CP_MACCP", 2 },
			{ "CP_THREAD_ACP", 3 },
			{ "CP_SYMBOL", 42 },
			{ "IBM037", 37 },
			{ "IBM437", 437 },
			{ "IBM500", 500 },
			{ "ASMO-708", 708 },
			{ "ASMO-449", 709 },
			{ "DOS-710", 710 },
			{ "DOS-720", 720 },
			{ "IBM737", 737 },
			{ "IBM775", 775 },
			{ "IBM850", 850 },
			{ "IBM852", 852 },
			{ "IBM855", 855 },
			{ "IBM857", 857 },
			{ "IBM00858", 858 },
			{ "IBM860", 860 },
			{ "IBM861", 861 },
			{ "DOS-862", 862 },
			{ "IBM863", 863 },
			{ "IBM864", 864 },
			{ "IBM865", 865 },
			{ "CP866", 866 },
			{ "IBM869", 869 },
			{ "IBM870", 870 },
			{ "WINDOWS-874", 874 },
			{ "CP875", 875 },
			{ "SHIFT_JIS", 932 },
			{ "GB2312", 936 },
			{ "KS_C_5601-1987", 949 },
			{ "BIG5", 950 },
			{ "IBM1026", 1026 },
			{ "IBM01047", 1047 },
			{ "IBM01140", 1140 },
			{ "IBM01141", 1141 },
			{ "IBM01142", 1142 },
			{ "IBM01143", 1143 },
			{ "IBM01144", 1144 },
			{ "IBM01145", 1145 },
			{ "IBM01146", 1146 },
			{ "IBM01147", 1147 },
			{ "IBM01148", 1148 },
			{ "IBM01149", 1149 },
			{ "UTF-16", 1200 },
			{ "UNICODEFFFE", 1201 },
			{ "WINDOWS-1250", 1250 },
			{ "WINDOWS-1251", 1251 },
			{ "WINDOWS-1252", 1252 },
			{ "WINDOWS-1253", 1253 },
			{ "WINDOWS-1254", 1254 },
			{ "WINDOWS-1255", 1255 },
			{ "WINDOWS-1256", 1256 },
			{ "WINDOWS-1257", 1257 },
			{ "WINDOWS-1258", 1258 },
			{ "JOHAB", 1361 },
			{ "MACINTOSH", 10000 },
			{ "X-MAC-JAPANESE", 10001 },
			{ "X-MAC-CHINESETRAD", 10002 },
			{ "X-MAC-KOREAN", 10003 },
			{ "X-MAC-ARABIC", 10004 },
			{ "X-MAC-HEBREW", 10005 },
			{ "X-MAC-GREEK", 10006 },
			{ "X-MAC-CYRILLIC", 10007 },
			{ "X-MAC-CHINESESIMP", 10008 },
			{ "X-MAC-ROMANIAN", 10010 },
			{ "X-MAC-UKRAINIAN", 10017 },
			{ "X-MAC-THAI", 10021 },
			{ "X-MAC-CE", 10029 },
			{ "X-MAC-ICELANDIC", 10079 },
			{ "X-MAC-TURKISH", 10081 },
			{ "X-MAC-CROATIAN", 10082 },
			{ "UTF-32", 12000 },
			{ "UTF-32BE", 12001 },
			{ "X-CHINESE_CNS", 20000 },
			{ "X-CP20001", 20001 },
			{ "X_CHINESE-ETEN", 20002 },
			{ "X-CP20003", 20003 },
			{ "X-CP20004", 20004 },
			{ "X-CP20005", 20005 },
			{ "X-IA5", 20105 },
			{ "X-IA5-GERMAN", 20106 },
			{ "X-IA5-SWEDISH", 20107 },
			{ "X-IA5-NORWEGIAN", 20108 },
			{ "US-ASCII", 20127 },
			{ "X-CP20261", 20261 },
			{ "X-CP20269", 20269 },
			{ "IBM273", 20273 },
			{ "IBM277", 20277 },
			{ "IBM278", 20278 },
			{ "IBM280", 20280 },
			{ "IBM284", 20284 },
			{ "IBM285", 20285 },
			{ "IBM290", 20290 },
			{ "IBM297", 20297 },
			{ "IBM420", 20420 },
			{ "IBM423", 20423 },
			{ "IBM424", 20424 },
			{ "X-EBCDIC-KOREANEXTENDED", 20833 },
			{ "IBM-THAI", 20838 },
			{ "KOI8-R", 20866 },
			{ "IBM871", 20871 },
			{ "IBM880", 20880 },
			{ "IBM905", 20905 },
			{ "IBM00924", 20924 },
			{ "EUC-JP", 20932 },
			{ "X-CP20936", 20936 },
			{ "X-CP20949", 20949 },
			{ "CP1025", 21025 },
			{ "KOI8-U", 21866 },
			{ "ISO-8859-1", 28591 },
			{ "ISO-8859-2", 28592 },
			{ "ISO-8859-3", 28593 },
			{ "ISO-8859-4", 28594 },
			{ "ISO-8859-5", 28595 },
			{ "ISO-8859-6", 28596 },
			{ "ISO-8859-7", 28597 },
			{ "ISO-8859-8", 28598 },
			{ "ISO-8859-9", 28599 },
			{ "ISO-8859-13", 28603 },
			{ "ISO-8859-15", 28605 },
			{ "X-EUROPA", 29001 },
			{ "ISO-8859-8-I", 38598 },
			{ "ISO-2022-JP", 50220 },
			{ "CSISO2022JP", 50221 },
			{ "ISO-2022-JP", 50222 },
			{ "ISO-2022-KR", 50225 },
			{ "X-CP50227", 50227 },
			{ "EBCDIC-JP", 50930 },
			{ "EBCDIC-US-JP", 50931 },
			{ "EBCDIC-KR", 50933 },
			{ "EBCDIC-CN-eXT", 50935 },
			{ "EBCDIC-CN", 50936 },
			{ "EBCDIC-US-CN", 50937 },
			{ "EBCDIC-JP-EXT", 50939 },
			{ "EUC-JP", 51932 },
			{ "EUC-CN", 51936 },
			{ "EUC-KR", 51949 },
			{ "HZ-GB-2312", 52936 },
			{ "GB18030", 54936 },
			{ "X-ISCII-DE", 57002 },
			{ "X-ISCII-BE", 57003 },
			{ "X-ISCII-TA", 57004 },
			{ "X-ISCII-TE", 57005 },
			{ "X-ISCII-AS", 57006 },
			{ "X-ISCII-OR", 57007 },
			{ "X-ISCII-KA", 57008 },
			{ "X-ISCII-MA", 57009 },
			{ "X-ISCII-GU", 57010 },
			{ "X-ISCII-PA", 57011 },
			{ "UTF-7", 65000 },
			{ "UTF-8", 65001 },
		};

		for (const auto &encoding : ENCODINGS) {
			r_encodings[encoding.name] = encoding.code_page;
		}
	}
};
