/*
 *  ConfigFile.cpp
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

#include "ConfigFile.h"
#include <cstdio>
#include <cstring>

typedef struct tagSetting
{
	string name;
	string value;
	
	tagSetting* next;
} SETTING;

ConfigFile::ConfigFile(string filename)
{
	_settings = NULL;
	
	_failed = true;
	
	const char* name = filename.c_str(); 
	FILE* f = fopen(name, "r");
	if ( !f )
		return;
	
	while ( !feof(f) ) 
	{
		char line[256];
		int lineLength = 255;
		
		char* pLine = line;
		char* pEqual = NULL;
		
		while ( !feof(f) )
		{
			char c = getc(f);
			if ( c=='\r' )
				continue;
			else if ( c=='\n' )
				break;
			else 
			{
				if ( c=='=' )
					pEqual = pLine;
				
				if ( lineLength-- )
					*pLine++ = c;
			}
		}
		*pLine = 0;
				
		if ( lineLength && strlen(line)>0 && line[0]!='#' && pEqual )
		{
			*pEqual++ = 0;
			
			SETTING* setting = new SETTING();
			setting->name = line;
			setting->value = pEqual;
			setting->next = (SETTING*) _settings;
			
			_settings = setting;
		}
	}
	fclose(f);
	
	_failed = false;
}

ConfigFile::~ConfigFile()
{
	while ( _settings )
	{
		SETTING* setting = (SETTING*) _settings;
		_settings = setting->next;
		
		delete (setting);
	}
}

bool ConfigFile::Failed()
{
	return _failed;
}

string ConfigFile::GetSetting(string name, string defaultValue)
{
	SETTING* setting = (SETTING*) _settings;
	while ( setting )
	{
		if ( setting->name==name )
			return setting->value;
		
		setting = setting->next;
	}
	
	return defaultValue;
}

string ConfigFile::GetSetting(string name)
{
	return GetSetting(name, "");
}
