/*
*  indexer.cpp
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

const char version[] = "0.65a";

#ifdef WIN32

// windows defines
#define WIN32_LEAN_AND_MEAN 	// Exclude rarely-used stuff from Windows headers

#ifdef VC6    // VC& must be set in project settings, not set automatically
// VC6 has no fseeko or _fseeki64, use _lseeki64 instead:
#include <io.h>
#define fseeko(a,b,c) _lseeki64(_fileno(a),b,c)
#else
// VC7 has also no fseeko, but _fseeki64
#define fseeko _fseeki64
#endif

#pragma warning(disable : 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <bzlib.h>
#include "char_convert.h"
#include "indexer.h"

#define BUFFER_SIZE 65536
#define CONTENT_SIZE 65536
#define BZIP_BLOCK_SIZE_100K 9
#define SIZEOF_POSITION_INFORMATION 16

#define MAX_IGNORED_NAMESPACES 32

FILE*   srcFile = NULL; 
FILE*	destFile = NULL;
FILE*	imageTitlesFile = NULL;
BZFILE* destBZFile = NULL; 
FILE*	titlesFile = NULL;

int numberOfIgnoredNamespaces = 0;
char** ignoredNamespaces = NULL;
int* articlesIgnoredByNamespaces = NULL;

char  articlesData[20] = "articles.bin";
const char* articlesTitlesTempFileName = "titles.tmp";
char  imageTitlesFileName[20] = "images.txt";

char* imagesPrefix = NULL;

size_t currentDestSize = 0;
fpos_t currentBlockPos = 0;

size_t articlesTitlesSize;
char* articlesTitles;

int* articlesIndex;

bool internalIndexingStats = false;
IndexStatus *indexStatus = NULL;

// global options with their defaults
bool extractImagesOnly = false;					// only get the list of images used by the articles
bool dontExtractImages = false;					// only create the 'article.bin' database but not the lis of images
bool addIndexWithDiacriticsRemoved = false;		// add a second index which is diacrity/simplied chinese aware
bool longOutfileName = false;		            // add language code to the output filename
bool removedUnusedArticles = true;				// skip some namespaces, i.e. Image, Wikipedia or Help
bool verbose = true;							// except for the copyright and done message be silent
bool standaloneApp = true;						// don't write anything to the console window
bool includeTitleInImagelist = false;			// only for debugging: include a line for each scanned articel in images.txt

// Adds an integer value as an utf8 encoded char to the destion string
inline void AddUtf8Coding(unsigned int number, char* dst)
{
	if ( !dst )
		return;
	
	if ( number<0x00080 )
		*dst += number;
	else if ( number<0x00800 ) 
	{
		*dst++ = (0xc0 | (number>>6));
		*dst++ = (0x80 | (number & 0x3f));
	}
	else if ( number<0x010000 )
	{
		*dst++ = (0xe0 | (number>>12));
		*dst++ = (0x80 | (number>>6 & 0x3f));
		*dst++ = (0x80 | (number & 0x3f));
	}
	else 
	{
		*dst++ = (0xf0 | (number>>18));
		*dst++ = (0x80 | (number>>12 & 0x3f));
		*dst++ = (0x80 | (number>>6 & 0x3f));
		*dst++ = (0x80 | (number & 0x3f));
	}
}

// Removes xml style chars chars like "&gt"; "&amp;" or "#nnn;" from the given src string
static void XmlDecode(char* src)
{
	char* dst = src;
	while ( *src )
	{
		char c = *src++;
		if ( c=='&' )
		{
			char* tag = src;
			while ( *src && *src!=';' )
				src++;
			
			if ( *src )
			{
				*src++ = 0x0;
				
				if ( !strcmp(tag, "lt") )
					*dst++ = '<';
				else if ( !strcmp(tag, "gt") )
					*dst++ = '>';
				else if ( !strcmp(tag, "amp") )
					*dst++ = '&';
				else if ( !strcmp(tag, "quot") )
					*dst++ = '"';
				else if ( *tag=='#' && strlen(tag)>1 )
				{
					tag++;
					unsigned int number = atoi(tag);
					AddUtf8Coding(number, dst);
				}
				else
				{
					*dst++ = '&';
					strcpy(dst, tag);
					dst += (src - tag - 1);
					*dst++ = ';';
				}
			}
		}
		else
			*dst++ = c;
	}
	
	*dst = 0;
}

static void tolower_utf8(char* data)
{
	if ( !data )
		return;
	
	while ( *data )
	{
		unsigned char c = *data;
		if ( c<0x80 )
			*data = tolower(c);
		else if ( c==0xc3 && *(data+1) )
		{
			data++; 		
			
			c = *data;
			if ( c>=0x80 && c<=0x9e )
				*data = c | 0x20;
		}
		data++;
	}
}

static void Quicksort(int left, int right)
{
	if ( left<right )
	{
		int i = left;
		int j = right - 1;
		
		char* pivotTitle = articlesTitles + articlesIndex[right] + SIZEOF_POSITION_INFORMATION;
		do
		{
			while ( i<right )
			{
				char* title = articlesTitles + articlesIndex[i] + SIZEOF_POSITION_INFORMATION;
				if ( strcmp(title, pivotTitle)<=0 )
					i++;
				else
					break;
			}
			
			while ( j>left )
			{
				char* title = articlesTitles + articlesIndex[j] + SIZEOF_POSITION_INFORMATION;
				if ( strcmp(pivotTitle, title)<=0 )
					j--;
				else
					break;
			}
			
			if ( i<j )
			{
				int help = articlesIndex[i];
				articlesIndex[i] = articlesIndex[j];
				articlesIndex[j] = help;
			}
		}
		while (i < j);
		if ( i!=right )
		{
			int help = articlesIndex[i];
			articlesIndex[i] = articlesIndex[right];
			articlesIndex[right] = help;
		}
		
		Quicksort(left, i-1);
		Quicksort(i+1, right);
	}
}

static void ExtractImageNames(char* text, char* lowerText, const char* extension, size_t extensionLength)
{
	// no check here, peformance!
	// if ( !text || !*text || !extension || !*extension )
	//	return;
	
	char* pos = lowerText; 
	
	// search text for extension
	while ( (pos=strstr(pos, extension)) )
	{
        // extension found. first check if after the extension there is no alphanumeric
        // like in "http://www.giftpflanzen.de" which is found in search for .gif
        // We limit here to letters, numbers and dot
        char nextChar = pos[extensionLength];
        if (nextChar >= 'a' && nextChar <= 'z' || nextChar >= '0' && nextChar <= '9' || nextChar == '.')
        {
            // this was not a real image extension, continue searching after the just found extension
            pos += extensionLength;
            continue;
        }

		// correct extension found, search start of image name (limited by = or : or | etc)
		// blanks are valid filename characters, so we can't look for blanks as delimiter

		// switch over to the not lowercase text to get the proper filename
		char* found = text + (pos-lowerText);
		char* current = found - 1;
		
		while(current>text 
			&& *current != '=' 
			&& *current != ':' 
			&& *current != '|' 
			&& *current != '[' 
			&& *current != ']' 
			&& *current != '{' 
			&& *current != '}' 
			&& *current != '/' 
			&& *current != '\\' 
			&& *current != '\n' )
			current--;
		current++;
		
		// skip leading whitespace
		while( *current==' ' || *current=='\t' )
			current++;

		// check if we found a valid name, skip comments "<!--"
		if ( found > current && strncmp(current, "<!--", 4) != 0
			&& strncmp(current, "!--", 3) != 0)
		{
			size_t length = (found-current) + extensionLength;
            if (length < 200) {   // skip extremely long (probably false) names
			    fwrite(current, 1, length, imageTitlesFile);
			    fputc(10, imageTitlesFile);
            }
		} 

		// continue searching after the just found extension
		pos += extensionLength;
	}
}

static void WriteArticle(char* title, char* text)
{
	if ( !title || !text )
		return;
	
	// fix the title, remove xml codings
	XmlDecode(title);
	
	// do we need this article?
	if ( numberOfIgnoredNamespaces )
	{
		char* colon = strstr(title, ":");
		if ( colon )
		{
			*colon = 0x0;
			
			int i = 0;
			while ( i<numberOfIgnoredNamespaces )
			{
				if ( !strcmp(ignoredNamespaces[i], title) )
				{
					indexStatus->bytesSkipped += strlen(text);
					indexStatus->articlesSkipped++;
					articlesIgnoredByNamespaces[i]++;
					return;
				}
				
				i++;
			}
			
			*colon = ':';
		}
	}
	
	// fix the text, remove xml codings
	XmlDecode(text);
	size_t length = strlen(text);
	
	if ( !dontExtractImages )
	{
		// extract all image links in the text. This is not easy, as the Wiki language allows
		// a lot of variants. Our current approach: Look for known extensions and
		// treat everything in front of that extension as the filename

		char* lowerText = (char*) malloc(length+1);

		char *src = text;
		char *dst = lowerText;
		while ( *src )
			*dst++ = tolower(*src++);
		*dst = 0;
        if (includeTitleInImagelist)  // this is for finding problems easier, not for end users
            fprintf(imageTitlesFile, "====T:== %s ====\n", title);

		ExtractImageNames(text, lowerText, ".gif", 4);
		ExtractImageNames(text, lowerText, ".png", 4);
		ExtractImageNames(text, lowerText, ".jpg", 4);
		ExtractImageNames(text, lowerText, ".jpeg", 5);
		ExtractImageNames(text, lowerText, ".svg", 4);

		free(lowerText);
		
        if ( extractImagesOnly ) {
        	indexStatus->articlesWritten++;
			return; // all done
        }
	}
	
	int bzerror = BZ_OK;
	if ( currentDestSize+length>BZIP_BLOCK_SIZE_100K*100000 )
	{
		if ( destBZFile ) 
		{
			BZ2_bzWriteClose(&bzerror, destBZFile, 0, NULL, NULL);
			destBZFile = NULL;
		}
	}
	
	if ( !destBZFile )
	{
		fgetpos(destFile, &currentBlockPos);
		currentDestSize = 0;
		indexStatus->blockCount++;
		
		destBZFile = BZ2_bzWriteOpen(&bzerror, destFile, BZIP_BLOCK_SIZE_100K, 0, 30);
	}
	
	// an entry is build like this:
	// 8 bytes block position in the file
	// 4 bytes position inside the block
	// 4 bytes length of the article
	// title in plain utf-8 coding
	// terminating zero
	
	fwrite(&currentBlockPos, 1, sizeof currentBlockPos, titlesFile);
	fwrite(&currentDestSize, 1, sizeof currentDestSize, titlesFile);
	fwrite(&length, 1, sizeof length, titlesFile);
	fwrite(title, 1, strlen(title)+1, titlesFile);
	
	// put the text to the articles data file
	BZ2_bzWrite(&bzerror, destBZFile, text, (int) length);
	currentDestSize += length;
	
	indexStatus->articlesWritten++;
	indexStatus->bytesTotal += length;
}

void Help()
{
	if ( !standaloneApp )
		return;

	printf("Usage:\r\n  indexer [-adIi] <filename> \r\n\r\n");
	printf("   filename: articles file from wikipedia like 'enwiki-latest-pages-articles.xml.bz2'\r\n");
	printf("    -a : Add all articles (not removing namespaces like 'Help', 'Category' or so)\r\n");
	printf("    -d : Add an index where the diacritics (i.e. иж, ? or so) in titles are not taken into account\r\n");
	printf("    -I : Not scan for images. 'images.txt' is not procduced.\r\n");
	printf("    -i : Scan for images only. An 'articles.bin' is not produced.\r\n");
	printf("    -s : Silent, don't produce any output on the console window (except the copright message).\r\n");
	printf("    -l : Use long output filename (include language code) for articles_xx.bin.\r\n");
	printf("\r\n");
}

void FreeIngoredNamespacesData()
{
	if ( ignoredNamespaces )
	{
		int i = 0;
		while ( i<numberOfIgnoredNamespaces )
		{
			if ( ignoredNamespaces[i] )
				free(ignoredNamespaces[i]);
			i++;
		}

		free(ignoredNamespaces);
		ignoredNamespaces = 0;
	}
	numberOfIgnoredNamespaces = 0;

	if ( articlesIgnoredByNamespaces )
	{
		free(articlesIgnoredByNamespaces);
		articlesIgnoredByNamespaces = NULL;
	}
}

void CleanupAfterError()
{
	if ( titlesFile )
	{
		fclose(titlesFile);
		remove(articlesTitlesTempFileName);
		titlesFile = NULL;
	}

	if ( destFile )
	{
		fclose(destFile);
		remove(articlesData);
		destFile = NULL;
	}

	if ( srcFile )
	{
		fclose(srcFile);
		srcFile = NULL;
	}

	if ( internalIndexingStats )
	{
		free(indexStatus);
		indexStatus = NULL;
		internalIndexingStats = false;
	}

	if ( articlesIndex )
	{
		free(articlesIndex);
		articlesIndex = NULL;
	}

	if ( articlesTitles )
	{
		free(articlesTitles);
		articlesTitles = NULL;
	}

	if ( imagesPrefix )
	{
		free(imagesPrefix);
		imagesPrefix = NULL;
	}

	FreeIngoredNamespacesData();
}

// The repacker/indexer
static int index(int argc, char* argv[])
{
	if ( standaloneApp )
		printf("Wiki2Touch repackager/indexer\r\nCopyright (c) 2008 T. Haukap\r\nVersion %s\r\n", version);

	char* sourceFileName = NULL;
	articlesTitles = NULL;
	
	// check for options 
	for (int i=1; i<argc; i++)
	{
		if ( argv[i][0]=='-' )
		{
			for (size_t j=1; j<strlen(argv[i]); j++)
			{
				switch( argv[i][j] )
				{
				case 'd': 
					addIndexWithDiacriticsRemoved = true;
					break;
					
				case 'a':
					removedUnusedArticles = false;
					break;
					
				case 'i':
					extractImagesOnly = true;
					break;
					
				case 'I':
					dontExtractImages = true;
					break;

				case 'T':   // for debugging, undocumented for user in help
					includeTitleInImagelist = true;
					break;

				case 's':
					verbose = false;
					break;

				case 'l':
					longOutfileName = true;
					break;

				default:
					Help();
					break;
				}
			}
		}
		else 
		{
			sourceFileName = argv[i];
			
			i++;

			if ( standaloneApp )
			{
				while ( i<argc )
					printf("warning: additional argument '%s' ignored.\r\n", argv[i++]);
			}
			else
				i = argc;
		}
	}
	
	if ( !sourceFileName )
	{
		Help();
		return 1;
	}

    if (longOutfileName) 
    {
        strcpy(articlesData, "articles_xx.bin");
        articlesData[9] = sourceFileName[0];
        articlesData[10] = sourceFileName[1];
        strcpy(imageTitlesFileName, "images_xx.txt");
        imageTitlesFileName[7] = sourceFileName[0];
        imageTitlesFileName[8] = sourceFileName[1];
    }
	
	if ( dontExtractImages && extractImagesOnly )
	{
		if ( standaloneApp )
			printf("Extracting images only while at the same time not extracting images is simply not possible\r\n");

		return 1;
	}
	
	// init some globals (only necessary if the GUI indexer is used but who cares)
	srcFile = NULL; 
	destFile = NULL;
	imageTitlesFile = NULL;
	destBZFile = NULL; 
	titlesFile = NULL;

	numberOfIgnoredNamespaces = 0;
	ignoredNamespaces = NULL;
	articlesIgnoredByNamespaces = NULL;

	currentDestSize = 0;
	imagesPrefix = NULL;

	imagesPrefix = NULL;

	currentDestSize = 0;
	currentBlockPos = 0;

	articlesTitlesSize = 0;;
	articlesTitles = NULL;

	articlesIndex = 0;

	// reserve space for 32 entries from the start
	ignoredNamespaces = (char**) malloc(sizeof(char*)*MAX_IGNORED_NAMESPACES);
	memset(ignoredNamespaces, 0x00, sizeof(char*)*MAX_IGNORED_NAMESPACES);
	
	articlesIgnoredByNamespaces = (int*) malloc(sizeof(int)*MAX_IGNORED_NAMESPACES);
	memset(articlesIgnoredByNamespaces, 0x00, sizeof(int)*MAX_IGNORED_NAMESPACES);

	internalIndexingStats = !indexStatus;
	if ( internalIndexingStats ) 
		indexStatus = (IndexStatus*) malloc(sizeof(IndexStatus));
	memset(indexStatus, 0x0, sizeof(IndexStatus));

	if ( verbose )
		printf("working on %s\r\n", sourceFileName);

	srcFile = fopen((char*) sourceFileName, "rb");
	if ( !srcFile )
	{
		CleanupAfterError();
		if ( standaloneApp )
			printf("error opening source file\r\n");
		return 1;
	}
	
	// try to ge the size
	fpos_t sourceSize;
	fseeko(srcFile, 0, SEEK_END);
	fgetpos(srcFile, &sourceSize);
	fseeko(srcFile, 0, SEEK_SET);
	
	int bzerror;
	BZFILE *bzf = BZ2_bzReadOpen(&bzerror, srcFile, 0, 0, NULL, 0);
	
	destFile = 0;
	if ( !extractImagesOnly )
	{
		// open the dest file for the data
		destFile = fopen(articlesData, "wb");
		if ( !destFile )
		{
			CleanupAfterError();
			return 1;
		}
	}
	else if (verbose)
		printf("extracting images only.\r\n");
	
	FILEHEADER fileHeader;
	memset(&fileHeader, 0, sizeof fileHeader);
	fileHeader.languageCode[0] = tolower(sourceFileName[0]);
	fileHeader.languageCode[1] = tolower(sourceFileName[1]);
	fileHeader.version = 1;
	
	bool chinesesCharacters = (fileHeader.languageCode[0]=='z' && fileHeader.languageCode[1]=='h');
	
	titlesFile = 0;
	if ( !extractImagesOnly )
	{
		fwrite(&fileHeader, sizeof(FILEHEADER), 1, destFile);
		titlesFile = fopen(articlesTitlesTempFileName, "wb");
		if ( !titlesFile )
		{
			CleanupAfterError();
			return 1;
		}
	}
	
	imageTitlesFile = 0;
	if ( !dontExtractImages )
	{
		imageTitlesFile = fopen(imageTitlesFileName, "wb");
		if ( !imageTitlesFile )
		{
			CleanupAfterError();
			return 1;
		}
	}
	
	char buffer[BUFFER_SIZE];
	size_t read;
	
	int state = 0;
	bool endTag = false;
	
	char tagName[512];
	char* pTagName = NULL;
	int tagNameSize = 0;
	
	char tagAttributes[512];
	char* pTagAttributes = NULL;
	int tagAttributesSize = 0;
	
	char* content = NULL;
	char* pContent = NULL;
	int contentSize = 0;
	int contentRemain = 0;
	
	int namespaceKey = 0;
	
	fpos_t totalBytes = 0;
	
	char* articleTitle = NULL;
	while ( (read=BZ2_bzRead(&bzerror, bzf, buffer, BUFFER_SIZE)) )
	{
		char *buf = buffer;
		totalBytes += read;
		
		while ( read-- )
		{
			char c = *buf++;
			switch (state)
			{
			case 0:
				// inside text
				if ( c=='<' )
				{
					if ( content )
						*pContent = 0x0;
					
					pTagAttributes = NULL;
					pTagName = NULL;
					endTag = false;
					
					state = 1;
				}
				else if ( content )
				{
					// collect the content of the tag
					if ( !contentRemain )
					{
						// realloc
						contentSize += CONTENT_SIZE;
						content = (char*) realloc(content, contentSize + 1);
						contentRemain = CONTENT_SIZE;
						
						pContent = content + (contentSize - CONTENT_SIZE);
					}
					
					*pContent++ = c;
					contentRemain--;
				}
				break;
				
			case 1:
				// inside tag
				if ( c=='>' )
				{
					state = 0;
					
					// done reading the tag
					if ( pTagName )
						*pTagName = 0x0;
					else
						tagName[0] = 0x0;
					
					if ( pTagAttributes )
						*pTagAttributes++ = 0x0;
					else
						tagAttributes[0] = 0x0;
					
					if ( !endTag )
					{
						if ( !strcmp(tagName, "title") || !strcmp(tagName, "text") || !strcmp(tagName, "namespace") )
						{
							if ( content )
								free(content);

							// start collecting the content
							contentSize = CONTENT_SIZE;
							content = (char*) malloc(contentSize + 1);
							contentRemain = CONTENT_SIZE;
							
							pContent = content;
							
							if ( !strcmp(tagName, "namespace") )
							{
								namespaceKey = 0;
								
								// save the key
								char* key = strstr(tagAttributes, "key=\"");
								if ( key ) 
								{
									key += 5;
									
									char buffer[512];
									char* pBuffer = buffer;
									while ( *key && *key!='"' )
										*pBuffer++ = *key++;
									*pBuffer = 0;
									
									namespaceKey = atoi(buffer);
								}
							}
						}
					}
					else if ( endTag )
					{
						if ( !strcmp(tagName, "title") )
						{
							if ( articleTitle )
								free(articleTitle);
							
							// store the title for later use
							articleTitle = content;
							
							// this prevents that the memory is freed
							content = NULL;
						}
						else if ( !strcmp(tagName, "text") )
						{
							if ( articleTitle )
							{
								WriteArticle(articleTitle, content);
								
								free(articleTitle);
								articleTitle = NULL;
							}
							
							if ( (indexStatus->articlesWritten&0x0ff)==0 )
							{
								fpos_t currentPos;
								fgetpos(srcFile, &currentPos);
								indexStatus->progress = (int)(10000*currentPos/sourceSize);
								if ( verbose && (indexStatus->articlesWritten&0x03ff)==0 )
								{
									printf("\rProcessed: %.0f%% (%i articles) ", (100.0*currentPos/sourceSize), indexStatus->articlesWritten);
									fflush(stdout);
								}
							}
						}
						else if ( !strcmp(tagName, "namespace") )
						{
							if ( namespaceKey && content && strlen(content) )
							{
								if ( strlen(content)<32 )
								{
									if ( namespaceKey==6 && strlen(content)<32 )
									{
										strcpy(fileHeader.imageNamespace, content);
										
										// only do that if it's not Image, these will always be searched
										if ( strcmp(content, "Image") )
										{
											// get it into the proper form
											imagesPrefix = (char*) malloc(2 + strlen(content) + 1 + 1);
											strcpy(imagesPrefix, "[[");
											strcat(imagesPrefix, content);
											strcat(imagesPrefix, ":");
										}
									}
									else if ( namespaceKey==10 && strlen(content)<32 )
										strcpy(fileHeader.templateNamespace, content);
									
									if ( removedUnusedArticles )
									{
										// add some namespaces to the  list of ignored ones
										switch ( namespaceKey )
										{
										case -2: // Media
										case -1: // Wiki
										case  1: // Talk
										case  2: // User  
										case  3: // User talk
										case  4: // Wikipedia
										case  5: // Wikipedia talk
										case  6: // Image
										case  7: // Image talk
										case  8: // Mediawiki
										case  9: // Mediawiki talk
										case 12: // Help 
										case 13: // Help talk
										case 14: // Category
										case 15: // Category talk
											if ( numberOfIgnoredNamespaces<MAX_IGNORED_NAMESPACES)
											{
												ignoredNamespaces[numberOfIgnoredNamespaces] = (char*) malloc(strlen(content)+1);
												strcpy(ignoredNamespaces[numberOfIgnoredNamespaces], content);
												if ( verbose )
													printf("Articles in namespace '%s' are ignored\r\n", ignoredNamespaces[numberOfIgnoredNamespaces]);
												numberOfIgnoredNamespaces++;
											}
											break;
										}
									}
								}
							}
						}
						
						if ( content )
							free(content);
						
						content = NULL;
						contentRemain = 0;
						contentSize = 0;
					}
				}
				else if ( !pTagName )
				{
					// first char after the "<"
					
					pTagName = tagName;
					tagNameSize = 0;
					
					if (c=='/')
						endTag = true;
					else if (c!=' ') 
						*pTagName++ = c; 
					else 
						pTagAttributes = tagAttributes; 						
				}
				else if ( !pTagAttributes )
				{
					if ( c==' ' )
						pTagAttributes = tagAttributes; 						
					else if ( tagNameSize<512 )
					{
						*pTagName++ = c; 
						tagNameSize++;
					}
				}
				else
				{
					if ( tagAttributesSize<512 )
					{
						*pTagAttributes++ = c;
						tagAttributesSize++;
					}
				}
				break;
			}
		}
	}

	if ( content )
	{
		free(content);
		content = NULL;
	}
	
	if ( extractImagesOnly ) 
	{
		fclose(imageTitlesFile);
		imageTitlesFile = NULL;

		fclose(srcFile);
		srcFile = NULL;

		if ( standaloneApp )
			printf("\nScanning for images done: %i articles scanned, %i articles skipped\n", indexStatus->articlesWritten, indexStatus->articlesSkipped);
		return 0;
	}

	if ( verbose )
#ifdef WIN32
	printf("\n\rRepackaging done:\r\n%I64d bytes before, %i articles written (%I64d bytes)\r\n%i articles skipped (%I64d bytes)\r\n%i blocks\r\n",
		totalBytes,
		indexStatus->articlesWritten,
		indexStatus->bytesTotal,
		indexStatus->articlesSkipped,
		indexStatus->bytesSkipped,
		indexStatus->blockCount
		);

#else
	printf("\n\rRepackaging done:\r\n%lld bytes before, %i articles written (%lld bytes)\r\n%i articles skipped (%lld bytes)\r\n%i blocks\r\n",
		totalBytes,
		indexStatus->articlesWritten,
		indexStatus->bytesTotal,
		indexStatus->articlesSkipped,
		indexStatus->bytesSkipped,
		indexStatus->blockCount
		);
	
#endif
	if ( verbose )
	{
		if ( numberOfIgnoredNamespaces )
		{
			int i = 0;
			while ( i<numberOfIgnoredNamespaces )
			{
				if ( articlesIgnoredByNamespaces[i] )
					printf("%i articles skipped in namespace '%s'\r\n", articlesIgnoredByNamespaces[i], ignoredNamespaces[i]);
				i++;
			}
		}
		
		printf("Images namespace is: %s\r\n", fileHeader.imageNamespace);
		printf("Templates namespace is : %s\r\n", fileHeader.templateNamespace);
	}
	
	if ( destBZFile ) 
	{
		BZ2_bzWriteClose(&bzerror, destBZFile, 0, NULL, NULL);
		destBZFile = NULL;
	}
	if ( titlesFile )
	{
		fclose(titlesFile);
		titlesFile = NULL;
	}
	
	BZ2_bzReadClose(&bzerror, bzf);
	fclose(srcFile);
	srcFile = NULL;
	
	if ( imageTitlesFile )
	{
		fclose(imageTitlesFile);
		imageTitlesFile = NULL;
	}
	
	if ( extractImagesOnly )
	{
		// all done
		if ( standaloneApp )
			printf("extracting images only done.\r\n");
		return 0;
	}
	
	/* Indexing (one or two are created )*/
	int index = 0;
	int indexes = addIndexWithDiacriticsRemoved ? 2 : 1;
	for (index = 0; index<indexes; index++)
	{
		if ( verbose )
		{
			printf("\r\nIndexing: Processing index number %i\r\n", index);
			printf("reading articles titles\r\n");
		}

		struct stat statbuf;
		if (stat(articlesTitlesTempFileName, &statbuf) < 0) 
		{
			CleanupAfterError();
			if ( standaloneApp )
				printf("file not found.\r\n");
			
			return 1;
		}
		
		articlesTitlesSize = statbuf.st_size;
		articlesTitles = (char*) malloc(articlesTitlesSize);
		if ( !articlesTitles )
		{
			CleanupAfterError();
			if ( standaloneApp )
				printf("out of memory.\r\n");
			
			return 1;
		}
		
		FILE* f = fopen(articlesTitlesTempFileName, "rb");
		if ( !f )
		{
			CleanupAfterError();
			if ( standaloneApp )
				printf("problems opening file of article titles.\r\n");
			
			return 1;
		}
		
		read = fread(articlesTitles, 1, articlesTitlesSize, f); 
		if ( read!=articlesTitlesSize )
		{
			CleanupAfterError();
			if ( standaloneApp )
				printf("problems reading titles file.\r\n");
			
			return 1;
		}
		fclose(f);
		
		if (destFile &&  index==0)
		{
			// on the first run append the article titles at the end of the file
			fgetpos(destFile, &fileHeader.titlesPos);
			fwrite(articlesTitles, 1, articlesTitlesSize, destFile);
			
			fileHeader.numberOfArticles = indexStatus->articlesWritten;
		}
		
		if ( verbose )
			printf("lowering and indexing articles titles\r\n");
		
		articlesIndex = (int*) malloc(indexStatus->articlesWritten * sizeof(int));
		int no = 0;
		
		char* help = articlesTitles;
		while ( (help-articlesTitles) < (int) read )
		{
			articlesIndex[no++] = (int) (help - articlesTitles);
			
			// skip the binary position information (8+4+4 bytes)
			help += SIZEOF_POSITION_INFORMATION;
			
			size_t length = strlen(help);
			
			// lower the title (it is utf8 encoded so take that into account)
			tolower_utf8(help);
			
			// remove the diacritics (only for index number 1)
			// attention this may change the length so length calculation has to be done before
			if ( index==1 ) 
			{
				if ( !chinesesCharacters )
					exchange_diacritic_chars_utf8(help);
				else
					tc2sc_utf8(help);
			}
			
			// go to the start of the next title
			help += (length + 1);
		}
		
		if ( verbose )
			printf("sorting index\r\n");
		Quicksort(0, indexStatus->articlesWritten-1);
		
		if ( verbose )
			printf("checking index: ");
		
		bool failed = false;
		char* title = articlesTitles + articlesIndex[0] + SIZEOF_POSITION_INFORMATION;
		for (int i=1; i<indexStatus->articlesWritten; i++)
		{
			char* next = articlesTitles + articlesIndex[i] + SIZEOF_POSITION_INFORMATION;
			if ( strcmp(title, next)>0 )
			{
				failed = true;
				break;
			}
			
			title = next;
		}
		
		if ( verbose )
			printf("%s\r\n", failed ? "failed" : "passed");
		
		if ( !failed )
		{	
			/* write out our index */
			if ( verbose )
				printf("writing index: %i bytes\r\n", indexStatus->articlesWritten*sizeof(int));
			
			// finally append the articles index at the end of the file
			if ( index==0 )
				fgetpos(destFile, &fileHeader.indexPos_0);
			else
				fgetpos(destFile, &fileHeader.indexPos_1);
			fwrite(articlesIndex, 1, indexStatus->articlesWritten * sizeof(int), destFile);
		}
		else
		{
			CleanupAfterError();
			if ( standaloneApp )
				printf("index check failed.\r\n");

			return 1;
		}
		
		free(articlesIndex);
		free(articlesTitles);
	}
	
	// remove the temp file
	remove(articlesTitlesTempFileName);
	
	if ( imagesPrefix )
		free(imagesPrefix);
	
	FreeIngoredNamespacesData();

	if ( internalIndexingStats )
	{
		free(indexStatus);
		indexStatus = NULL;
		internalIndexingStats = false;
	}

	fseek(destFile, 0, SEEK_SET);	// NOT fseeko, the old VC6 has problem with it
	fwrite(&fileHeader, sizeof fileHeader, 1, destFile);
	fclose(destFile);
	destFile = NULL;
	
	if ( standaloneApp )
		printf("Done.\r\n\r\n");
	
	return 0;
}

int main(int argc, char **argv){
	return index(argc, argv);
}
