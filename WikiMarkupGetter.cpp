/*
 *  WikiMarkupGetter.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <wchar.h>
#include <bzlib.h>

#include "Settings.h"
#include "CPPStringUtils.h"
#include "StopWatch.h"

#include "WikiMarkupGetter.h"

#define BUFFER_SIZE 32767

WikiMarkupGetter::WikiMarkupGetter(string language_code) 
{
	_languageCode = string(language_code);
}

WikiMarkupGetter::~WikiMarkupGetter() 
{
}

wstring WikiMarkupGetter::GetMarkupForArticle(const wstring articleName) 
{
	return GetMarkupForArticle(CPPStringUtils::to_utf8(articleName));
}

wstring WikiMarkupGetter::GetMarkupForArticle(const string utf8ArticleName)
{
	TitleIndex* titleIndex = settings.GetTitleIndex(_languageCode);
	if ( !titleIndex )
		return wstring();
	
	ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(utf8ArticleName);
	
	if ( !articleSearchResult )
		return wstring();
	
	wstring result = GetMarkupForArticle(articleSearchResult);
	titleIndex->DeleteSearchResult(articleSearchResult);
	
	return result;
}

wstring WikiMarkupGetter::GetMarkupForArticle(ArticleSearchResult* articleSearchResult)
{
	if ( !articleSearchResult )
		return wstring();
	
	fpos_t blockPos = articleSearchResult->BlockPos(); 
	int articlePos = articleSearchResult->ArticlePos();
	int articleLength = articleSearchResult->ArticleLength();
	
	_lastArticleTitle = string(articleSearchResult->TitleInArchive());

	TitleIndex* titleIndex = settings.GetTitleIndex(_languageCode);
	string filename = titleIndex->DataFileName();	
	FILE* f = fopen(filename.c_str(), "rb");
	if ( !f )
		return wstring();
					
	// seek to the block
	fseeko(f, blockPos, SEEK_SET);
		
	// open the block
	int bzerror;
	BZFILE *bzf = BZ2_bzReadOpen(&bzerror, f, 0, 0, NULL, 0);
	
	char* text = (char*) malloc(articleLength+1);
	char buffer[BUFFER_SIZE];
	int read;
	while ( (read=BZ2_bzRead(&bzerror, bzf, buffer, BUFFER_SIZE)) )
	{
		if ( articlePos-read<0 )
		{
			char* start = buffer + articlePos;
			int len = (read-articlePos);
			if ( len>articleLength )
				len = articleLength;
			
			char* pText = text;
			strncpy(pText, start, len);
			articleLength -= len;
			pText += len;
			
			while ( articleLength && (read=BZ2_bzRead(&bzerror, bzf, buffer, BUFFER_SIZE)) )
			{
				if ( articleLength>read )
					len = read;
				else
					len = articleLength;
				
				strncpy(pText, buffer, len);
				articleLength -= len;
				pText += len;
			}
			
			*pText = 0x0;
			break;
		}
		else
			articlePos -= read;
	}
	
	BZ2_bzReadClose(&bzerror, bzf);
	fclose(f);
	
	wstring content = CPPStringUtils::from_utf8w(text);
	free(text);
	
	return content;
}

string WikiMarkupGetter::GetLastArticleTitle()
{
	return _lastArticleTitle;
}

wstring WikiMarkupGetter::GetTemplate(const wstring templateName, string templatePrefix)
{
	return GetTemplate(CPPStringUtils::to_utf8(templateName), templatePrefix);
}

wstring WikiMarkupGetter::GetTemplate(const string utf8TemplateName, string templatePrefix)
{	
	// remove "_" and exchange them with spaces
	string templateName = utf8TemplateName;
	
	size_t pos = 0;
	while ( (pos=templateName.find("_"))!=string::npos )
		templateName.replace(pos, 1, " ", 1);
	
	// check the "en:convert/" template
	// remove trailing slashes (a common error)
	/*
	while ( templateName.length()>0 && templateName[templateName.length()-1]=='/' )
		templateName = templateName.substr(0, templateName.length()-1);
	*/
	string filename = CPPStringUtils::to_lower(CPPStringUtils::url_encode(templateName));
	while ( (pos=filename.find("/"))!=string::npos )
		filename.replace(pos, 1, "%2f", 3);

	while ( (pos=filename.find("\\"))!=string::npos )
		filename.replace(pos, 1, "%5c", 3);

	while ( (pos=filename.find(":"))!=string::npos )
		filename.replace(pos, 1, "%3a", 3);

	// if the filename is empty, caching is disabled
	if ( true )
		filename = settings.Path() + _languageCode + "/cache/" + filename;
	else
		filename = "";
	
	if ( !filename.empty() ) 
	{
		// try to get the template fron the cache
		FILE* f = fopen(filename.c_str(), "rb");
		if ( f ) 
		{
			int error = fseek(f, 0, SEEK_END);
			fpos_t size;
			
			if ( !error )
				error = fgetpos(f, &size);

			if ( !error )
				error = fseek(f, 0, SEEK_SET);
			
			int read = 0;
			char* buffer = NULL;
			
			if ( !error && size>0 ) {
				buffer = (char*) malloc(size);
				read = fread(buffer, 1, size, f);
			}
				
			fclose(f);
			
			if ( read ) 
			{
				string result = string(buffer);
				free(buffer);
				return CPPStringUtils::from_utf8w(result);
			}
			
			if ( buffer )
				free(buffer);
		}	
	}
	
	wstring text = wstring();
	
	// test for some "build in" templates
	if ( templateName=="tl" )
	{
		text = L"{{[[Template:{{{1}}}|{{{1}}}]]}}";
	}
	else 
	{
		if ( templateName.find(":")==string::npos )
			templateName = templatePrefix + templateName;
		else if ( templateName[0]==':' )
		{
			// sort of an include of another article, a template without a namespace
			templateName = templateName.substr(1);
		}
				 
		TitleIndex* titleIndex = settings.GetTitleIndex(_languageCode);
		
		// we're looking for templates; if there are more than one take it; maybe we're redirected
		ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(templateName, true);

		if ( !articleSearchResult )
			return wstring(L"-");
		
		templateName = articleSearchResult->Title();
		if ( templateName!=articleSearchResult->TitleInArchive() )
			templateName = articleSearchResult->TitleInArchive();
		
		titleIndex->DeleteSearchResult(articleSearchResult);
			
		wstring wikiTemplate = GetMarkupForArticle(templateName);
		if ( wikiTemplate.empty() )
			return wstring(L"-");
		
		if ( wikiTemplate.length()<200 )
		{
			// for speed reason, make no sense to scan a 1 MB articles
			wstring lowercaseArticle = CPPStringUtils::to_lower(wikiTemplate);
			
			size_t pos = 0;
			if ( (pos=lowercaseArticle.find(L"#redirect"))!=string::npos )
			{
				string newUtf8ArticleName = CPPStringUtils::to_utf8(wikiTemplate.substr(pos + 9)); 
				while ( newUtf8ArticleName.length()>0 && newUtf8ArticleName[0]!='[' )
					newUtf8ArticleName = newUtf8ArticleName.substr(1);

				while ( newUtf8ArticleName.length()>0 && newUtf8ArticleName[0]=='[' )
					newUtf8ArticleName = newUtf8ArticleName.substr(1);
				
				pos = newUtf8ArticleName.find("]]");
				if ( pos!=string::npos )
				{
					newUtf8ArticleName = newUtf8ArticleName.substr(0, pos);

					while ( (pos=newUtf8ArticleName.find("_"))!=string::npos )
						newUtf8ArticleName.replace(pos, 1, " ", 1);
					
					wikiTemplate =  GetMarkupForArticle(newUtf8ArticleName);
				}
			}
		}
		
		size_t pos;
		if ( (pos=wikiTemplate.find(L"<onlyinclude>"))!=string::npos )
		{
			wikiTemplate = wikiTemplate.substr(pos + 13);
			
			pos = wikiTemplate.find(L"</onlyinclude>");
			if ( pos!=string::npos )
				text = wikiTemplate.substr(0, pos);
		}
		else
		{
			wstring tag = wstring();
			bool endTag = false;
			
			int noinclude = 0;
			// int comment = 0;
			
			int state = 0;
			int length = wikiTemplate.length();
			
			for (int i=0; i<length; i++)
			{
				wchar_t c = wikiTemplate[i];
				switch (state)
				{
					case 0:
						// Inside text
						if ( c=='<' ) 
						{
							tag = wstring();
							endTag = false;
							state = 1;
						}
						else if (!noinclude) 
						{
							text += c;
						}
						break;
						
					case 1:
						// Inside a tag
						if ( c=='/' && tag.empty() ) 
						{
							endTag = true;
						}
						else if ( c=='>' )
						{
							// end of a tag
							if ( !endTag )
							{
								if ( tag==L"noinclude" )
									noinclude++;
								else if ( tag!=L"includeonly" ) 
								{
									if ( !noinclude )
										text += L"<" + tag + L">";
								}
							}
							else
							{
								if ( tag==L"noinclude" )
									noinclude--;
								else if ( tag!=L"includeonly" ) 
								{
									if ( !noinclude )
										text += L"</" + tag + L">";
								}
							}
							state = 0;
						}
						else
							tag += c;
						break;
				}
			}
		}
	}
	
	// try to fix some bugs in the language
	/*
	while ( (pos=text.find(L"{{{!}} ", pos))!=string::npos )
		text.replace(pos+2, 1, L"(", 1);

	while ( (pos=text.find(L"{{!}}} ", pos))!=string::npos )
		text.replace(pos+3, 1, L")", 1);
	*/
	if ( !filename.empty() ) 
	{
		// try to store the text
		FILE* f = fopen(filename.c_str(), "wb");
		if ( f ) 
		{
			string data = CPPStringUtils::to_utf8(text);
			fwrite(data.c_str(), 1, data.length(), f);
			
			char c = 0;
			fwrite(&c, 1, 1, f);
		
			fclose(f);
		}	
	}
	
	return text;
}


