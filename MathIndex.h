/*
 *  ImageIndex.h
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

#ifndef MATHINDEX_H
#define MATHINDEX_H

#include <string>
using namespace std;

class MathIndex
{
public:
	MathIndex(string pathToDataFile);
	~MathIndex();
	
	int NumberOfImages();
	unsigned char* GetImage(string filename, size_t* size);
	
private:
	string	_dataFileName;
	int		_numberOfImages;
	
	fpos_t	_titlesPos;
	fpos_t	_indexPos;
	
	string GetFilename(FILE* f, int imageNumber);
	
	fpos_t			_lastImagePos;
	unsigned int	_lastImageLength;
};

#endif
