/*
 *  TitleIndex.cpp
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

#include "TitleIndex.h"
#include "CPPStringUtils.h"
#include <stdio.h>

const char* ARTICLES_DATA_NAME = "articles";
const char* ARTICLES_DATA_EXTENSION = ".bin";

#define SIZEOF_POSITION_INFORMATION 16

#pragma pack(1)
typedef struct 
{
	char languageCode[2];				// 2 bytes
	unsigned int numberOfArticles;		// 4 bytes
	fpos_t	titlesPos;					// 8 bytes
	fpos_t	indexPos_0;					// 8 bytes
	fpos_t	indexPos_1;					// 8 bytes; the second one has discritcs removed or traditional chineses chars are converted to simpified chineses chars
	unsigned char version;				// 1 byte
	char reserved1[1];					// 1 byte
	char imageNamespace[32];			// namespace prefix for images   (without the colon)
	char templateNamespace[32];			// namespace prefix for template (without the colon)
	char reserved2[160];				// for future use
} FILEHEADER;
#pragma pack(pop)

TitleIndex::TitleIndex(string pathToDataFile)
{
	_imageNamespace = "";
	_templateNamespace = "";
	isChinese = false;

	_dataFileName = pathToDataFile;
	if ( _dataFileName.length()>0 && _dataFileName[_dataFileName.length()-1]!='/' )
		_dataFileName += '/';
	_dataFileName += ARTICLES_DATA_NAME;
	_dataFileName += ARTICLES_DATA_EXTENSION;
	
	_numberOfArticles = -1;
	
	_indexPos_0 = 0;
	_indexPos_1 = 0;

	_imageNamespace = "";
	_templateNamespace = "";
	_useManifest = false;
		
	// get the number of articles; this also checks if the files exists
	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
	{
		// second try
		_dataFileName = "";
		
		int length = pathToDataFile.length();
		if ( length && pathToDataFile[length-1]=='/' )
		{
			if ( (length>=4) && pathToDataFile[length-4]=='/' )
				_dataFileName = pathToDataFile + ARTICLES_DATA_NAME + "_" + pathToDataFile.substr(length-3, 2) + ARTICLES_DATA_EXTENSION;
		}
		else
		{
			if ( (length>=3) && pathToDataFile[length-3]=='/' )
				_dataFileName = pathToDataFile + "/" + ARTICLES_DATA_NAME + "_" + pathToDataFile.substr(length-2, 2) + ARTICLES_DATA_EXTENSION;
		}
		
		if ( _dataFileName!="" )
			f = fopen(_dataFileName.c_str(), "rb");
		if (!f)
		{
			FILE *fpManifest=fopen((pathToDataFile+"/manifest").c_str() , "r");
			if(fpManifest)
			{
				ConfigFile *c=new ConfigFile(pathToDataFile+"/manifest");
				string compressedFile=c->GetSetting("CompressedXML");
				if(f = fopen((pathToDataFile+"/"+compressedFile).c_str(),"r"))
				{
					_dataFileName=compressedFile;
					_pathToDataFile=pathToDataFile;
					_useManifest=true;
											
					_Manifest=c;
				}
			}
		}
	}
	
	if ( f && !_useManifest )
	{
		int error = 0;

		FILEHEADER fileheader;
		
		// if the old, short fileheader is used this will read over the end of the header
		// no problem, if the file is less than 256 it's unuseable anyway
		if ( fread(&fileheader, sizeof(FILEHEADER), 1, f)!=1 )
			error = 1;
		
		if ( !error ) 
		{	
			_numberOfArticles = fileheader.numberOfArticles;
			_titlesPos = fileheader.titlesPos;
			_indexPos_0 = fileheader.indexPos_0;
			
			isChinese = (tolower(fileheader.languageCode[0])=='z') && (tolower(fileheader.languageCode[1])=='h'); 
			
			if ( fileheader.version==1 )
			{
				_indexPos_1 = fileheader.indexPos_1;
				_imageNamespace = string(fileheader.imageNamespace);
				_templateNamespace = string(fileheader.templateNamespace);
			}
		}
		
		fclose(f);
	}
	else if( _useManifest && f)
	{
		if(EOF==sscanf((_Manifest->GetSetting("NumberOfArticles")).c_str(), "%d", &_numberOfArticles))
		{
			_numberOfArticles = -1;
		}
		if(EOF==sscanf((_Manifest->GetSetting("NumberOfBlocks")).c_str(), "%d", &_numberOfBlocks))
		{
			_numberOfBlocks = -1;
		}
		isChinese = (_Manifest->GetSetting("LanguageCode").compare("zh")==0);
		_imageNamespace = _Manifest->GetSetting("Image","Image");
		_templateNamespace = _Manifest->GetSetting("Template","Template");
		fclose(f);
	}
}

TitleIndex::~TitleIndex()
{
}

ArticleSearchResult* TitleIndex::FindArticle(string title, bool multiple)
{
	if ( _numberOfArticles<=0  )
		return NULL;
	if ( _useManifest )
		return FindArticle2(title, multiple);

	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
		return NULL;

	int indexNo = 0;
	
	string lowercaseTitle = CPPStringUtils::to_lower_utf8(title);
	int foundAt = -1;
	int lBound = 0;
	int uBound = _numberOfArticles - 1;
	int index = 0;	

	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;
		
		// get the title at the specific index
		string titleAtIndex = GetTitle(f, index, indexNo);
		
		// make it lowercase and skip the prefix
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
		
		if ( lowercaseTitle<titleAtIndex )
			uBound = index - 1;
		else if ( lowercaseTitle>titleAtIndex )
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
	
	// check if there are more than one articles with the same lowercase name
	int startIndex = foundAt;
	while ( startIndex>0 )
	{
		string titleAtIndex = GetTitle(f, startIndex-1, indexNo);
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
	
		if ( lowercaseTitle!=titleAtIndex )
			break;
			
		startIndex--;
	}

	int endIndex = foundAt;
	while ( endIndex<(_numberOfArticles-1) )
	{
		string titleAtIndex = GetTitle(f, endIndex+1, indexNo);
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
		
		if ( lowercaseTitle!=titleAtIndex )
			break;
		
		endIndex++;
	}
	
	if ( !multiple )
	{
		// return a result only if we have a direct hit (case is taken into account)
		
		if ( startIndex!=endIndex )
		{
			// check if one matches 100%
			for(int i=startIndex; i<=endIndex; i++)
			{		
				string titleInArchive = GetTitle(f, i, indexNo);
				if ( title==titleInArchive )
				{					
					fclose(f);
					
					return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
				}
			}
		
			// nope, multiple matches
			fclose(f);

			return NULL;
		}
		else
		{
			// return the one and only result
			string titleInArchive = GetTitle(f, foundAt, indexNo);		
			fclose(f);

			return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
		}
	}
	else
	{
		ArticleSearchResult* result = NULL;
		for(int i=startIndex; i<=endIndex; i++)
		{
			string titleInArchive = GetTitle(f, i, indexNo);
			
			if ( title==titleInArchive )
			{
				// 100% match
				DeleteSearchResult(result);
				
				fclose(f);

				return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
			}

			// collect the results
			if ( !result )
				result = new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
			else
				result->Next = new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
		}
		
		fclose(f);
		
		return result;
	}
}

void TitleIndex::DeleteSearchResult(ArticleSearchResult* articleSearchResult)
{
	while ( articleSearchResult )
	{
		ArticleSearchResult* help = articleSearchResult;
		articleSearchResult = articleSearchResult->Next;
		
		delete(help);
	}
}

bool TitleIndex::UseManifest()
{
	return _useManifest;
}

string TitleIndex::DataFileName()
{
	return _dataFileName;
}
string TitleIndex::PathToDataFile()
{
	return _pathToDataFile;
}
int TitleIndex::NumberOfArticles()
{
	return _numberOfArticles;
}

string TitleIndex::GetSuggestions(string phrase, int maxSuggestions)
{
	string suggestions = string();
	
	if ( _numberOfArticles<=0  )
		return suggestions;

	if ( _useManifest )
		return GetSuggestions2(phrase, maxSuggestions);

	int indexNo = 1;
	if ( !_indexPos_1 )
		indexNo = 0;

	
	string lowercasePhrase = PrepareSearchPhrase(phrase);
	
	int phraseLength = lowercasePhrase.length();
	if ( phraseLength==0 )
		return suggestions;
	
	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
		return suggestions;
		
	int foundAt = -1;
	int lBound = 0;
	int uBound = _numberOfArticles - 1;
	int index = 0;	
	string titleAtIndex;
	
	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;
		
		// get the title at the specific index
		titleAtIndex = GetTitle(f, index, indexNo);
		
		titleAtIndex = PrepareSearchPhrase(titleAtIndex);
		
		if ( lowercasePhrase<titleAtIndex )
			uBound = index - 1;
		else if ( lowercasePhrase>titleAtIndex )
			lBound = index + 1;
		else
		{
			foundAt = index;
			break;
		}
	}
	
	if ( foundAt<0 )
	{
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
		
		if ( lowercasePhrase>titleAtIndex )
		{
			// last one?
			if ( index==_numberOfArticles-1) 
			{
				fclose(f);
				return suggestions;
			}
			
			// no
			index++;
			titleAtIndex = GetTitle(f, index, indexNo);
			
			titleAtIndex = PrepareSearchPhrase(titleAtIndex);
						
			if ( titleAtIndex.length()>phraseLength )
				titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
			if ( lowercasePhrase!=titleAtIndex )
			{
				// still not starting with the phrase?
				fclose(f);
				return suggestions;
			}
		}
		else if ( lowercasePhrase<titleAtIndex )
		{
			// first one?
			if ( index==0 ) 
			{
				fclose(f);
				return suggestions;
			}
			
			// no
			index--;
			titleAtIndex = GetTitle(f, index, indexNo);
			titleAtIndex = PrepareSearchPhrase(titleAtIndex);
			
			if ( titleAtIndex.length()>phraseLength )
				titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
			if ( lowercasePhrase!=titleAtIndex )
			{
				// still not starting with the phrase?
				fclose(f);
				return suggestions;
			}
		}
		
		foundAt = index;
	}
	
	// go to the first article which starts with the phrase
	int startIndex = foundAt;
	while ( startIndex>0 )
	{
		string titleAtIndex = GetTitle(f, startIndex-1, indexNo);
		titleAtIndex = PrepareSearchPhrase(titleAtIndex);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
		
		if ( lowercasePhrase!=titleAtIndex )
			break;
		
		startIndex--;
	}
	
	int results = 0;
	while ( startIndex<(_numberOfArticles-1) && results<maxSuggestions )
	{
		string suggestion = GetTitle(f, startIndex, indexNo);
		titleAtIndex = PrepareSearchPhrase(suggestion);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
		if ( lowercasePhrase!=titleAtIndex )
			break;

		if ( !suggestions.empty() )
			suggestions += "\n";
		suggestions += suggestion;
		
		startIndex++;
		results++;
	}
	
	if ( (results==maxSuggestions) && (startIndex<(_numberOfArticles+1)) )
	{
		// check if the next would also meet
		startIndex++;
		
		string suggestion = GetTitle(f, startIndex, indexNo);
		titleAtIndex = PrepareSearchPhrase(suggestion);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);

		// yes, add an empty line at the end of the list
		if ( lowercasePhrase==titleAtIndex )
			suggestions += "\n";
	}

	fclose(f);

	return suggestions;
}

string TitleIndex::GetRandomArticleTitle()
{
	if ( _numberOfArticles<=0 )
		return string();
	if ( _useManifest )
		return GetRandomArticleTitle2();
	
	FILE* f = fopen(_dataFileName.c_str(), "rb");
	if ( !f )
		return string();
	
	int j = 20;
	while ( j-- )
	{
		int i = (int) ((double) random() / RAND_MAX * _numberOfArticles);
		string result = GetTitle(f, i, 0);
		
		if ( result.find(":")==string::npos )
		{
			fclose(f);
			return result;
		}
	}
	
	fclose(f);
	return string();
}

string TitleIndex::ImageNamespace()
{
	return _imageNamespace;
}

string TitleIndex::TemplateNamespace()
{
	return _templateNamespace;
}

string TitleIndex::GetTitle(FILE* f, int articleNumber, int indexNo)
{
	_lastBlockPos = 0;
	_lastArticlePos = 0;
	_lastArticleLength = 0;					  
						  
	if ( !f || articleNumber<0 || articleNumber>=_numberOfArticles  )
		return string();
	
	fpos_t indexPos = _indexPos_0;
	if ( indexNo==1 && _indexPos_1 )
		indexPos = _indexPos_1;
	
	int error = fseeko(f, indexPos + articleNumber*sizeof(int), SEEK_SET);
	if ( error )
		return string();

	fpos_t titlePos = 0;
	size_t read = fread(&titlePos, sizeof(int), 1, f);

	if ( !read )
		return string();

	error = fseeko(f, _titlesPos + titlePos, SEEK_SET);
	if ( error )
		return string();
	
	// store the article location and size for use in the future
	fread(&_lastBlockPos, sizeof(_lastBlockPos), 1, f);
	fread(&_lastArticlePos, sizeof(_lastArticlePos), 1, f);
	fread(&_lastArticleLength, sizeof(_lastArticleLength), 1, f);
	
	string result;
	unsigned char c = 0;
	while ( !feof(f) && (c=fgetc(f)) )
		result += c;
	
	return result;
}

string TitleIndex::PrepareSearchPhrase(string phrase)
{
	string lowercasePhrase = CPPStringUtils::to_lower_utf8(phrase);
	
	// do we have a different index sorting ?
	if ( _indexPos_1==0 )
		return lowercasePhrase;

	// yes
	if ( isChinese )
		lowercasePhrase = CPPStringUtils::tc2sc_utf8(lowercasePhrase);
	else
		lowercasePhrase = CPPStringUtils::exchange_diacritic_chars_utf8(lowercasePhrase);
	
	return lowercasePhrase; 
}

/* search result class */

