/*
*  pack.cpp
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

// pack.cpp : Creates image database

const char version[] = "0.65a";

#define _FILE_OFFSET_BITS 64   // needed on Linux

#ifdef _WIN32
#pragma once

// standard includes
#include <iostream>
#include <iomanip>
#include <map>
#include <list>
#include <string>
#define WIN32_LEAN_AND_MEAN 	// Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <ctype.h>

#pragma warning(disable : 4996)
#endif

#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <fstream>

#ifndef _WIN32
#include <dirent.h>
#endif

using namespace std;


#ifdef _WIN32
// MS defines off_t always as 32 bit, so we can't use it :-(
typedef unsigned __int64 offset_t;
// VC7/VC8 has no fseeko, you have to use _fseeki64
#define fseeko _fseeki64
#define ftello _ftelli64
#else 
// on other systems (OS X, Linux), off_t should be 64 bit (which is what we need)
// and fseeko/ftello should be present
typedef off_t offset_t ;
#endif

FILE*	destFile = NULL;

const char* imageDataCommons = "images.bin";
char imageData[] = "images_xx.bin";
offset_t currentDestSize = 0;
string g_imageDb = "images.db";
string g_lang;

char* filenames;
unsigned int filenamesSize = 0;
unsigned int filenamesAllocated = 0;
int filenamesCount = 0;
static time_t tStart;

int* filenamesIndex;

/* true if only the resulting size is calculated */
bool countOnly = false;
bool delayedAdding = true;
bool useDb = false;

/* if the size of an image is larger than this value it will be skipped */
size_t maxSize = 1024*1024;

int skipped = 0;

#pragma pack(push,1)
typedef struct 
{
	char languageCode[2];
	unsigned int numberOfImages;
	
	offset_t	titlesPos;
	offset_t	indexPos;
	char reserved[10];
} FILEHEADER;
#pragma pack(pop)

#define SIZEOF_POSITION_INFORMATION 12

#ifdef __linux__
inline char tolower(char x) {
    return (char)tolower((int)x);
}
inline char toupper(char x) {
    return (char)toupper((int)x);
}
#endif

void tolower_utf8(char* data)
{
	if ( !data  )
		return;
	
	while (*data)
	{
		unsigned char c = *data;
		if ( c<0x80 )
			*data = tolower(c);
		else if ( c==0xc3 && *(data+1)>0 )
		{
			data++;

			c = *data;
			if ( c>=0x80 && c<=0x9e )
				*data = c | 0x20;
		}
		data++;
	}
}

void timeValue(int totalsecs, char *dest)
{
  int secs=totalsecs%60;
  int mins=(totalsecs/60)%60;
  int hours=(totalsecs/3600)%24;
  int days=(totalsecs/86400);
  if(days==0) {
    sprintf(dest,"%d:%02d:%02d",hours,mins,secs);
  } else if(days==1) {
    sprintf(dest,"%d day+%d:%02d:%02d",days,hours,mins,secs);
  } else {
    sprintf(dest,"%d days+%d:%02d:%02d",days,hours,mins,secs);
  }
}


void Quicksort(int left, int right)
{
	if ( left<right )
	{
		int i = left;
		int j = right - 1;
		
		char* pivotTitle = filenames + filenamesIndex[right] + SIZEOF_POSITION_INFORMATION;
		do
		{
			while ( i<right )
			{
				char* title = filenames + filenamesIndex[i] + SIZEOF_POSITION_INFORMATION;
				if ( strcmp(title, pivotTitle)<=0 )
					i++;
				else
					break;
			}
			
			while ( j>left )
			{
				char* title = filenames + filenamesIndex[j] + SIZEOF_POSITION_INFORMATION;
				if ( strcmp(pivotTitle, title)<=0 )
					j--;
				else
					break;
			}
			
			if ( i<j )
			{
				int help = filenamesIndex[i];
				filenamesIndex[i] = filenamesIndex[j];
				filenamesIndex[j] = help;
			}
		}
		while (i < j);
		if ( i!=right )
		{
			int help = filenamesIndex[i];
			filenamesIndex[i] = filenamesIndex[right];
			filenamesIndex[right] = help;
		}
		
		Quicksort(left, i-1);
		Quicksort(i+1, right);
	}
}

