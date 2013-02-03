/*
 *  ImageIndex.cpp
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

#include "ImageIndex.h"
#include "CPPStringUtils.h"
#include <cstdio>
#include <cstdlib>

const char* IMAGES_DATA_NAME = "images";
const char* IMAGES_DATA_EXTENSION = ".bin";

#define SIZEOF_POSITION_INFORMATION 16

#pragma pack(push,1)
typedef struct 
{
	char languageCode[2];
	unsigned int numberOfImages;
	
	fpos_t	titlesPos;
	fpos_t	indexPos;
	char reserved[10];
} IMAGEFILEHEADER;
#pragma pack (pop)

ImageIndex::ImageIndex(string pathToDataFile)
{
	_dataFileName = pathToDataFile;
	if ( _dataFileName.length()>0 && _dataFileName[_dataFileName.length()-1]!='/' )
		_dataFileName += '/';
	_dataFileName += IMAGES_DATA_NAME;
	_dataFileName += IMAGES_DATA_EXTENSION;
	
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
				_dataFileName = pathToDataFile + IMAGES_DATA_NAME + "_" + pathToDataFile.substr(length-3, 2) + IMAGES_DATA_EXTENSION;
		}
		else
		{
			if ( (length>=3) && pathToDataFile[length-3]=='/' )
				_dataFileName = pathToDataFile + "/" + IMAGES_DATA_NAME + "_" + pathToDataFile.substr(length-2, 2) + IMAGES_DATA_EXTENSION;
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

ImageIndex::~ImageIndex()
{
}

unsigned char* ImageIndex::GetImage(string filename, size_t* size)
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
	if ( lowercaseFilename.find(".svg")==lowercaseFilename.length()-4 )
		lowercaseFilename += ".png";

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
	//debug for large images.bin
	fprintf(stderr, "_lastImgaePos:%16llx \n",_lastImagePos);
	*size = fread(data, 1, _lastImageLength, f);
	fclose(f);
	
	return data;
}

int ImageIndex::NumberOfImages()
{
	return _numberOfImages;
}

string ImageIndex::GetFilename(FILE* f, int imageNumber)
{
	_lastImagePos = 0;
	_lastImageLength = 0;					  
						  
	if ( !f || imageNumber<0 || imageNumber>=_numberOfImages  )
		return string();
	
	int error = fseeko(f, _indexPos + imageNumber*sizeof(int), SEEK_SET);
	//debug for large file
	off_t ftellpos=ftello(f);
	fprintf(stderr, "after fseeko _indexPos+imageNumber*sizeofint:0x%llx \n",ftellpos);
	if ( error )
		return string();

	int titlePos=0;
	size_t read = fread(&titlePos, sizeof(int), 1, f);
	fprintf(stderr, "titlePos=0x%x\n",titlePos);

	if ( !read )
		return string();

	error = fseeko(f, _titlesPos + titlePos, SEEK_SET);
	//debug for large file
	ftellpos=ftello(f);
	fprintf(stderr, "after fseeko _titlePos+titlePos:0x%llx \n",ftellpos);
	if ( error )
		return string();
	
	// store the article location and size for use in the future
	fread(&_lastImagePos, sizeof(_lastImagePos), 1, f);
	fread(&_lastImageLength, sizeof(_lastImageLength), 1, f);
	fprintf(stderr, "_lastImagePos=0x%llx\n_lastImageLength=%d\n",_lastImagePos,_lastImageLength);
	
	string result;
	unsigned char c = 0;
	while ( !feof(f) && (c=fgetc(f)) )
		result += c;
	
	return result;
}