ArticleSearchResult::ArticleSearchResult(string title, string titleInArchive, fpos_t blockPos, fpos_t articlePos, int articleLength)
{
	Next = NULL;
	
	_title = title;
	_titleInArchive = titleInArchive;
	
	_blockPos = blockPos;
	_articlePos = articlePos;
	_articleLength = articleLength;
}

string ArticleSearchResult::Title()
{
	return _title;
}

string ArticleSearchResult::TitleInArchive()
{
	return _titleInArchive;
}
									

fpos_t ArticleSearchResult::BlockPos()
{
	return _blockPos;
}

fpos_t ArticleSearchResult::ArticlePos()
{
	return _articlePos;
}

int ArticleSearchResult::ArticleLength()
{
	return _articleLength;
}




#pragma mark useManifest support

ArticleSearchResult* TitleIndex::FindArticle2(string title, bool multiple)
{
	if ( _numberOfArticles<=0  )
		return NULL;
	FILE *fpIdxRecord, *fpIdxSort, *fpBlkOffset;

	fpIdxRecord=fopen((_pathToDataFile+"/indexrecord").c_str(), "rb");
	fpIdxSort=fopen((_pathToDataFile+"/indexsort").c_str(), "rb");
	fpBlkOffset=fopen((_pathToDataFile+"/blockoffset").c_str(), "rb");
	if(!(fpIdxRecord && fpBlkOffset && fpIdxSort))
	{
		if(fpIdxRecord)
			fclose(fpIdxRecord);
		if(fpIdxSort)
			fclose(fpIdxSort);
		if(fpBlkOffset)
			fclose(fpBlkOffset);
		return NULL;
	}
	//just check, not used here.
	if(fpBlkOffset)
		fclose(fpBlkOffset);

	int indexNo = 0;
	
	string lowercaseTitle = CPPStringUtils::to_lower_utf8(title);
	int foundAt = -1;
	int lBound = 0;
	int uBound = _numberOfArticles - 1;
	int index = 0;	

	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;
		
		// get the title at the specific index
		string titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, index, indexNo);
		
		// make it lowercase and skip the prefix
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
		
		if ( lowercaseTitle<titleAtIndex )
			uBound = index - 1;
		else if ( lowercaseTitle>titleAtIndex )
			lBound = ++index;
		else
		{
			foundAt = index;
			break;
		}
	}
	
	if ( foundAt<0 )
	{
		fclose(fpIdxRecord);
		fclose(fpIdxSort);
		
		return NULL;
	}
	
	// check if there are more than one articles with the same lowercase name
	int startIndex = foundAt;
	while ( startIndex>0 )
	{
		string titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, startIndex-1, indexNo);
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
	
		if ( lowercaseTitle!=titleAtIndex )
			break;
			
		startIndex--;
	}

	int endIndex = foundAt;
	while ( endIndex<(_numberOfArticles-1) )
	{
		string titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, endIndex+1, indexNo);
		titleAtIndex = CPPStringUtils::to_lower_utf8(titleAtIndex);
		
		if ( lowercaseTitle!=titleAtIndex )
			break;
		
		endIndex++;
	}
	
	if ( !multiple )
	{
		// return a result only if we have a direct hit (case is taken into account)
		
		if ( startIndex!=endIndex )
		{
			// check if one matches 100%
			for(int i=startIndex; i<=endIndex; i++)
			{		
				string titleInArchive = GetTitle2(fpIdxRecord, fpIdxSort, i, indexNo);
				if ( title==titleInArchive )
				{					
					fclose(fpIdxRecord);
					fclose(fpIdxSort);
					
					return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
				}
			}
		
			// nope, multiple matches
			fclose(fpIdxRecord);
			fclose(fpIdxSort);

			return NULL;
		}
		else
		{
			// return the one and only result
			string titleInArchive = GetTitle2(fpIdxRecord, fpIdxSort, foundAt, indexNo);		
			fclose(fpIdxRecord);
			fclose(fpIdxSort);

			return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
		}
	}
	else
	{
		ArticleSearchResult* result = NULL;
		for(int i=startIndex; i<=endIndex; i++)
		{
			string titleInArchive = GetTitle2(fpIdxRecord, fpIdxSort, i, indexNo);
			
			if ( title==titleInArchive )
			{
				// 100% match
				DeleteSearchResult(result);
				
				fclose(fpIdxRecord);
				fclose(fpIdxSort);

				return new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
			}

			// collect the results
			if ( !result )
				result = new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
			else
				result->Next = new ArticleSearchResult(title, titleInArchive, _lastBlockPos, _lastArticlePos, _lastArticleLength);
		}
		
		fclose(fpIdxRecord);
		fclose(fpIdxSort);
		
		return result;
	}
}