void url_decode(char* src)
{
	if ( !src )
		return;
	
	char* dst = src;
	while ( *src )
	{
		unsigned char c = *src++;
		if ( c=='%' && *src && *(src+1) )
		{			
			int number = 0;
			
			unsigned char digit = (unsigned char) *src++;
			digit = toupper(digit);
			if ( digit<='9' )
				digit -= 48;
			else
				digit -= 55;
			number = digit;
			
			digit = (unsigned char) *src++;
			digit = toupper(digit);
			if ( digit<='9' )
				digit -= 48;
			else
				digit -= 55;
			
			number = number*16 + digit;
			*dst++ = (char) number;
		}
		else
			*dst++ = c;
	}
	
	*dst = 0x0;
}

void AddFile(const char* path, const char* cfilename)
{
    char fbuf[512*1024];
	if ( !path || !cfilename )
		return;
	
	char filename[1024];
	strcpy(filename, cfilename);
	
	char* filepathname = (char*) malloc(strlen(path) + strlen(filename) + 2);
	if (!filepathname) {
		printf ("Out of memory error\n"); exit(10);
	}
	strcpy(filepathname, path);
	strcat(filepathname, "/");
	strcat(filepathname, filename);
	
    /*speed up: don't do a stat first, simply read as many bytes as possible with fread 
    struct stat statbuf;
	if (stat(filepathname, &statbuf) < 0) 
	{
		// not found, return
		free(filepathname);
		skipped++;
		return;
	}
	
	size_t length = statbuf.st_size;
	if ( maxSize && (length>maxSize) )
	{
		// to large
		free(filepathname);
		skipped++;
		return;
	}
*/	
	url_decode(filename);
	tolower_utf8(filename);
	
	// Copy the file to the dest file
	FILE* f = fopen(filepathname, "rb");
	free(filepathname);
	if ( !f )
		return;
	
	unsigned char* buffer = (unsigned char*) malloc(maxSize+1);
	if (!buffer) {
		printf ("Out of memory error\n"); exit(10);
	}
    setvbuf( f, fbuf, _IOFBF, 512*1024 );
	size_t length = fread(buffer, 1, maxSize+1, f);
	fclose(f);
	if ( !length || length > maxSize)
	{
		skipped++;
		free(buffer);
		return;
	}
	
	length = fwrite(buffer, 1, length, destFile);
	free(buffer);
	
	// an entry is build like this:
	// 8 bytes block position in the file
	// 4 bytes length of the file
	// filename in utf-8 coding
	// terminating zero
	
	// store the title information	
	if ( (filenamesSize + SIZEOF_POSITION_INFORMATION + strlen(filename) + 1)>filenamesAllocated )
	{
		if ( !filenamesAllocated )
		{
			filenamesAllocated = 32767;
			filenames = (char*) malloc(filenamesAllocated);
		}
		else
		{
			filenamesAllocated += 32767;
			filenames = (char*) realloc(filenames, filenamesAllocated);
		}
	}
	
	//* add it to the index
	char* filenamesBuffer = filenames + filenamesSize;
	
	// store the location of the image inside the file
	memcpy(filenamesBuffer, &currentDestSize, sizeof(currentDestSize));
	filenamesBuffer += sizeof(currentDestSize);
	filenamesSize += sizeof(currentDestSize); 
	
	// store the length of the image
	memcpy(filenamesBuffer, &length, sizeof(length));
	filenamesBuffer += sizeof(length);
	filenamesSize += sizeof(length); 
	
	// store the filename itself
	strcpy(filenamesBuffer, filename);
	filenamesBuffer += strlen(filename) + 1;
	filenamesSize += (unsigned int) strlen(filename) + 1;
	
	currentDestSize += length;
	
	filenamesCount++;
}

