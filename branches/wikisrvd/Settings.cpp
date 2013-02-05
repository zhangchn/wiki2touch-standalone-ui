/*
 *  Settings.cpp
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

#include "Settings.h"
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "CPPStringUtils.h"

const char* version = "0.65";

Settings settings;

typedef struct tagLANGUAGECONFIG
{
	string	languageCode;
	ConfigFile* configFile;
	tagLANGUAGECONFIG* next;
} LANGUAGECONFIG;

typedef struct tagTITLEINDEX
{
	string	languageCode;
	TitleIndex* titleIndex;
	tagTITLEINDEX* next;
} TITLEINDEX;

typedef struct tagIMAGEINDEX
{
	string	languageCode;
	ImageIndex* imageIndex;
	tagIMAGEINDEX* next;
}IMAGEINDEX;
typedef struct tagMATHINDEX
{
	string	languageCode;
	MathIndex* mathIndex;
	tagMATHINDEX* next;
} MATHINDEX;


Settings::Settings()
{
	_debug = false;
	_verbose = false;
	_expandTemplates = false;
	_jsmath = false;
	
	_addr = inet_addr("127.0.0.1");
	_addr = INADDR_ANY;
	_port = 8080;
	_path = "~/Media/Wikipedia";
	_webContentPath = "";
	
	// this is the default language
	_defaultLanguageCode = "en";
	
	_languageConfigs = NULL;
	_titleIndexes = NULL;
}

Settings::~Settings()
{	
	while ( _languageConfigs )
	{
		LANGUAGECONFIG* languageConfig = (LANGUAGECONFIG*) _languageConfigs;
		_languageConfigs = languageConfig->next;
		
		if ( languageConfig->configFile )
			delete(languageConfig->configFile);
		
		delete(languageConfig);
	}
	
	while ( _titleIndexes )
	{
		TITLEINDEX* titleIndex = (TITLEINDEX*) _titleIndexes;
		_titleIndexes = titleIndex->next;
		
		if ( titleIndex->titleIndex )
			delete(titleIndex->titleIndex);
		
		delete(titleIndex);
	}
}

bool Settings::Init(int argc, char *argv[])
{
	int i = 1;
	while ( i<argc )
	{
		if ( !strcmp(argv[i], "-b") ) 
		{
			if ( i<argc-1 )
			{
				i++;
				_path = string(argv[i]);
			}
		}
		else if ( !strcmp(argv[i], "-p") ) 
		{
			if ( i<argc-1 )
			{
				i++;
				_port = atoi(argv[i]);
				if ( _port<=0x0 || _port>0xffff ) 
				{
					printf("illegal port: %i\r\n", _port);
					return false;
				}
			}
		}
		else if ( !strcmp(argv[i], "-a") ) 
		{
			if ( i<argc-1 )
			{			
				i++;
				_addr = inet_addr(argv[i]);
			}
		}
		else if ( !strcmp(argv[i], "-l") ) 
		{
			if ( i<argc-1 )
			{			
				i++;
				_defaultLanguageCode = argv[i];
			}
		}
		else if ( !strcmp(argv[i], "-t") || !strcmp(argv[i], "-t+") ) 
			_expandTemplates = true;
		else if ( !strcmp(argv[i], "-t-") ) 
			_expandTemplates = false;
		else if ( !strcmp(argv[i], "-v") || !strcmp(argv[i], "-v+") ) 
			_verbose = true;
		else if ( !strcmp(argv[i], "-v-") )
			_verbose = false; 
		else if ( !strcmp(argv[i], "-d") ) 
			_debug = true;
		else if ( !strcmp(argv[i], "-jsmath") )
			_jsmath = true;
		
		i++;
	}

	const char* pfx = getenv("HOME");
	if ( pfx ) 
		_home = pfx;
	else
		// unable to resolve the home variable, use the path for the iPod Touch/iPhone
		_home = "/var/mobile/";
	if ( !_home.length() || _home[_home.length()-1]!='/' )
		_home += "/";


	if ( _path.empty() )
		_path = string("~/Media/Wikipedia");

	if ( _path.find("~")==0 ) 
	{
		if ( _path.length()>1 && _path[1]=='/' )
            _path = _home + _path.substr(2);
//			_path = "/var/mobile/" + _path.substr(2);
		else
			_path = _home + _path.substr(1);
	}
	
	// add the trailing slash if necessary
	if ( _path[_path.length()-1]!='/' )
		_path += '/';	

	// create the cache folder in any case
	_cache = _home + "Library/Caches/Wiki2Touch/";
	mkdir(_cache.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
	
	// store the application path
	_basePath = argv[0];
	size_t pos = _basePath.find_last_of('/');
	if ( pos!=string::npos )
		_basePath = _basePath.substr(0, pos+1);
	
	// set the path for the web content folder
	_webContentPath = _basePath + "webcontent";
	
	// find out which languages are installed
	_installedLanguages = string();
	string firstFoundLanguageCode = string();
	
	// check for our "default" database (contains help and so on)
	string path;
	struct stat statbuf;
	
	DIR* dir = opendir(_path.c_str());
	if ( dir )
	{
		struct dirent* dirbuf;
		while ( (dirbuf=readdir(dir)) )
		{
			if ( ((dirbuf->d_type==DT_DIR) || (dirbuf->d_type==DT_LNK)) && dirbuf->d_name[0]!='.' )
			{
				path = _path + dirbuf->d_name + "/articles.bin";
				bool found = (stat(path.c_str(), &statbuf) >=0 && S_ISREG(statbuf.st_mode));
				if ( !found )
				{
					// try an alternate name
					path = _path + dirbuf->d_name + "/articles_" + dirbuf->d_name + ".bin";
					found = (stat(path.c_str(), &statbuf) >=0 && S_ISREG(statbuf.st_mode));
				}
				if ( !found )
				{
					path = _path + dirbuf->d_name + "/manifest";
					found = (stat(path.c_str(), &statbuf) >=0 && S_ISREG(statbuf.st_mode));
					if(found)
					{
						ConfigFile *pManifest= new ConfigFile(path);
						string compressedXMLPath = pManifest->GetSetting("CompressedXML");
						found = (stat((_path+ dirbuf->d_name+"/"+compressedXMLPath).c_str(),&statbuf) >=0 && S_ISREG(statbuf.st_mode));
					}
				}

				if ( found )
				{
					if ( _installedLanguages.empty() ) 
					{
						firstFoundLanguageCode = dirbuf->d_name;
						_installedLanguages = dirbuf->d_name;
					}
					else
						_installedLanguages += string(",") + string(dirbuf->d_name);
					
					// create the cache dir (anyway, either it is existing or not)
					// string cache = _path + dirbuf->d_name + "/cache"; 
					string cache = _cache +  dirbuf->d_name + "/";
					
					struct stat dirstatbuf;
					if ( stat(cache.c_str(), &dirstatbuf) >=0 )
					{
						if ( dirstatbuf.st_ctime>statbuf.st_ctime )
						{
							// article.bin is newer, clear the cache
							// rmdir(cache.c_str());
							// mkdir(cache.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
						}
					}
					else
						mkdir(cache.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
				}
			}
		}
		
		closedir(dir);
	}
	
	// check for our "default" database (contains help and so on)
	path = _basePath + "xx/articles.bin";
	if ( stat(path.c_str(), &statbuf) >=0 && S_ISREG(statbuf.st_mode) )
	{
		if ( _installedLanguages.empty() ) 
		{
			_installedLanguages = "xx";
			firstFoundLanguageCode = "xx";
		}
		else
			_installedLanguages += ",xx";
	}
	
	if ( !IsLanguageInstalled(_defaultLanguageCode) )
		_defaultLanguageCode = firstFoundLanguageCode;
		
	return true;
}

bool Settings::Verbose()
{
	return _verbose;
}

bool Settings::Debug()
{
	return _debug;
}

bool Settings::IsJSMathEnabled()
{
	return _jsmath;
}

bool Settings::ExpandTemplates()
{
	return _expandTemplates;
}

in_addr_t Settings::Addr()
{
	return _addr;
}

int Settings::Port()
{
	return _port;
}

string Settings::Home()
{
	return _home;
}

string Settings::Cache()
{
	return _cache;
}

string Settings::Path()
{
	return _path;
}

string Settings::DefaultLanguageCode()
{
	return _defaultLanguageCode;
}

string Settings::InstalledLanguages()
{
	return _installedLanguages;
}

bool Settings::IsLanguageInstalled(string languageCode)
{
	if ( !languageCode.empty() && _installedLanguages.find(languageCode)!=string::npos )
		return true;
	else
		return false;
}

bool Settings::AreImagesInstalled(string languageCode)
{
	// do we have common images for all languages?
	if ( GetImageIndex("xc")->NumberOfImages() >=0 )
		return true;
	
	// no, check local ones
	return GetImageIndex(languageCode)->NumberOfImages()>0;
}

/*
bool Settings::AreMathsInstalled(string languageCode)
{
	if (GetMathIndex("xc")->NumberOfImages() >=0)
		return true;
	return GetMathIndex(languageCode)->NumberOfImages()>0;
}
*/