string TitleIndex::GetTitle2(FILE* fpIdxRecord, FILE* fpIdxSort, int articleNumber, int indexNo)
{
	_lastBlockNumber = 0;
	_lastBlockPos = 0;
	_lastArticlePos = 0;
	_lastArticleLength = 0;					  
						  
	if ( articleNumber<0 || articleNumber>=_numberOfArticles  )
		return string();
	
	int error = fseeko(fpIdxSort, articleNumber*sizeof(size_t), SEEK_SET);
	if ( error )
		return string();

	off_t o1=0;
	size_t titlePos = 0;
	size_t read = fread(&o1, sizeof(size_t), 1, fpIdxSort);
	titlePos+=o1;

	if ( !read )
		return string();

	error = fseeko(fpIdxRecord, titlePos, SEEK_SET);
	if ( error )
		return string();
	
	// store the article location and size for use in the future
	unsigned long int uli1=0;
	unsigned int ui1=0,ui2=0,ui3=0;
	fread(&uli1, sizeof(unsigned long int), 1, fpIdxRecord);
	_lastBlockNumber+=uli1;
	_lastBlockPos=(_lastBlockNumber)*sizeof(off_t)*2;
	fread(&ui1, sizeof(unsigned int), 1, fpIdxRecord);
	_lastArticlePos+=ui1;
	fread(&ui2, sizeof(unsigned int), 1, fpIdxRecord);
	_lastArticleLength+=ui2;
	unsigned int name_len;
	fread(&name_len, sizeof(unsigned int), 1, fpIdxRecord);
	
	string result;
	unsigned char c = 0;
	int i;
	for(i=0;i<name_len;i++)
	{
		c=fgetc(fpIdxRecord);
		result += c;
	}
	
	return result;
}

