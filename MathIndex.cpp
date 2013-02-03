/*
 *  MathIndex.cpp
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

#include "MathIndex.h"
#include "CPPStringUtils.h"
#include <cstdlib>

const char* MATH_DATA_NAME = "math";
const char* MATH_DATA_EXTENSION = ".bin";

#define SIZEOF_POSITION_INFORMATION 16

#pragma pack(push,1)
typedef struct 
{
	char languageCode[2];
	unsigned int numberOfImages;
	
	off_t	titlesPos;
	off_t	indexPos;
	char reserved[10];
} IMAGEFILEHEADER;
#pragma pack(pop)

MathIndex::MathIndex(string pathToDataFile)
{
	_dataFileName = pathToDataFile;
	if ( _dataFileName.length()>0 && _dataFileName[_dataFileName.length()-1]!='/' )
		_dataFileName += '/';
	_dataFileName += MATH_DATA_NAME;
	_dataFileName += MATH_DATA_EXTENSION;
	
	_numberOfImages = -1;
		
	// get the number of articles; this also checks if the files exists
	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
	{
		// second try
		_dataFileName = "";
		
		size_t length = pathToDataFile.length();
		if ( length && pathToDataFile[length-1]=='/' )
		{
			if ( (length>=4) && pathToDataFile[length-4]=='/' )
				_dataFileName = pathToDataFile + MATH_DATA_NAME + "_" + pathToDataFile.substr(length-3, 2) + MATH_DATA_EXTENSION;
		}
		else
		{
			if ( (length>=3) && pathToDataFile[length-3]=='/' )
				_dataFileName = pathToDataFile + "/" + MATH_DATA_NAME + "_" + pathToDataFile.substr(length-2, 2) + MATH_DATA_EXTENSION;
		}
		
		if ( _dataFileName!="" )
			f = fopen(_dataFileName.c_str(), "rb");
	}
	
	if ( f )
	{
		int error = 0;

		IMAGEFILEHEADER fileheader;		
		if ( fread(&fileheader, sizeof(IMAGEFILEHEADER), 1, f)!=1 )
			error = 1;
		
		if ( !error ) 
		{			
			_numberOfImages = fileheader.numberOfImages;
			_titlesPos = fileheader.titlesPos;
			_indexPos = fileheader.indexPos;
		}
		
		fclose(f);
	}
	else
		_dataFileName = "";
}

MathIndex::~MathIndex()
{
}

unsigned char* MathIndex::GetImage(string filename, size_t* size)
{
	_lastImagePos = -1;
	_lastImageLength = 0;
	
	if ( !size )
		return NULL;
	*size = 0;
	
	if ( _numberOfImages<=0  )
		return NULL;

	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
		return NULL;

	string lowercaseFilename = CPPStringUtils::to_lower_utf8(filename);
	
	// .svg is stored as .png
	// Not needed by math, at present
	//if ( lowercaseFilename.find(".svg")==lowercaseFilename.length()-4 )
	//	lowercaseFilename += ".png";

	int foundAt = -1;
	int lBound = 0;
	int uBound = _numberOfImages - 1;
	int index = 0;	

	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;
		
		// get the title at the specific index
		string filenameAtIndex = GetFilename(f, index);
		
		if ( lowercaseFilename<filenameAtIndex )
			uBound = index - 1;
		else if ( lowercaseFilename>filenameAtIndex )
			lBound = ++index;
		else
		{
			foundAt = index;
			break;
		}
	}
	
	if ( foundAt<0 )
	{
		fclose(f);
		return NULL;
	}

	if ( _lastImagePos<0 || _lastImageLength==0 )
	{
		fclose(f);
		return NULL;
	}
	
	unsigned char* data = (unsigned char*) malloc(_lastImageLength);
	fseeko(f, _lastImagePos, SEEK_SET);
	*size = fread(data, 1, _lastImageLength, f);
	fclose(f);
	
	return data;
}

int MathIndex::NumberOfImages()
{
	return _numberOfImages;
}

string MathIndex::GetFilename(FILE* f, int imageNumber)
{
	_lastImagePos = 0;
	_lastImageLength = 0;					  
						  
	if ( !f || imageNumber<0 || imageNumber>=_numberOfImages  )
		return string();
	
	int error = fseeko(f, _indexPos + imageNumber*sizeof(int), SEEK_SET);
	if ( error )
		return string();

	int titlePos;
	size_t read = fread(&titlePos, sizeof(int), 1, f);

	if ( !read )
		return string();

	error = fseeko(f, _titlesPos + titlePos, SEEK_SET);
	if ( error )
		return string();
	
	// store the article location and size for use in the future
	fread(&_lastImagePos, sizeof(_lastImagePos), 1, f);
	fread(&_lastImageLength, sizeof(_lastImageLength), 1, f);
	
	string result;
	unsigned char c = 0;
	while ( !feof(f) && (c=fgetc(f)) )
		result += c;
	
	return result;
}