string Settings::WebContentPath()
{
	return _webContentPath;
}

string Settings::Version()
{
	return string(version);
}

ConfigFile* Settings::LanguageConfig(string languageCode)
{
	CPPStringUtils::to_lower(languageCode);
	
	LANGUAGECONFIG* languageConfig = (LANGUAGECONFIG*) _languageConfigs;
	while ( languageConfig && languageConfig->languageCode!=languageCode)
		languageConfig = languageConfig->next;
	
	if ( languageConfig )
		return languageConfig->configFile;
	
	languageConfig = new LANGUAGECONFIG;
	
	languageConfig->languageCode = languageCode;
	languageConfig->configFile = new ConfigFile(Path() + languageCode + "/language.config");
	languageConfig->next = (LANGUAGECONFIG*) _languageConfigs;
	
	_languageConfigs = languageConfig;
	
	return languageConfig->configFile;
}

TitleIndex* Settings::GetTitleIndex(string languageCode)
{
	CPPStringUtils::to_lower(languageCode);
	
	TITLEINDEX* titleIndex = (TITLEINDEX*) _titleIndexes;
	while ( titleIndex && titleIndex->languageCode!=languageCode)
		titleIndex = titleIndex->next;
	
	if ( titleIndex )
		return titleIndex->titleIndex;
	
	titleIndex = new TITLEINDEX;
	
	titleIndex->languageCode = languageCode;
	
	// our "special" database is located here
	if ( languageCode=="xx" )
		titleIndex->titleIndex = new TitleIndex(_basePath + languageCode);
	else
		titleIndex->titleIndex = new TitleIndex(Path() + languageCode);
	
	titleIndex->next = (TITLEINDEX*) _titleIndexes;
	
	_titleIndexes = titleIndex;
	
	return titleIndex->titleIndex;
}

