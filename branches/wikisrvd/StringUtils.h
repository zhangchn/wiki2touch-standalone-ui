/*
 *  StringUtils.h
 *  Wiki2Touch/wikisrvd
 *
 *  Copyright (c) 2008 by Tom Haukap.
 * 
 *  This file is part of Wiki2Touch.
 * 
 *  Wiki2Touch is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Wiki2Touch is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Wiki2Touch. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <wchar.h>
//#include <cwchar>

extern const wchar_t* dayName[];
extern const wchar_t* monNameAbbr[];
extern const wchar_t* monName[];

void display(const wchar_t* src);
wchar_t* trim_left_multiline(wchar_t* src);
wchar_t* trim_left(wchar_t* src);
wchar_t* trim_right(wchar_t* src);
wchar_t* trim(wchar_t* src);
wchar_t* url_encode(const wchar_t* src);

wchar_t _to_wlower(const wchar_t c);
void to_lower(wchar_t* src);
wchar_t _to_wupper(const wchar_t c);
void to_upper(wchar_t* src);

bool isintalpha(wchar_t c);
wchar_t* wstrdup(const wchar_t* src);
wchar_t* wstrndup(const wchar_t* src, size_t count);
int watoi(const wchar_t* src);

bool startsWith(const wchar_t* src, const wchar_t* prefix);
bool startsWithCase(const wchar_t* src, const wchar_t* prefix);

char* LoadFile(const char* filename);

wchar_t** split(const wchar_t* src, wchar_t splitChar);
void free_split_result(wchar_t** data);

#endif //STRINGUTILS_H