string TitleIndex::GetSuggestions2(string phrase, int maxSuggestions)
{
	string suggestions = string();
	
	if ( _numberOfArticles<=0  )
		return suggestions;

	int indexNo = 1;
	if ( !_indexPos_1 )
		indexNo = 0;

	
	string lowercasePhrase = PrepareSearchPhrase(phrase);
	
	int phraseLength = lowercasePhrase.length();
	if ( phraseLength==0 )
		return suggestions;

	FILE *fpIdxRecord, *fpIdxSort, *fpBlkOffset;

	fpIdxRecord=fopen((_pathToDataFile+"/indexrecord").c_str(), "rb");
	fpIdxSort=fopen((_pathToDataFile+"/indexsort").c_str(), "rb");
	fpBlkOffset=fopen((_pathToDataFile+"/blockoffset").c_str(), "rb");
	if(!(fpIdxRecord && fpBlkOffset && fpIdxSort))
	{
		if(fpIdxRecord)
			fclose(fpIdxRecord);
		if(fpIdxSort)
			fclose(fpIdxSort);
		if(fpBlkOffset)
			fclose(fpBlkOffset);
		return string();
	}
	if(fpBlkOffset)
		fclose(fpBlkOffset);


	//FILE* f = fopen(_dataFileName.c_str(), "rb");
	//if ( !f )
	//	return suggestions;
		
	int foundAt = -1;
	int lBound = 0;
	int uBound = _numberOfArticles - 1;
	int index = 0;	
	string titleAtIndex;
	
	while ( lBound<=uBound )
	{	
		index = (lBound + uBound) >> 1;
		// get the title at the specific index
		titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, index, indexNo);
		titleAtIndex = PrepareSearchPhrase(titleAtIndex);
		
		if ( lowercasePhrase<titleAtIndex )
			uBound = index - 1;
		else if ( lowercasePhrase>titleAtIndex )
			lBound = index + 1;
		else
		{
			foundAt = index;
			break;
		}
	}
	
	if ( foundAt<0 )
	{
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
		
		if ( lowercasePhrase>titleAtIndex )
		{
			// last one?
			if ( index==_numberOfArticles-1) 
			{
				fclose(fpIdxRecord);
				fclose(fpIdxSort);
				return suggestions;
			}
			
			// no
			index++;
			titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, index, indexNo);
			
			titleAtIndex = PrepareSearchPhrase(titleAtIndex);
						
			if ( titleAtIndex.length()>phraseLength )
				titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
			if ( lowercasePhrase!=titleAtIndex )
			{
				// still not starting with the phrase?
				fclose(fpIdxRecord);
				fclose(fpIdxSort);
				return suggestions;
			}
		}
		else if ( lowercasePhrase<titleAtIndex )
		{
			// first one?
			if ( index==0 ) 
			{
				fclose(fpIdxRecord);
				fclose(fpIdxSort);
				return suggestions;
			}
			
			// no
			index--;
			titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, index, indexNo);
			titleAtIndex = PrepareSearchPhrase(titleAtIndex);
			
			if ( titleAtIndex.length()>phraseLength )
				titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
			if ( lowercasePhrase!=titleAtIndex )
			{
				// still not starting with the phrase?
				fclose(fpIdxRecord);
				fclose(fpIdxSort);
				return suggestions;
			}
		}
		
		foundAt = index;
	}
	
	// go to the first article which starts with the phrase
	int startIndex = foundAt;
	while ( startIndex>0 )
	{
		string titleAtIndex = GetTitle2(fpIdxRecord, fpIdxSort, startIndex-1, indexNo);
		titleAtIndex = PrepareSearchPhrase(titleAtIndex);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
		
		if ( lowercasePhrase!=titleAtIndex )
			break;
		
		startIndex--;
	}
	
	int results = 0;
	while ( startIndex<(_numberOfArticles-1) && results<maxSuggestions )
	{
		string suggestion = GetTitle2(fpIdxRecord, fpIdxSort, startIndex, indexNo);
		titleAtIndex = PrepareSearchPhrase(suggestion);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);
			
		if ( lowercasePhrase!=titleAtIndex )
			break;

		if ( !suggestions.empty() )
			suggestions += "\n";
		suggestions += suggestion;
		
		startIndex++;
		results++;
	}
	
	if ( (results==maxSuggestions) && (startIndex<(_numberOfArticles+1)) )
	{
		// check if the next would also meet
		startIndex++;
		
		string suggestion = GetTitle2(fpIdxRecord, fpIdxSort, startIndex, indexNo);
		titleAtIndex = PrepareSearchPhrase(suggestion);
		
		if ( titleAtIndex.length()>phraseLength )
			titleAtIndex = titleAtIndex.substr(0, phraseLength);

		// yes, add an empty line at the end of the list
		if ( lowercasePhrase==titleAtIndex )
			suggestions += "\n";
	}

	fclose(fpIdxRecord);
	fclose(fpIdxSort);

	return suggestions;
}

