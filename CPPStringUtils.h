/*
 *  CPPStringUtils.h
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

using namespace std;

#include <string>
#include <wctype.h>

class CPPStringUtils {
public:
	static std::string to_string(const std::wstring source);
	static std::wstring to_wstring(const std::string source);

	static std::string to_string(int source);
	static std::wstring to_wstring(int source);
	
	static std::string to_utf8(const std::string source);
	static std::string to_utf8(const std::wstring source);
	static std::string from_utf8(const std::string source);
	static std::wstring from_utf8w(const std::string source);
	
	static std::string to_lower(std::string src);
	static std::wstring to_lower(std::wstring src);
	static std::string to_lower_utf8(std::string utf8_src);
	
	static std::string trim(std::string src);
	static std::wstring trim(std::wstring src);
	
	static std::string url_encode(std::string src);
	static std::wstring url_encode(std::wstring src);
	
	static std::string url_decode(std::string src);
	
	static std::string exchange_diacritic_chars_utf8(string src);
	static std::string tc2sc_utf8(string src);
	static std::wstring js_format(std::wstring src);

};


class DBH 
	{	
	public:
		DBH(const wchar_t* arg);
		DBH(std::wstring arg);
		
		std::string result;
		std::wstring data;
		
		void Print();
	};