#ifdef _WIN32
void AddFilesFromDirectory(const char* path)
{
	char buffer[512];
	strcpy(buffer, path);
	strcat(buffer, "\\*.*");
    list<string> dirList;
    list<string> fileList;

	
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	
	hFind = FindFirstFile(buffer, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		printf ("Invalid file handle. Error is %u\n", GetLastError());
		return;
	}	
	
	bool go = true;
	while ( go )
	{
		if ( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			if ( strcmp(FindFileData.cFileName, ".")!=0 && strcmp(FindFileData.cFileName, "..")!=0 )
			{
				char subfolder[1024];
				strcpy(subfolder, path);
				strcat(subfolder, "\\");
				strcat(subfolder, FindFileData.cFileName);
                dirList.push_back(string(subfolder));
				
				//AddFilesFromDirectory(subfolder);
			}
		}
		else
		{			
			if ( (filenamesCount & 0x1FF)==0 )
			{
                time_t tDur = time(0)-tStart;
				printf("\rfound %6i images so far (%d img/sec)  ",  
                    filenamesCount, filenamesCount / (tDur?tDur:1));
				fflush(stdout);
			}
			if ( countOnly )
			{
				if ( !maxSize || FindFileData.nFileSizeLow<=maxSize )
				{
					char filename[1024];
					strcpy(filename, FindFileData.cFileName);
					url_decode(filename);
					
					currentDestSize += FindFileData.nFileSizeLow;
					filenamesSize += (unsigned int) (sizeof(currentDestSize) + sizeof(offset_t) + strlen(filename) + 1);
					filenamesCount++;
				}
				else
					skipped++;
			}
            else if (delayedAdding)
            {
                string p(path);
                p += '\\';
                p += FindFileData.cFileName;
                fileList.push_back(p);
            }
            else 
				AddFile(path, FindFileData.cFileName);
		}
		go = (FindNextFile(hFind, &FindFileData) != 0);
	}
    FindClose(hFind);

    for (list<string>::iterator it = fileList.begin(); it != fileList.end(); it++)
    {
        int pos = it->rfind('\\');
        AddFile(it->substr(0, pos).c_str(), it->substr(pos+1).c_str());
        if ( (filenamesCount & 0x1FF)==1 )
        {
            time_t tDur = time(0)-tStart;
            printf("\rfound %6i images so far (%d img/sec)  ",  
                filenamesCount, filenamesCount / (tDur?tDur:1));
            fflush(stdout);
        }
    }
    fileList.clear();
    for (list<string>::iterator it = dirList.begin(); it != dirList.end(); it++)
    {
        AddFilesFromDirectory(it->c_str());
    }
    dirList.clear();
}

#else

void AddFilesFromDirectory(const char*path)
{
	DIR* dir = opendir(path);
	if ( !dir )
	{
		printf("failed to open the dir '%s'.\n\n", path);
		fclose(destFile);
		return;
	}
	
	struct dirent* dirbuf;
	while ( dirbuf=readdir(dir) )
	{
		if ( dirbuf->d_type==DT_DIR || dirbuf->d_type==DT_LNK )
		{
			if ( strcmp(dirbuf->d_name, ".")!=0 && strcmp(dirbuf->d_name, "..")!=0 )
			{
				char subfolder[1024];
				strcpy(subfolder, path);
				strcat(subfolder, "/");
				strcat(subfolder, dirbuf->d_name);
				
				AddFilesFromDirectory(subfolder);
			}
		}
		else if ( dirbuf->d_type==DT_REG && strcmp(dirbuf->d_name, ".DS_Store")!=0 )
		{
			if ( (filenamesCount % 1024)==0 )
			{
				printf("\rAdded so far: %i", filenamesCount);
				fflush(stdout);
			}
			
			if ( countOnly )
			{
				char filename[1024];
				strcpy(filename, path);
				strcat(filename, "/");
				strcat(filename, dirbuf->d_name);
				
				struct stat statbuf;
				if (stat(filename, &statbuf) >= 0)
				{
					if ( !maxSize || statbuf.st_size<=maxSize )
					{
						strcpy(filename, dirbuf->d_name);
						url_decode(filename);
						
						currentDestSize += statbuf.st_size;
						filenamesSize += (unsigned int) (sizeof(currentDestSize) + sizeof(offset_t) + strlen(filename) + 1);
						filenamesCount++;
					}
					else
						skipped++;
				}
				else
					skipped++;
			}
			else
				AddFile(path, dirbuf->d_name);
		}
	}
	closedir(dir);
}
#endif

void Help()
{
	printf("Usage:\r\n    pack [-c] [-s max_size] language_code directory\r\n\r\n");
	printf("        language_code: Two character language code (i.e. 'en', 'de')\r\n");
	printf("        directory:     a folder where the images to pack are stored\r\n");
	printf("        -s max_size:   if an image is larger it will be skipped\r\n");
	printf("        -c:            build no destination file but count only the size it will be\r\n\r\n");
}

