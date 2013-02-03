/*
 *  Settings.h
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <sys/types.h>
#include <arpa/inet.h>
#include <string>

#include "ConfigFile.h"
#include "TitleIndex.h"
#include "ImageIndex.h"
#include "MathIndex.h"

using namespace std;

class Settings 
{	
public:
	Settings();
	~Settings();
	
	bool Init(int argc, char *argv[]);
	
	bool Verbose();
	bool Debug();
	bool ExpandTemplates();
	bool IsJSMathEnabled();
	
	in_addr_t Addr();
	int Port();
	
	string Path();
	string Home();
	string Cache();
	string DefaultLanguageCode();
	string InstalledLanguages();
	string WebContentPath();
	string Version();
	
	bool IsLanguageInstalled(string languageCode);
	bool AreImagesInstalled(string languageCode);
	
	ConfigFile* LanguageConfig(string languageCode);
	TitleIndex* GetTitleIndex(string languageCode);
	ImageIndex* GetImageIndex(string languageCode);
	MathIndex* GetMathIndex(string languageCode);
	
private:
	bool _verbose;
	bool _debug;
	bool _expandTemplates;
	bool _jsmath;
	
	in_addr_t _addr;
	int _port;
	string _path;
	string _home;
	string _cache;
	string _defaultLanguageCode;
	string _installedLanguages;
	string _basePath;
	string _webContentPath;
	
	void* _languageConfigs;
	void* _titleIndexes;
	void* _imageIndexes;
	void* _mathIndexes;

};

extern Settings settings;

#endif //SETTINGS_H
	