string TitleIndex::GetRandomArticleTitle2()
{
	if ( _numberOfArticles<=0 )
		return string();
	
	FILE *fpIdxRecord, *fpIdxSort, *fpBlkOffset;

	fpIdxRecord=fopen((_pathToDataFile+"/indexrecord").c_str(), "rb");
	fpIdxSort=fopen((_pathToDataFile+"/indexsort").c_str(), "rb");
	fpBlkOffset=fopen((_pathToDataFile+"/blockoffset").c_str(), "rb");
	if(!(fpIdxRecord && fpBlkOffset && fpIdxSort))
	{
		if(fpIdxRecord)
			fclose(fpIdxRecord);
		if(fpIdxSort)
			fclose(fpIdxSort);
		if(fpBlkOffset)
			fclose(fpBlkOffset);
		return string();
	}
	if(fpBlkOffset)
		fclose(fpBlkOffset);

	
	int j = 20;
	while ( j-- )
	{
		int i = (int) ((double) random() / RAND_MAX * _numberOfArticles);
		string result = GetTitle2(fpIdxRecord, fpIdxSort, i, 0);
		
		if ( result.find(":")==string::npos )
		{
			fclose(fpIdxSort);
			fclose(fpIdxRecord);
			return result;
		}
	}
	
	fclose(fpIdxSort);
	fclose(fpIdxRecord);
	return string();
}


