/*
 *  StringUtils.cpp
 *  wikisrv
 *
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

#include <cstdlib>
#include "StringUtils.h"

const wchar_t* dayName[] = {L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday", 0x0};
const wchar_t* monNameAbbr[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec", 0x0};
const wchar_t* monName[] = {L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December", 0x0};

void display(const wchar_t* src)
{
	char result[wcslen(src)+1];
	char* pResult = result;
	
	while ( (*pResult++=*src++) ) ;
	
	pResult = result;
}

wchar_t* trim_left_multiline(wchar_t* src)
{
	if ( src==NULL )
		return NULL;
	
	if ( !*src )
		return src;
	
	wchar_t* dst = src;
	while ( *src && *src<=0x20 && *src!=L'\n' )
		src++;
	
	bool flag = false;
	//if ( src==dst )
	//	return src;
	
	wchar_t* result = dst;
	while ( *src )
	{
		if ( *src == L'\n' )
			flag = true;
		else if ( *src > 0x20 )
			flag = false;
		else if ( flag )
		{
			src++;
			continue;
		}
		*dst++ = *src++;
	}
	*dst = 0x0;
	
	return result;
}

wchar_t* trim_left(wchar_t* src)
{
	if ( src==NULL )
		return NULL;
	
	if ( !*src )
		return src;
	
	wchar_t* dst = src;
	while ( *src && *src<=0x20  )
		src++;
	
	if ( src==dst )
		return src;
	
	wchar_t* result = dst;
	while ( *src )
		*dst++ = *src++;
	*dst = 0x0;
	
	return result;
}

wchar_t* trim_right(wchar_t* src)
{
	if ( src==NULL )
		return NULL;
	
	if ( !*src )
		return src;
	
	wchar_t* dst = src;
	
	src = src + wcslen(src) - 1;
	
	while ( src>=dst && *src<=0x20  )
		src--;
	
	if ( src<dst || *src )
		src++;
	
	*src = 0x0;
	
	return dst;
}

wchar_t* trim(wchar_t* src)
{
	return trim_right(trim_left(src));
}

wchar_t* url_encode(const wchar_t* src)
{
	if ( !*src )
		return NULL;
	
	int count = 0;
	const wchar_t* psrc = src;
	while ( *psrc )
	{
		unsigned int c = *psrc;
		if ( c=='<' || c=='>' || c=='"' || c=='#' || c=='%' || c=='<' || c>0x7f )
			count += 3;
		else
			count++;
		
		psrc++;
	}

	psrc = src;
	wchar_t* dst = (wchar_t*) malloc((count+1) * sizeof(wchar_t));
	wchar_t* pdst = dst;
	while ( *psrc )
	{
		unsigned int c = *psrc;
		if ( c=='<' || c=='>' || c=='"' || c=='#' || c=='%' || c=='<' || c>0x7f )
		{
			*pdst++ = L'%';
			
			unsigned char d = (c & 0xf0) >> 4;
			if ( d<10 )
				*pdst++ = '0' + d;
			else
				*pdst++ = 'A' - 10 + d;
			
			d = (c & 0x0f);
			if ( d<10 )
				*pdst++ = '0' + d;
			else
				*pdst++ = 'A' - 10 + d;
		}
		else
			*pdst++ = c;
		
		psrc++;
	}
	*pdst = 0;
	return dst;
}

wchar_t _to_wlower(const wchar_t c) {if (c<0x80) return towlower(c); else if (c>=0xc0 && c<0xdf) return c+0x20; else return c;};

void to_lower(wchar_t* src)
{
	if ( !src )
		return;
	
	while ( *src )
		*src++ = _to_wlower(*src);
}

wchar_t _to_wupper(const wchar_t c) {if (c<0x80) return towupper(c); else if (c>=0xe0 && c<0xef) return c-0x20; else return c;};

void to_upper(wchar_t* src)
{
	if ( !src )
		return;
	
	while ( *src )
		*src++ = _to_wupper(*src);
}

bool isintalpha(wchar_t c)
{
	return isalpha(c) || (c>=0xc0 && c<=0xff);
}

wchar_t* wstrdup(const wchar_t* src)
{
	if ( !src )
		return NULL;
	
	size_t length = wcslen(src);
	wchar_t* dst = (wchar_t*) malloc((length+1) * sizeof(wchar_t));
	if ( length )
		wcscpy(dst, src);
	else
		*dst = 0x0;
	
	return dst;
}

wchar_t* wstrndup(const wchar_t* src, size_t count)
{
	if ( !src )
		return NULL;
	
	int length = 0;
	const wchar_t* help = src;
	while ( *help && length<count )
		length++;
	
	wchar_t* dst = (wchar_t*) malloc((length+1) * sizeof(wchar_t));
	if ( length )
		wcsncpy(dst, src, length);
	dst[length] = 0x0;
	
	return dst;
}

int watoi(const wchar_t* src)
{
	if ( !src || !*src )
		return 0;
	
	int result = 0;
	
	char c;
	while ( (c=*src++) )
	{
		if ( c<0x30 || c>0x39 )
			return result;
		else
			result = result*10 + (c-0x30);
	}
	
	return result;
}

bool startsWith(const wchar_t* src, const wchar_t* prefix)
{
	if ( !src || !prefix )
		return false;
	
	while ( *src && *prefix )
	{
		if ( *src++!=*prefix++ )
			break;
	}
	
	return !*prefix;
}

bool startsWithCase(const wchar_t* src, const wchar_t* prefix)
{
	if ( !src || !prefix )
		return false;
	
	while ( *src && *prefix )
	{
		if ( tolower(*src++)!=tolower(*prefix++) )
			break;
	}
	
	return !*prefix;
}

char* LoadFile(const char* filename)
{
	if ( !filename || !*filename )
		return NULL;

	FILE* f = fopen(filename, "rb");
	if ( !f ) 
		return NULL;
	
	int error = fseek(f, 0, SEEK_END);
	off_t size;
			  
	if ( !error )
		error = fgetpos(f, &size);
			  
	if ( !error )
		error = fseek(f, 0, SEEK_SET);
			  
	size_t read = 0;
	char* buffer = NULL;
			  
	if ( !error && size>0 ) 
	{
		buffer = (char*) malloc(size + 1);
		read = fread(buffer, 1, size, f);
	}
	fclose(f);
	
	if ( !read )
	{
		free(buffer);
		return NULL;
	}
	
	buffer[read] = 0;
	
	return buffer;
}

wchar_t** split(const wchar_t* src, wchar_t splitChar)
{
	if ( ! src )
		return NULL;
	
	int size = 2;
	int count = 0;
	wchar_t** list = (wchar_t**) malloc((size+1)*sizeof(wchar_t*));
	
	const wchar_t* start = src;
	wchar_t** dst = list;
	do 
	{
		if ( *src==splitChar || !*src )
		{
			if ( count==size )
			{
				size += 2;
				list = (wchar_t**) realloc(list, (size+1)*sizeof(wchar_t*));
				dst = list + count;
			}
			
			size_t length = (src-start);
			*dst = (wchar_t*) malloc((length+1)*sizeof(wchar_t));
			if ( length )
				wcsncpy(*dst, start, length);
			(*dst)[length] = 0;
			dst++;
			
			start = (src+1);
			count++;
		}
	}
	while ( *src++ );
	
	*dst = NULL;
	return list;
}

void free_split_result(wchar_t** data)
{
	if ( !data )
		return;
	
	wchar_t** list = data;
	while ( *data )
	{
		free(*data);
		data++;
	}
	free(list);
}