int scanDatabase()
{
    int lcnt;
    filenamesCount=0;
    ifstream inputfile;
    inputfile.open(g_imageDb.c_str());
    string line;
    lcnt=0;
    if (inputfile.is_open()) {
        string part[6];
        while ( ! inputfile.eof()) {
            getline(inputfile, line);
            int start = 0;
            for (int i=0; i<6; i++) {
                unsigned int nextTab = line.find('\t', start);
                part[i]=line.substr(start, nextTab-start);
                if (nextTab != string::npos) {
                    start=nextTab+1;
                } else {
                    // that was the last part
                    for (int j=i+1; j<6; j++)
                        part[j]="";
                    break;  // jump out of i-loop
                }
            }

            // ok, we scanned and splitted the line, now see what it says to us
            if (part[0] == "OK" && part[2] == g_lang && part[1][0]=='a') {   // found image
                string path = "images/";
                if (g_lang=="xc")
                    path += "commons/";
                else
                    path += g_lang + "/";
                path += part[1][0];
                AddFile(path.c_str(), part[1].c_str());
                ++filenamesCount;//cout << part[1] << endl;
            }
            if ( ((++lcnt) & 0x1ff) == 1) {
                time_t tDur = time(0)-tStart;
				printf("\r[scanning '%s']    found %6i images so far (%d img/sec)  ", g_imageDb.c_str(), 
                    filenamesCount, filenamesCount / (tDur?tDur:1));
				fflush(stdout);
            }
        }
    }
    inputfile.close();
    return filenamesCount;
}

void checkStructSizes()
{
    // we get paranoid here: check that all struct members have the right size
    FILEHEADER fileHeader;
    if (sizeof(fileHeader.numberOfImages) != 4) {
        printf("ERROR: compiled application has int not 32 bit wide\n");
        exit(1);
    }
    if (sizeof(fileHeader.titlesPos) != 8) {
        printf("ERROR: compiled application has off_t not 64 bit wide\n");
        exit(1);
    }
    if (sizeof(fileHeader) != 32) {
        printf("ERROR: compiled application has wrong struct alignment\n");
        exit(1);
    }
    fileHeader.titlesPos = 0x0203;
    unsigned char * p = (unsigned char *)&fileHeader.titlesPos;
    if (p[0] != 3 || p[1] != 2){
        printf("ERROR: compiled application has wrong endianess (1)\n");
        exit(1);
    }
    if (p[6] != 0 || p[7] != 0){
        printf("ERROR: compiled application has wrong endianess (2)\n");
        exit(1);
    }
}
    