ImageIndex* Settings::GetImageIndex(string languageCode)
{
	CPPStringUtils::to_lower(languageCode);
	
	IMAGEINDEX* imageIndex = (IMAGEINDEX*) _imageIndexes;
	while ( imageIndex && imageIndex->languageCode!=languageCode)
		imageIndex = imageIndex->next;
	
	if ( imageIndex )
		return imageIndex->imageIndex;
	
	imageIndex = new IMAGEINDEX;
	
	imageIndex->languageCode = languageCode;
	
	if ( languageCode=="xx" ) // our "special" database is located here
		imageIndex->imageIndex = new ImageIndex(_basePath + languageCode);
	else if ( languageCode=="xc" ) // images from wiki "commons" is locate in the Wikipedia folder itself 
		imageIndex->imageIndex = new ImageIndex(Path());
	else
		imageIndex->imageIndex = new ImageIndex(Path() + languageCode);
	
	imageIndex->next = (IMAGEINDEX*) _imageIndexes;
	
	_imageIndexes = imageIndex;
	
	return imageIndex->imageIndex;
}

MathIndex* Settings::GetMathIndex(string languageCode)
{
	CPPStringUtils::to_lower(languageCode);
	
	MATHINDEX* mathIndex = (MATHINDEX*) _mathIndexes;
	while ( mathIndex && mathIndex->languageCode!=languageCode)
		mathIndex = mathIndex->next;
	
	if ( mathIndex )
		return mathIndex->mathIndex;
	
	mathIndex = new MATHINDEX;
	
	mathIndex->languageCode = languageCode;
	
	if ( languageCode=="xx" ) // our "special" database is located here
		mathIndex->mathIndex = new MathIndex(_basePath + languageCode);
	else if ( languageCode=="xc" ) // maths from wiki "commons" is locate in the Wikipedia folder itself 
		mathIndex->mathIndex = new MathIndex(Path());
	else
		mathIndex->mathIndex = new MathIndex(Path() + languageCode);
	
	mathIndex->next = (MATHINDEX*) _mathIndexes;
	
	_mathIndexes = mathIndex;
	
	return mathIndex->mathIndex;
}