int pack(int argc, char* argv[])
{
	char* languageCode = NULL;
	char* path = NULL;
	//maxSize = 0;
	countOnly = false;

	printf("Wiki2Touch (image) file composer\r\nCopyright (c) 2008 T. Haukap\r\nVersion %s\r\n", version);
    checkStructSizes();
    
	if ( argc==1 )
	{
		Help();
		return 1;
	}
	// check for options 
	for (int i=1; i<argc; i++)
	{
		if ( argv[i][0]=='-' )
		{
			char* param = argv[i];
			for (size_t j=1; j<strlen(param); j++)
			{
				switch( argv[i][j] )
				{
					case 'c': 
						countOnly = true;
						break;

					case 'i': 
						delayedAdding = false;
						break;

					case 'd': 
						useDb = true;
						break;

                    case 's':
						if ( strlen(argv[i])==2 && ((i+1)<argc) )
						{
							i++;
							maxSize = atoi(argv[i]);
						}
						else
						{
							Help();
							return 1;
						}
						break;

					default:
						Help();
						return 1;
				}
			}
		}
		else 
		{
			languageCode = argv[i++];
			if ( strlen(languageCode)!=2 )
			{
				Help();
				return 1;
			}
            g_lang = languageCode;

			if (languageCode[0]=='x' && languageCode[1]=='c') {
				// commons, output to images.bin
				strcpy(imageData, imageDataCommons);
			} else {
				imageData[7] = languageCode[0];
				imageData[8] = languageCode[1];
			}

			if ( i>=argc )
			{
				Help();
				return 1;
			}
			path = argv[i++];

			while ( i<argc )
				printf("warning: additional argument '%s' ignored.\r\n", argv[i++]);
		}
	}

	if ( !countOnly )
	{
		// open the dest file for the data
		destFile = fopen(imageData, "wb");
		if ( !destFile )
		{
			printf("Unable to open destination file '%s'\n", imageData);
			return 1;
		}
		printf("Writing to file '%s'", imageData);
        void *buf = malloc(1024*1024);
        if (buf) {
            setvbuf( destFile, (char*)buf, _IOFBF, 1024*1024 );
    		printf("  (buffer %d KB)", 1024);
        }
        printf("\n");
	}
	
	FILEHEADER fileHeader;
    tStart = time(0);
	memset(&fileHeader, 0x0, sizeof(fileHeader));
	fileHeader.languageCode[0] = languageCode[0];
	fileHeader.languageCode[1] = languageCode[1];
	if ( !countOnly )
		fwrite(&fileHeader, sizeof fileHeader, 1, destFile);
	currentDestSize = sizeof(fileHeader);

    if (useDb)
        scanDatabase();
    else 
        AddFilesFromDirectory(path);
 
	if ( !countOnly )
	{
		/* Indexing */
		// append the filename at the end of the file
		fileHeader.titlesPos = ftello(destFile);
		fwrite(filenames, 1, filenamesSize, destFile);
		
		printf("\ncreating index...\n");
		
		char* help = filenames;
		filenamesIndex = (int*) malloc(filenamesCount * sizeof(int));
		int no = 0;
		while ( (help-filenames) < (int) filenamesSize )
		{
			filenamesIndex[no++] = (int) (help - filenames);
			
			// skip the binary position information (8+4 bytes)
			help += SIZEOF_POSITION_INFORMATION;
			
			// go to the start of the next title
			help += (strlen(help) + 1);
		}
		
		fileHeader.numberOfImages = filenamesCount;
		printf("total files added: %i\r\n", filenamesCount);
		
		printf("sorting index...\n");
		Quicksort(0, filenamesCount-1);
		
		printf("checking index: ");
		bool failed = false;
		char* filename = filenames + filenamesIndex[0] + SIZEOF_POSITION_INFORMATION;
		for (int i=1; i<filenamesCount; i++)
		{
			char* next = filenames + filenamesIndex[i] + SIZEOF_POSITION_INFORMATION;
			if ( strcmp(filename, next)>0 )
			{
				failed = true;
				break;
			}
			
			filename = next;
		}
		
		printf("%s\r\n", failed ? "failed" : "passed");
		if ( !failed )
		{	
			/* write out our index */
			printf("writing index: %i bytes\r\n", filenamesCount*sizeof(int));
			
			// finally append the articles index at the end of the file
                        fileHeader.indexPos = ftello(destFile);
			fwrite(filenamesIndex, 1, filenamesCount * sizeof(int), destFile);
		}
		else
			remove(imageData);
		
		free(filenamesIndex);
		free(filenames);
		
		fseek(destFile, 0, SEEK_SET);
		fwrite(&fileHeader, sizeof fileHeader, 1, destFile);
		
		fclose(destFile);
	}
	
	offset_t totalBytes =  currentDestSize + filenamesSize + filenamesCount*sizeof(int);
    time_t tEnd = time(0);
    char elapsed[30];
    time_t tDur = tEnd-tStart;
    timeValue((int)(tDur), elapsed);

	printf("\nDone: %i file(s) added, %i skipped, total number of file(s) is %i\n", filenamesCount, skipped, filenamesCount+skipped);
#ifdef _WIN32
	// Microsoft does not understand %lld
	printf("      total size is %4.2f GB (%I64i bytes)\n", ((double)(totalBytes>>20))/1000, totalBytes);
#else
	printf("      total size is %4.2f GB (%lld bytes)\n", ((double)(totalBytes>>20))/1000, totalBytes);
#endif
	printf("      total time for packaging: %s   (%d img/sec)\n\n", elapsed,	
        filenamesCount / (tDur?tDur:1));
	return 0;
}

int get(int argc, char* argv[])
{
	printf("Wiki2Touch (image) getter\r\nCopyright (c) 2008 T. Haukap\r\nVersion %s\r\n", version);
    checkStructSizes();

	if ( argc!=2 )
	{
		printf("Usage:	get <image_name>\r\n");
		printf("	<image_name>: name of the image to retrieve from 'images.bin'\r\n\r\n");
		return 1;
	}

	FILE* f = fopen(imageData, "rb");
	if ( !f )
	{
		printf("'images.bin' not found.\r\n");
		return 1;
	}

	FILEHEADER fileHeader;		
	if ( fread(&fileHeader, sizeof(FILEHEADER), 1, f)!=1 )
	{
		fclose(f);

		printf("problems reading 'images.bin'.\r\n");
		return 1;
	}
	
	if ( fileHeader.numberOfImages<=0  )
	{
		fclose(f);

		printf("no images detected in 'images.bin'.\r\n");
		return 1;
	}

	char* lowercaseFilename = (char*) malloc(strlen(argv[1]) + 1 + 4); // ".png"?
	strcpy(lowercaseFilename, argv[1]);
	url_decode(lowercaseFilename);
	tolower_utf8(lowercaseFilename);

	// .svg is stored as .png
	if ( strstr(lowercaseFilename, ".svg")==(lowercaseFilename + strlen(lowercaseFilename)-4) )
		strcat(lowercaseFilename, ".png");

	int foundAt = -1;
	int lBound = 0;
	int uBound = fileHeader.numberOfImages - 1;
	int index = 0;	

	offset_t imagePos;
	int imageLength;

	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;

		// get the title at the specific index
		int error = fseeko(f, fileHeader.indexPos + index*sizeof(int), SEEK_SET);
		if ( error )
		{
			fclose(f);
			free(lowercaseFilename);

			printf("problems reading 'images.bin' file.\r\n");
			return 1;
		}

		int titlePos;
		size_t read = fread(&titlePos, sizeof(int), 1, f);

		if ( !read )
		{
			fclose(f);
			free(lowercaseFilename);

			printf("problems reading 'images.bin' file.\r\n");
			return 1;
		}

		error = fseeko(f, fileHeader.titlesPos + titlePos, SEEK_SET);
		if ( error )
		{
			fclose(f);
			free(lowercaseFilename);

			printf("problems reading 'images.bin' file.\r\n");
			return 1;
		}
		
		// store the article location and size for use in the future
		fread(&imagePos, sizeof(imagePos), 1, f);
		fread(&imageLength, sizeof(imageLength), 1, f);

		char filenameAtIndex[512];
		char* help = filenameAtIndex;
		char c;
		while ( !feof(f) && (c=fgetc(f)) ) 
			*help++ = c;
		*help = 0;

		if ( strcmp(lowercaseFilename, filenameAtIndex)<0 )
			uBound = index - 1;
		else if ( strcmp(lowercaseFilename, filenameAtIndex)>0 )
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
		free(lowercaseFilename);

		printf("Not found.\r\n");
		return 1;
	}

	printf("image found.\r\n");
	unsigned char* data = (unsigned char*) malloc(imageLength);
	fseeko(f, imagePos, SEEK_SET);
	size_t size = fread(data, 1, imageLength, f);
	if ( size!=imageLength )
	{
		fclose(f);
		free(data);
		free(lowercaseFilename);
		
		printf("problems reading images data.\r\n");
		return 1;
	}
	fclose(f);

	printf("writing file %s (%i bytes)\r\n", lowercaseFilename, imageLength);
	f = fopen(lowercaseFilename, "wb");
	fwrite(data, 1, imageLength, f);
	fclose(f);

	free(data);
	free(lowercaseFilename);

	printf("Done.\r\n");
	return 0;
}

int main(int argc, char* argv[])
{
	char* help = (char*) malloc(strlen(*argv)+1);
	strcpy(help, *argv);

	char* filename = help + strlen(help);
	while ( filename>help && *filename!='/' && *filename!='\\' )
	{
		if ( *filename=='.' )
			*filename = 0;
		filename--;
	}
	if ( *filename=='/' || *filename=='\\' )
		filename++;

	if ( strcmp(filename, "pack")==0 )
		pack(argc, argv);
	else if ( strcmp(filename, "get")==0 )
		get(argc, argv);
	else
		printf("do not rename this file\r\n");

	free(help);
}
