/*
 *  WikiParser.cpp
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
#include <string.h>
#include <memory.h>
#include <map>

#include "Settings.h"
#include "WikiMarkupParser.h"
#include "WikiMarkupGetter.h"
#include "CPPStringUtils.h"
#include "StopWatch.h"
#include "StringUtils.h"
#include "ConfigFile.h"

//#ifdef MATH_SUPPORT
#include <CommonCrypto/CommonDigest.h>
//#endif

#define OUTPUT_GROWS	8192

#define DEBUG false
#define RECURSION_LIMIT 64

const wchar_t* wikiTags[] = {L"unused", L"nowiki", L"pre", L"source", L"imagemap", L"code", L"ref", L"references", L"math", 0x0};
const wchar_t* nsNames[]  = {L"unused", L"Talk", L"User", L"User_talk", L"Wikipedia", L"Wikipedia_talk", L"Image", L"Image_talk",
						     L"MediaWiki", L"MediaWiki_talk", L"Template", L"Template_talk", L"Help", L"Help_talk", L"Category", L"Category_talk", 0x0};

const wchar_t* ignoredTemplates[] = {L"commons", 0x0};

typedef struct tagTEMPLATEPARAM
{
	wstring position;
	wstring name;
	wstring value;
	tagTEMPLATEPARAM* next;
} TEMPLATEPARAM;

typedef struct tagTOC
{
	wchar_t* name;
	int level;
	
	tagTOC*	next;
} TOC;

typedef struct tagREF
{
	const	wchar_t* start;
	int		length;
	tagREF*	next;
} REF;

WikiMarkupParser::WikiMarkupParser(const wchar_t* languageCode, const wchar_t* pageName, bool doExpandTemplates) 
{
	_languageCodeW = (wchar_t*) malloc((wcslen(languageCode)+1) * sizeof(wchar_t));
	wcscpy((wchar_t*) _languageCodeW, languageCode);
		
	_pInput = NULL;
	_pCurrentInput = NULL;
	
	_pOutput = NULL;
	_pCurrentOutput = NULL;
	_iOutputSize = 0;
	_iOutputRemain = 0;
	
	_doExpandTemplates = doExpandTemplates;
	
	_newLine = 0;
	
	_orderedEnumeration = 0;
	_unorderedEnumeration = 0;
	_definitionList = 0;

	_italic = -1;
	_bold = -1;
	
	_externalLinkNo = 0;
	_recursionCount = 0;
	
	_stop = false;

	_pCurrentTag = NULL;
	
	_tocPosition = -1;
	
	_toc = NULL;
	
	_references = NULL;
	
	_categories = NULL;
	
	_pageName = pageName;
		
	string lc = CPPStringUtils::to_string(_languageCodeW);
	_titleIndex = settings.GetTitleIndex(lc);
	_languageConfig = settings.LanguageConfig(lc);	
	
	string imageNamespace = string();
	if ( _titleIndex )
		imageNamespace = CPPStringUtils::to_lower(_titleIndex->ImageNamespace());
	if ( imageNamespace.empty() ) 
		imageNamespace = CPPStringUtils::to_lower(_languageConfig->GetSetting("imagePrefix", "image"));
	if ( imageNamespace.empty() )
		imageNamespace = "image";
												
	_imageNamespace = wstrdup(CPPStringUtils::to_wstring(imageNamespace).c_str());
	
	_imagesInstalled = settings.AreImagesInstalled(lc);
}

WikiMarkupParser::~WikiMarkupParser()
{
	if ( _pInput!=NULL ) 
	{
		free(_pInput);
		_pInput = NULL;
	}

	if ( _pOutput!=NULL ) 
	{
		free(_pOutput);
		_pOutput = NULL;
	}	
	
	while ( _pCurrentTag )
	{
		tagType *oldTag = _pCurrentTag;
		_pCurrentTag = oldTag->pPrevious;
		if ( _pCurrentTag!=NULL ) 
			_pCurrentTag->pNext = NULL;
	
		free(oldTag->name);
		delete(oldTag);
	}

	while ( _toc )
	{
		TOC* toc = (TOC*) _toc;
		_toc = toc->next;

		if ( toc->name )
			free(toc->name);
	
		delete(toc);
	}	
	
	while ( _references )
	{
		REF* ref = (REF*) _references;
		_references = ref->next;
		
		delete(ref);
	}	
	
	if ( _categories )
	{
		free(_categories);
		_categories = NULL;
	}
	
	if ( _languageCodeW )
	{
		free((wchar_t*) _languageCodeW);
		_languageCodeW = NULL;
	}
	
	if ( _imageNamespace )
	{
		free(_imageNamespace);
		_imageNamespace = NULL;
	}
}

void WikiMarkupParser::SetInput(const wchar_t* pInput) 
{
	if ( pInput==NULL )
		return;
		
	if ( _pInput!=NULL )
		free(_pInput);
		
	_pInput = (wchar_t*) malloc( (wcslen(pInput)+1) * sizeof(wchar_t) );
	
	const wchar_t* pSrc = pInput;
	wchar_t* pDest = _pInput;	
	while (*pSrc!=0x0) {
		if ( *pSrc=='\r' ) {
			if ( *(pSrc+1)=='\n' ) 
				*pDest++ = *pSrc;
			else
				*pDest++ = '\n';
		}
		else
			*pDest++ = *pSrc;
			
		pSrc++;
	}
	*pDest = 0x0;
	
	_inputLength = wcslen(_pInput);
		
	if ( _pOutput!=NULL )
		free(_pOutput);
		
	_pCurrentOutput = _pOutput;
	_iOutputRemain = _iOutputSize;
}

const wchar_t* WikiMarkupParser::GetOutput() 
{
	if ( _pOutput==NULL ) 
	{
		_iOutputSize = OUTPUT_GROWS;
		_pOutput = (wchar_t*) malloc((_iOutputSize+1) *sizeof(wchar_t));
			
		_pCurrentOutput = _pOutput;
		_iOutputRemain = _iOutputSize;
		*_pCurrentOutput = 0x0;
	}
	
	return _pOutput;
}

double WikiMarkupParser::EvaluateExpression(const wchar_t* expression)
{
	if ( !expression || !*expression )
		return 0;
	
	return 0;
	
	/*
	char buffer[8 + wcslen(expression) + 1 + 1];
	strcpy(buffer, "result=(");
	char* help = buffer + 8;
	while ( *expression )
		*help++ = tolower((char) *expression++);
	*help++ = ')';
	*help = 0x0;
	
	char* pos;
	while ( (pos=strstr(buffer, "mod")) )
	{
		*pos++ = ' ';
		*pos++ = '%';
		*pos = ' ';
	}
	
	int error = evaluateExpression(buffer);
	if ( error )
		return 0;
	
	for (int i = 0; i < nVariables; i++) 
	{
		if ( !strcmp(variable[i].name, "result") )
			return variable[i].value;
	}
	
	return 0;
	 */
}

void WikiMarkupParser::ReplaceInput(const wchar_t* text, int position, int length) 
{	
	if ( position<0 || length<0 )
		return;
	
	if ( position>_inputLength || (position+length)>_inputLength )
		return;
	
	int text_length = 0;
	if ( text )
		text_length = wcslen(text);
	
	int new_length = _inputLength - length + text_length;
	
	wchar_t* new_input = (wchar_t*) malloc((new_length+1) * sizeof(wchar_t));
	wcsncpy(new_input, _pInput, position);
	new_input[position] = 0x0;
	if ( text )
		wcscat(new_input, text);
	wcscat(new_input, _pInput + position + length);

	free(_pInput);
	
	_pInput = new_input;
	_pCurrentInput = _pInput + position;
	_inputLength = new_length;
}

inline wchar_t WikiMarkupParser::GetNextChar() 
{
	if ( *_pCurrentInput==0x00 )
		return 0x00;
		
	return *_pCurrentInput++;
}

inline wchar_t WikiMarkupParser::Peek() 
{
	return *_pCurrentInput;
}

inline wchar_t WikiMarkupParser::Peek(int count) 
{
	if ( ((_pCurrentInput-_pInput) + count)>_inputLength )
		return 0x0;
	else
		return *(_pCurrentInput + count);
}

inline void WikiMarkupParser::Eat(int count) 
{
	if ( count<0 )
		return;
		
	if ( ((_pCurrentInput-_pInput) + count)>=_inputLength )
		_pCurrentInput = _pInput + wcslen(_pInput);
	else
		_pCurrentInput += count;
}

wchar_t* WikiMarkupParser::RemoveComments(const wchar_t* src)
{
	if ( !src )
		return NULL;
	
	int srcLength = wcslen(src);
	const wchar_t* srcCurrent = src;
	
	wchar_t* dst = (wchar_t*) malloc((srcLength+1)*sizeof(wchar_t));
	wchar_t* dstCurrent = dst;
	
	int state = 0;
	wchar_t c;
	
	bool changed = false;
	
	while ( (c=*srcCurrent++) )
	{
		switch ( state )
		{
			case 0:
				// in text, currently only commets will be filtered out, not tags!
				if ( c=='<' ) 
				{
					if ( *srcCurrent=='!' && ((srcCurrent-src + 2)<srcLength) && srcCurrent[1]=='-' && srcCurrent[2]=='-' ) 
					{
						// this is a comment
						changed = true;
						
						state = 1;
						break;
					}
				}
				
				*dstCurrent++ = c;
				break;
				
			case 1:
				// inside a comment
				if ( c=='-' && *srcCurrent=='-' &&  srcCurrent[1]=='>' )
				{
					srcCurrent += 2;
					
					// end of comment
					state = 0;
				}
				break;
		}
	}
	*dstCurrent = 0x0;

	if ( !changed )
	{
		free(dst);
		return (wchar_t*) src;
	}
	else
		return dst;
}

wchar_t* WikiMarkupParser::ExpandTemplates(const wchar_t* src)
{
	if (_recursionCount>RECURSION_LIMIT)
		return NULL;
	if ( !src )
		return NULL;
	_recursionCount++;
	
//	if ( DEBUG )
//	wprintf(L"---\r\n%S\r\b---", src);	

	wchar_t* commentsRemoved = RemoveComments(src);
	if ( src!=commentsRemoved )
		src = commentsRemoved;
	else
		commentsRemoved = NULL;
	//strip it!
	
	bool handledOne = false;
	bool stripFlag = false;

	const wchar_t *src0=src;
	wchar_t *src1;
	map<wstring, wstring> stripMap;
	wstring strippedSrc=strip(wstring(src),&stripMap);
	int strippedLen=strippedSrc.length();
	int srcLength;
	const wchar_t* srcCurrent;
	const wchar_t * srcPos;
	if(stripMap.size()>0 || wcslen(src)<strippedLen)
	{
		//something stripped out!
		stripFlag=true;		
		srcPos=(const wchar_t *)malloc(sizeof(wchar_t)*(strippedLen+1));
		src1=(wchar_t*)srcPos;
		int count=0;
		for(;count<strippedLen;count++)
			src1[count]=strippedSrc[count];
		src1[count]=L'\0';
		srcCurrent = srcPos;
		srcLength = strippedLen;
	}else
	{
		
		//const wchar_t* srcPos = src;
		srcPos=src;
		//const wchar_t* srcCurrent = src;
		srcCurrent = src;
		srcLength = wcslen(src);
	}

	int dstSize = 0;
	int dstLength = srcLength;
	wchar_t* dst = (wchar_t*) malloc((dstLength+1)*sizeof(wchar_t));
	wchar_t* dstPos = dst;
	*dstPos = 0x0;
	
	const wchar_t* startOfTagName = NULL;
	const wchar_t* endOfTagName = NULL;
		
	int endTag = 0;
	
	int state = 0;
	wchar_t c;
	
	const wchar_t* nowiki = NULL;
	const wchar_t* pre = NULL;
	const wchar_t* source = NULL;
	
	bool insideSpecialTags = false;
	
	
	while ( (c=*srcCurrent++) )
	{

#if 0 //these should have been stripped out
		switch ( state )
		{
			case 0:
				// in text
				if ( c=='<' ) 
				{					 
					// this is a tag
					startOfTagName = _pCurrentInput;
					endOfTagName = NULL;
					 
					endTag = 0;
					if ( *srcCurrent=='/' ) 
					{
						endTag = 1;
						*srcCurrent++;
					}
					startOfTagName = srcCurrent;
					 
					state = 1;
				}
				break;
				
			case 1:
				// inside a tag
				if ( c=='>' || c==' ' ) 
				{
					if ( !endOfTagName )
					{
						endOfTagName = srcCurrent - 1;
					
						int length = endOfTagName - startOfTagName;
						if ( length>0 )
						{
							wchar_t tag[length+1];
							wcsncpy(tag, startOfTagName, length);
							tag[length] = 0x0;
							
							if ( !endTag && !insideSpecialTags )
							{
								// only do this if the tags ar not set nor this is an endtag
								if ( !wcscmp(tag, L"nowiki") ) 
								{
									nowiki = srcCurrent;
									insideSpecialTags = true;
								}
								else if ( !wcscmp(tag, L"pre") )
								{
									pre = srcCurrent;
									insideSpecialTags = true;
								}
								else if ( !wcscmp(tag, L"source") )
								{
									source = srcCurrent;
									insideSpecialTags = true;
								}
							} // no end tag and no special one
							else if ( endTag ) 
							{
								if ( insideSpecialTags )
								{
									if ( nowiki && !wcscmp(tag, L"nowiki") )
									{
										nowiki = NULL;
										insideSpecialTags = false;
									}
									else if ( pre && !wcscmp(tag, L"pre") )
									{
										pre = NULL;
										insideSpecialTags = false;
									}
									else if ( source && !wcscmp(tag, L"source") )
									{
										source = NULL;
										insideSpecialTags = false;
									}
								}
							} // end tag
						} // length > 0
					} // end of tag name encoutered
					
					if ( c==L'>' )
						state = 0;
				} // c=='>' || c==' '
				break;
		} // switch (c)

#endif
		
		if ( c=='{' && !insideSpecialTags ) 
		{
			// is the next char also a {
			if ( *srcCurrent=='{' )
			{
				// add everything we have to far
				int size = (srcCurrent-srcPos - 1); 
				if ( size>0 )
				{
					int newDstSize = dstSize + size;
					if ( newDstSize>dstLength )
					{
						if(stripFlag)
						dstLength += ( size + (srcLength - (srcCurrent - src1) ) + 256);
						else
						dstLength += ( size + (srcLength - (srcCurrent - src) ) + 256);
						dst = (wchar_t*) realloc(dst, (dstLength+1)*sizeof(wchar_t));
						
						dstPos = dst + dstSize;
					}
					
					wcsncpy(dstPos, srcPos, size);
					dstPos += size;
					*dstPos = 0x0;
					
					dstSize += size;
					
					srcPos = srcCurrent;
				}
				
				// look for the end of the template
				
				// this is the second {
				srcCurrent++;
				srcPos = srcCurrent;
				
				int brakedCount = 2;
				while ( *srcCurrent )
				{
					if ( *srcCurrent=='{' )
						brakedCount++;
					else if ( *srcCurrent=='}' )
					{
						brakedCount--;
						if ( !brakedCount )
						{
							srcCurrent++;
							break;
						}
					}
					
					srcCurrent++;
				}
				
				// forget it, the text ends before the template
				if ( brakedCount )
					break;
				
				if ( settings.ExpandTemplates() )
				{
					int templateLength = srcCurrent - srcPos - 2; // the two trailing } are ignored
					
					wchar_t* templateText = (wchar_t*) malloc((templateLength+1)*sizeof(wchar_t));
					if ( templateLength>0 )
						wcsncpy(templateText, srcPos, templateLength);
					templateText[templateLength] = 0x0;
					
					srcPos = srcCurrent;
			
					if ( DEBUG )
						wprintf(L"\r\nTemplate:\r\n%S\r\n", templateText);
									
					// do something with the template here
					if ( templateLength )
					{
						wchar_t* expandedTemplate = ExpandTemplate(templateText);
						if ( expandedTemplate )
						{
							if ( DEBUG )
								wprintf(L"\r\nExpanded Template:\r\n%S\r\n", expandedTemplate);
							
							int size = wcslen(expandedTemplate); 
							if ( size>4 )
							{
								wchar_t* help = ExpandTemplates(expandedTemplate);
								while ( help!=expandedTemplate )
								{
									free(expandedTemplate);
									expandedTemplate = help;
									
									if ( expandedTemplate )
										help = ExpandTemplates(expandedTemplate);
								}
								
								size = wcslen(expandedTemplate); 
							}
													
							if ( size>0 )
							{									
								if ( DEBUG )
									wprintf(L"\r\nExpanded Template:\r\n%S\r\n", expandedTemplate);
								
								int newDstSize = dstSize + size;
								if ( newDstSize>dstLength )
								{
									// add the current template size, the remaining bytes and 256
									if(stripFlag)
									dstLength += (size + (srcLength - (srcCurrent - src1) ) + 256);
									else
									dstLength += (size + (srcLength - (srcCurrent - src) ) + 256);
									dst = (wchar_t*) realloc(dst, (dstLength+1)*sizeof(wchar_t));
									
									dstPos = dst + dstSize;
								}
								
								wcsncpy(dstPos, expandedTemplate, size);
								dstPos += size;
								*dstPos = 0x0;
								
								dstSize += size;
							}
							
							free(expandedTemplate);
						}
					}
					
					handledOne = true;
					free(templateText);
				}
				else 
				{
					srcPos = srcCurrent;
					handledOne = true;
				}
				
			} // second char is {
		} // first char is {
		
	} // while
	
	if ( handledOne )
	{
		// add everything what is left now (if necessary)
		int size = (srcCurrent-srcPos - 1); 
		if ( size>0 )
		{
			int newDstSize = dstSize + size;
			if ( newDstSize>dstLength )
			{
				dstLength = newDstSize;
				dst = (wchar_t*) realloc(dst, (dstLength+1)*sizeof(wchar_t));
				
				dstPos = dst + dstSize;
			}
			
			wcsncpy(dstPos, srcPos, size);
			dstPos += size;
			*dstPos = 0x0;
			
			dstSize += size;
			
			srcPos = srcCurrent;		
		}
		else
			*dstPos = 0x0;
		//unstrip
		if(stripFlag){
			wstring unstrippedDst = unstrip1 (wstring(dst), stripMap);
			int unstrippedLen=unstrippedDst.length();
			if(unstrippedLen>(dstPos-dst))
				dst = (wchar_t *)realloc(dst, (unstrippedLen+1)*sizeof(wchar_t));
			int count;
			for(count=0;count<unstrippedLen;count++)
				dst[count]=*(unstrippedDst.c_str()+count);
			//wcsncpy(dst, unstrippedDst.c_str(), unstrippedLen);
			dst[unstrippedLen]=0x0;
			free(src1);
		}
	}
		
	//	if ( DEBUG )
	//	wprintf(L"---\r\n%S\r\b---", dst);	
		
	
	
	_recursionCount--;
	if ( !handledOne )
	{
		// if there was nothing to free the dst pointer and return the src
		// do not free commentsRemoved; if they are source is pointing to them
		
		free(dst);
		//if(stripFlag)
		//	return (wchar_t*) src0;
		return (wchar_t*) src0;
	}
	else
	{
		// everything is in dst now
		if ( commentsRemoved )
			free(commentsRemoved);
		
		return dst;
	}
}

wchar_t* WikiMarkupParser::ExpandTemplate(const wchar_t* templateText)
{
	if ( !templateText || !*templateText )
		return NULL;
	
	const wchar_t* pos = PosOfNextParamPipe(templateText);
	if ( !*pos ) 
	{
		wchar_t* result = HandleKnownTemplatesAndVariables(templateText);
		if ( result )
			return result;
		
		pos = (wchar_t*) templateText + wcslen(templateText);
	}
	else
	{
		wchar_t* result = HandleKnownTemplatesAndVariables(templateText);
		if ( result )
			return result;
	}
	
	// wprintf(L"Expanding template:\r\n'%S'\n", templateText);

	int length = pos-templateText+1;
	wchar_t* preTemplateName = (wchar_t*) malloc(length*sizeof(wchar_t));
	wcsncpy(preTemplateName, templateText, pos-templateText);
	preTemplateName[pos-templateText] = 0x0;
	
	// remove garbage
	trim(preTemplateName);	
	
	if ( *preTemplateName!=L'#' && wcsstr(preTemplateName, L"{{") )
	{
		wchar_t* expandedTemplateName = ExpandTemplates(preTemplateName);
		if ( expandedTemplateName!=preTemplateName )
		{
			free(preTemplateName);
			preTemplateName = expandedTemplateName;
		}
	}

	wchar_t templateName[wcslen(preTemplateName)+1];
	wcscpy(templateName, preTemplateName);
	trim(templateName);
	
	// remove garbage
	free(preTemplateName);
	
	// wprintf(L"Template name: '%S'\n", templateName);
		
	if ( wcsstr(templateName, L"#if:")==templateName ) 
	{		
		if ( !*pos  ) 
		{
			// that is an if but it has no true-condition, hence also no false, so quit
			if ( !*pos )
				return NULL;
		}
	
		DBH Name(templateText);
		
		bool result = false;
		wchar_t* condition = templateName + 4;
		
		if ( *condition )
		{
			result = true;
			
			if ( wcsstr(condition, L"{{") )
			{
				wchar_t* expandedCondition = ExpandTemplates(condition);
				if ( expandedCondition!=condition )
				{
					/*
					wchar_t* newTemplate = (wchar_t*) malloc((2 + 4 + wcslen(expandedCondition) + wcslen(pos) + 2 + 1) * sizeof(wchar_t));
					wcscpy(newTemplate, L"{{#if:");
					wcscat(newTemplate, expandedCondition);
					wcscat(newTemplate, pos);
					wcscat(newTemplate, L"}}");
					
					free(expandedCondition);
					return newTemplate;
					 */
					
					trim(expandedCondition);
					if ( !*expandedCondition )
						result = false;
					
					free(expandedCondition);
				}
			}
		}
	
		pos++;
		if ( result )
		{
			// pos points to the true value (hopefully)
			if ( *pos ) 
			{
				const wchar_t* resultStart = pos;
				pos = PosOfNextParamPipe(pos);
				
				int length = pos - resultStart;
				wchar_t value[length+1];
				if ( length )
					wcsncpy(value, resultStart, length);
				value[length] = 0x0;
				trim_right(value);
				
				DBH Value(value);
				
				return wstrdup(value);
			}
			else
				return NULL;
		}
		else 
		{
			pos = PosOfNextParamPipe(pos);
			
			// pos no points to the pipe of the false condition
			if ( *pos )
			{
				pos++;
				
				DBH Value(pos);
				
				return wstrdup(pos);
			}
			else
				return NULL;
		}
	}
	else if ( wcsstr(templateName, L"#ifexist:")==templateName ) 
	{		
		if ( !*pos  ) 
		{
			// that is an if but it has no true-condition, hence also no false, so quit
			if ( !*pos )
				return NULL;
		}
		
		
		bool result = false;
		wchar_t* expression = wstrdup(templateName+9);		
		if ( *expression )
		{
			DBH Expression1(expression);
			if ( wcsstr(expression, L"{{") )
			{
				wchar_t* expandedExpression = ExpandTemplates(expression);
				if ( expandedExpression!=expression )
				{
					free(expression);
					
					trim(expandedExpression);
					expression = expandedExpression;
				}
			}
			
			// evaluate the expression (i.e. the article name) here
			DBH Expression(expression);
			
			TitleIndex* titleIndex = settings.GetTitleIndex(CPPStringUtils::to_string(_languageCodeW));
			ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(CPPStringUtils::to_utf8(wstring(expression)));
			
			result = !articleSearchResult;
			titleIndex->DeleteSearchResult(articleSearchResult);
		}
		free(expression);
		
		pos++;
		if ( result )
		{
			// pos points to the true value (hopefully)
			if ( *pos ) 
			{
				const wchar_t* resultStart = pos;
				pos = PosOfNextParamPipe(pos);
				
				int length = pos - resultStart;
				wchar_t value[length+1];
				if ( length )
					wcsncpy(value, resultStart, length);
				value[length] = 0x0;
				trim_right(value);
				
				DBH Value(value);
				
				return wstrdup(value);
			}
			else
				return NULL;
		}
		else 
		{
			pos = PosOfNextParamPipe(pos);
			
			// pos no points to the pipe of the false condition
			if ( *pos )
			{
				pos++;
				
				DBH Value(pos);
				
				return wstrdup(pos);
			}
			else
				return NULL;
		}
	}
	else if ( wcsstr(templateName, L"#ifexpr:")==templateName ) 
	{		
		if ( !*pos  ) 
		{
			// that is an if but it has no true-condition, hence also no false, so quit
			if ( !*pos )
				return NULL;
		}
		
		DBH Name(templateText);
		
		bool result = false;
		
		wchar_t* expression = wstrdup(templateName+8);		
		if ( *expression )
		{
			if ( wcsstr(expression, L"{{") )
			{
				wchar_t* expandedExpression = ExpandTemplates(expression);
				if ( expandedExpression!=expression )
				{
					free(expression);
					
					 trim(expandedExpression);
					expression = expandedExpression;
				}
			}
			
			// evaluate the expression here
			DBH Expression(expression);
			
			double result = EvaluateExpression(expression); 
			if ( result )
				result = true;
		}
		free(expression);
		
		pos++;
		if ( result )
		{
			// pos points to the true value (hopefully)
			if ( *pos ) 
			{
				const wchar_t* resultStart = pos;
				pos = PosOfNextParamPipe(pos);
				
				int length = pos - resultStart;
				wchar_t value[length+1];
				if ( length )
					wcsncpy(value, resultStart, length);
				value[length] = 0x0;
				trim_right(value);
				
				DBH Value(value);
				
				return wstrdup(value);
			}
			else
				return NULL;
		}
		else 
		{
			pos = PosOfNextParamPipe(pos);
			
			// pos no points to the pipe of the false condition
			if ( *pos )
			{
				pos++;
				
				DBH Value(pos);
				
				return wstrdup(pos);
			}
			else
				return NULL;
		}
	}
	else if ( wcsstr(templateName, L"#expr:")==templateName ) 
	{		
		if ( !*pos  ) 
		{
			// that is an if but it has no true-condition, hence also no false, so quit
			if ( !*pos )
				return NULL;
		}
		
		DBH Name(templateText);
		
		if ( templateName[6] )
		{
			wchar_t* expression = wstrdup(templateName+6);		
			if ( wcsstr(expression, L"{{") )
			{
				wchar_t* expandedExpression = ExpandTemplates(expression);
				if ( expandedExpression!=expression )
				{
					free(expression);
					
					trim(expandedExpression);
					expression = expandedExpression;
				}
			}
			
			// evaluate the expression here
			DBH Expression(expression);
			
			wchar_t result[256];
			swprintf(result, 256, L"%g", EvaluateExpression(expression));
			free(expression);
			return wstrdup(result);
		}

		return NULL;
	}
	else if ( wcsstr(templateName, L"#ifeq:")==templateName || wcsstr(templateName, L"#ifneq:")==templateName )
	{
		DBH TT(templateText);
		bool notEqual = wcsstr(templateName, L"#ifeq:")==NULL;
		
		pos = wcsstr(templateText, L"#if") + (notEqual ? 7 : 6);
		if ( *pos )
		{
			const wchar_t* leftStart = pos;
			
			pos = PosOfNextParamPipe(pos);

			int length = pos - leftStart;			
			
			wchar_t left[length + 1];
			if ( length )
				wcsncpy(left, leftStart, length);
			left[length] = 0x0;
			trim(left);
			
			DBH Left(left);
			
			if ( wcsstr(left, L"{{") )
			{
				wchar_t* expandedLeft = ExpandTemplates(left);
				if ( expandedLeft!=left )
				{
					int length = 2 + 6 + wcslen(expandedLeft) + wcslen(pos) + 2;
					if ( notEqual )
						length++;
					
					wchar_t* newTemplate = (wchar_t*) malloc( (length+1) * sizeof(wchar_t) );
					if ( notEqual )
						wcscpy(newTemplate, L"{{#ifneq:");
					else
						wcscpy(newTemplate, L"{{#ifeq:");
					
					wcscat(newTemplate, expandedLeft);
					wcscat(newTemplate, pos);
					wcscat(newTemplate, L"}}");
					
					free(expandedLeft);
					return newTemplate;
				}
			}
			
			if ( *pos==L'|' )
			{
				pos++;
				const wchar_t* rightStart = pos;
				pos = PosOfNextParamPipe(pos);
				
				if ( *pos==L'|' ) 
				{
					length = pos - rightStart;
					
					wchar_t right[length + 1];
					if ( length )
						wcsncpy(right, rightStart, length);
					right[length] = 0x0;
					trim(right);

					DBH Right(right);

					if ( wcsstr(right, L"{{") )
					{
						wchar_t* expandedRight = ExpandTemplates(right);
						if ( expandedRight!=right )
						{
							int length = 2 + 6 + + wcslen(left) + 1 + wcslen(expandedRight) + wcslen(pos) + 2;
							if ( notEqual )
								length++;
							
							wchar_t* newTemplate = (wchar_t*) malloc( (length+1) * sizeof(wchar_t) );
							if ( notEqual )
								wcscpy(newTemplate, L"{{#ifneq:");
							else
								wcscpy(newTemplate, L"{{#ifeq:");
							
							wcscat(newTemplate, left);
							wcscat(newTemplate, L"|");
							wcscat(newTemplate, expandedRight);
							wcscat(newTemplate, pos);
							wcscat(newTemplate, L"}}");
							
							free(expandedRight);
							return newTemplate;
						}
					}
					
					pos++;
					const wchar_t* trueValue = pos; 
					
					pos = PosOfNextParamPipe(pos);
					
					if ( !wcscmp(left, right) && !notEqual )
					{
						length = pos - trueValue;
						
						wchar_t value[length + 1];
						if ( length )
							wcsncpy(value, trueValue, length);
						value[length] = 0x0;
						trim(value);
						
						return wstrdup(value);
					}
					else 
					{
						if ( *pos )
						{
							// pos is pointing to the pipe sign
							pos++;
							
							length = wcslen(pos);
							
							wchar_t value[length + 1];
							if ( length )
								wcsncpy(value, pos, length);
							value[length] = 0x0;
							trim(value);
							
							return wstrdup(value);
						}
					}
				}
			}
		}
		
		// wprintf(L"template expands to nothing\n");
		return NULL;
	}
	else if ( wcsstr(templateName, L"#switch:")==templateName )
	{
		if ( !*pos )
			return NULL;

		DBH TemplateText(templateText);

		int length = wcslen(templateName) - 8;
		
		wchar_t phrase[length+1];
		if ( length )
			wcsncpy(phrase, templateName + 8, length);
		phrase[length] = 0x0;
		trim(phrase);
		
		DBH Phrase(phrase);
		
		// wprintf(L"\r\n%S\r\n", templateText);
		if ( wcsstr(phrase, L"{{") )
		{
			wchar_t* expandedPhrase = ExpandTemplates(phrase);
			if ( expandedPhrase!=phrase )
			{
				wchar_t* newTemplate = (wchar_t*) malloc((2 + 8 + wcslen(expandedPhrase) + wcslen(pos) + 2 + 1) * sizeof(wchar_t));
				wcscpy(newTemplate, L"{{#switch:");
				wcscat(newTemplate, expandedPhrase);
				wcscat(newTemplate, pos);
				wcscat(newTemplate, L"}}");
				
				free(expandedPhrase);
				return newTemplate;
			}
		}
		
		bool takeNext = false;
		bool takeNextForDefault = false;
		
		wchar_t* defaultValue = NULL;
		
		pos++;
		while (*pos)
		{			
			const wchar_t* valueStart = pos;
			pos = PosOfNextParamPipe(pos);
			
			length = pos - valueStart;
			wchar_t data[length+1];
			wcsncpy(data, valueStart, length);
			data[length] = 0x0;
			// trim(data);
			
			DBH Data(data);

			wchar_t* equalPos = wcsstr(data, L"=");
			if ( equalPos )
			{
				length = equalPos - data;
				wchar_t name[length+1];
				if ( length )
					wcsncpy(name, data, length);
				name[length] = 0x0;
				trim(name);
				
				DBH Name(name);
				if ( !wcscmp(phrase, name) || takeNext )
				{
					if ( defaultValue )
						free(defaultValue);
					
					wchar_t* value = equalPos + 1;
					// trim(value);
					
					return wstrdup(value);
				}
				else if ( !wcscmp(name, L"#default") || takeNextForDefault )
				{
					if ( defaultValue )
						free(defaultValue);
					
					wchar_t* value = equalPos + 1;
					// trim(value);
					defaultValue = wstrdup(value);
					takeNextForDefault = false;
				}
			}
			else
			{
				// data this is just a name
				if ( !wcscmp(data, phrase) )
					takeNext = true;
				else if (!wcscmp(data, L"#default") )
					takeNextForDefault = true;
			}
			
			if ( *pos )
				pos++;
		}
		
		if ( defaultValue ) 
			return defaultValue;
		else
			return NULL;
	}
	else if ( *templateName==L'#' )
	{
		wstring error = L"USP:" + wstring(templateName);
		// probably #ifexp
		return wstrdup(error.c_str());
	}
	else
	{
		// check for ignored templates
		wchar_t* name = wstrdup(templateName);
		to_lower(name);
		const wchar_t** ignored = ignoredTemplates;
		while ( *ignored )
		{
			if ( !wcscmp(*ignored, name) )
			{
				free(name);
				return NULL;
			}
			
			ignored++;
		}
		
		free(name);
	}
	
	// Let's try to get the template
	WikiMarkupGetter wikiMarkupGetter(CPPStringUtils::to_string(_languageCodeW));
	
	string templatePrefix = _titleIndex->TemplateNamespace();
	if ( templatePrefix.empty() )
		templatePrefix = _languageConfig->GetSetting("templatePrefix", "Template:");
	else
		templatePrefix += ":";
	wstring wikiTemplate = wikiMarkupGetter.GetTemplate(CPPStringUtils::to_utf8(templateName), templatePrefix);
	
	// if ( DEBUG )
	//	wprintf(L"\r\nGot template:\r\n%S\r\n", wikiTemplate.c_str());	
	
	if ( wikiTemplate==L"-" )
		return NotHandledText(templateName);
	else if ( wikiTemplate== L"{{" + wstring(templateName) + L"}}" )
		return NULL; // prevents recursion:

	TEMPLATEPARAM* params = NULL;
	
	// get the template params, pos point to the pipe or the end of the string
	if ( *pos )
		pos++;
	
	wchar_t* templateParameters = wstrdup(pos);	
	
	if ( wcsstr(templateParameters, L"{{") )
	{
		wchar_t* newParams = ExpandTemplates(templateParameters);
		if ( newParams!=templateParameters )
		{
			// wprintf(L"\r\n%S\r\nNew:%S\r\n", templateParameters, newParams);
			free(templateParameters);
			templateParameters = newParams;
		}
	}		

	const wchar_t* templateParams = templateParameters;	
	int paramCount = 0;
	if ( wcslen(templateParams) )
	{
		pos = PosOfNextParamPipe(templateParams);
		
		int length = pos-templateParams;
		wchar_t firstParam[length + 1];
		if ( length )
			wcsncpy(firstParam, templateParams, length);
		firstParam[length] = 0x0;
		trim(firstParam);
		
		if ( wcsstr(firstParam, L"=") )
		{
			// as we have an equal sign we're dealing with named parameters
			while ( *templateParams )
			{
				pos = PosOfNextParamPipe(templateParams);

				int length = pos - templateParams;
				wchar_t data[length + 1];
				if ( length )
					wcsncpy(data, templateParams, length);
				data[length] = 0x0;
				trim(data);
				
				wchar_t* equalPos = wcsstr(data, L"=");
				if ( equalPos )
				{					
					int length = equalPos - data;
					wchar_t name[length + 1];
					if ( length )
						wcsncpy(name, data, length);
					name[length] = 0x0;
					trim(name);
					
					if ( *name )
					{
						equalPos++;
						paramCount++;
						
						length = wcslen(equalPos);
						wchar_t value[length + 1];
						if ( length )
							wcsncpy(value, equalPos, length);
						value[length] = 0x0;
						trim_right(value);
					
						TEMPLATEPARAM* param = new TEMPLATEPARAM();
						param->next = params;
						param->name = wstring(name);
						param->position = CPPStringUtils::to_wstring(paramCount);
						param->value = wstring(value);
					
						params = param;
					}
				}
				else
				{
					if ( params && *data )
					{
						// add the data (if present) to the last found params (as we have no euqal sign we have not name, looks like a list)
						params->value += L"| " + wstring(data);
					}
				}
				
				templateParams = pos;
				if ( *templateParams )
					templateParams++;
			}
		}
		else
		{
			// parameters are not named
			// as we have an equal sign we're dealing with named parameters
			while ( *templateParams )
			{
				pos = PosOfNextParamPipe(templateParams);
				
				int length = pos - templateParams;
				wchar_t data[length + 1];
				if ( length )
					wcsncpy(data, templateParams, length);
				data[length] = 0x0;
				trim(data);
			
				paramCount++;
												
				TEMPLATEPARAM* param = new TEMPLATEPARAM();
				param->next = params;
				param->name = wstring();
				param->position = CPPStringUtils::to_wstring(paramCount);
				param->value = wstring(data);
				params = param;
				
				templateParams = pos;
				if ( *templateParams )
					templateParams++;
			}
		}
	}
				
	TEMPLATEPARAM* listOfParams[paramCount];
	if ( paramCount>0 )
	{
		TEMPLATEPARAM* help = params;
		
		// add the param to a list (in revers order)
		for (int i=paramCount-1; i>=0; i--)
		{
			listOfParams[i] = help;
			help = help->next;
		}
		
		// for ( int i=0; i<paramCount; i++)
		// 	wprintf(L"%S. %S=%S\r\n", listOfParams[i]->position.c_str(), listOfParams[i]->name.c_str(), listOfParams[i]->value.c_str());
	}
	free(templateParameters);
		
	// so we have the template, lets parse out all the params
	size_t start = 0;
	while ( (start=wikiTemplate.find(L"{{{", start))!=string::npos )
	{
		start += 3;
		
		// replace params from inside to outside
		while (wikiTemplate[start]==L'{')
			start++;
		
		int end = start;
		int length = wikiTemplate.length();
		int braketCount = 3;
		
		while ( braketCount && end<length )
		{
			wchar_t c = wikiTemplate[end];
			
			if ( c=='{' )
				braketCount++;
			else if ( c=='}' )
				braketCount--;
			
			// we're on the way out but we don't see enought brakets
			if ( (braketCount<3) && c!=L'}' )
				break;
			
			end++;
		}
		
		if ( braketCount ) 
		{
			// this is an error, skip that section
			start = end;
			continue;
		}
		
		length = end - start - 3;
		wstring paramName = CPPStringUtils::trim(wikiTemplate.substr(start, length));
		
		wstring paramValue = wstring();
		wstring alternateValue = wstring();
		
		size_t spliterPos = 0;
		if ( (spliterPos=paramName.find(L"|"))!=string::npos ) 
		{
			// try to find the optional string
			alternateValue = paramName.substr(spliterPos+1);
			paramName = CPPStringUtils::trim(paramName.substr(0, spliterPos));
		}
		
		for (int i=0; i<paramCount; i++)
		{
			if ( listOfParams[i]->name==paramName ) 
			{
				paramValue = wstring(listOfParams[i]->value);
				break;
			}
			else if ( listOfParams[i]->position==paramName ) 
			{
				paramValue = wstring(listOfParams[i]->value);
				break;
			}
		}
				
		if ( paramValue.empty() )
			paramValue = alternateValue;
		
		if ( paramValue.empty() ) 
		{
			wikiTemplate.erase(start-3, length+6);
			start = start - 3;
		}
		else
		{
			wikiTemplate.replace(start-3, length+6, paramValue, 0, paramValue.length());
			start = start - 3;
		}
	}
	// wprintf(L"Result:\n%S\n", wikiTemplate.c_str());
	
	// cleanup
	while ( params )
	{
		TEMPLATEPARAM* help = params;
		params = params->next;
		delete help;
	}
	
	// if this expands to a table, add a newline in front
	if ( wikiTemplate.length()>2 && wikiTemplate[0]==L'{' && wikiTemplate[1]==L'|' )
		wikiTemplate = L"\n" + wikiTemplate;
	
	return wstrdup(wikiTemplate.c_str());
}

wchar_t* WikiMarkupParser::HandleKnownTemplatesAndVariables(const wchar_t* text)
{
	if ( text==NULL )
		return NULL;

	/* Table helpers  */
	else if ( !wcscmp(text, L"!-") )
		return wstrdup(L"|-");
	else if ( !wcscmp(text, L"!!") )
		return wstrdup(L"||");
	else if ( !wcscmp(text, L"!-!") )
		return wstrdup(L"|-\n|");
	else if ( !wcscmp(text, L"!+") )
		return wstrdup(L"|+");
	else if ( !wcscmp(text, L"!~") )
		return wstrdup(L"|-\n!");
	else if ( !wcscmp(text, L"(!") )
		return wstrdup(L"{|");
	else if ( !wcscmp(text, L"!)") )
		return wstrdup(L"|}");
	else if ( !wcscmp(text, L"((") )
		return wstrdup(L"{{");
	else if ( !wcscmp(text, L"))") )
		return wstrdup(L"}}");
	
	/* Namespaces and urls functions */
	else if ( startsWith(text, L"ns:") )
	{
		const wchar_t* number = text + 3;
		if ( !*number )
			return NotHandledText(text);
		
		int which = watoi(number);
		if ( which<1 || which>15 )
			return NotHandledText(text);
		
		return wstrdup(nsNames[which]);
	}	
	else if ( startsWith(text, L"localurl:") )
	{
		wchar_t* url = wcsstr(text, L":") + 1;
		if ( !*url )
			return NotHandledText(text);
			
		wchar_t buffer[2 + wcslen(_languageCodeW) + 1 + wcslen(url) + 1];
		wcscat(buffer, _languageCodeW);
		wcscat(buffer, L"/");
		wcscat(buffer, url);
		
		return wstrdup(buffer);
	}
	else if ( startsWith(text, L"urlencode:") )
	{
		wchar_t* url = wcsstr(text, L":") + 1;
		if ( !*(url+1) )
			return NotHandledText(text);

		return wstrdup(CPPStringUtils::url_encode(wstring(url)).c_str());
	}
	else if ( startsWith(text, L"anchorencode:") )
	{
		wchar_t* url = wcsstr(text, L":") + 1;
		if ( !*(url+1) )
			return NotHandledText(text);
		
		return wstrdup(CPPStringUtils::url_encode(wstring(url)).c_str());
	}
	else if ( startsWith(text, L"fullurl:") )
	{
		wchar_t* url = wcsstr(text, L":") + 1;
		if ( !*url )
			return NotHandledText(text);
		
		wchar_t buffer[22 + wcslen(_languageCodeW) + 1 + wcslen(url)];
		wcscpy(buffer, L"http://127.0.0.1/wiki/");
		wcscat(buffer, _languageCodeW);
		wcscat(buffer, L"/");
		wcscat(buffer, url);
		
		return wstrdup(buffer);
	}

	/* Formatting */         
	else if ( startsWith(text, L"#tag:") )
	{
		const wchar_t *value = text + 5;
		if ( !*value )
			return NotHandledText(text);
		wchar_t *pLastPipe = wcsrchr(value, L'|');
		wchar_t *buffer;
		fwprintf(stderr, L"#tag:%S\n",value);
		if(pLastPipe)
		{
			//has some content
			if(pLastPipe==value)
			{
				//empty result
				return L"";
			}
			//else if(pLastPipe==wcschr(value, L'|'))
			else
			{
				wchar_t *pFirstPipe=wcschr(value, L'|');
				size_t tagNameLen= (pFirstPipe-value);

				buffer = (wchar_t *)malloc(sizeof(wchar_t)*(wcslen(value)+tagNameLen+5));
				buffer[0]=L'<';
				const wchar_t *pValue=value;
				wchar_t *pBuffer=buffer+1;
				while(*pValue){
					if(*pValue==L'|'){
						if(pValue==pLastPipe)
							*pBuffer=L'>';
						else
							*pBuffer=L' ';
					}
					else 
						*pBuffer=*pValue;
					pBuffer++;
					pValue++;
				}
				*pBuffer=L'<';
				pBuffer++;
				*pBuffer=L'/';
				pBuffer++;
				
				wcsncpy(pBuffer, value, tagNameLen);
				pBuffer+=tagNameLen;
				*pBuffer=L'>';
				pBuffer++;
				*pBuffer=0x0;
				fwprintf(stderr, L"tagNameLen: %d,#tag:%S\n",tagNameLen, buffer);
				return buffer;
			}
		}
		else
		{
			//no content
			int tagNameLen=wcslen(value);
			buffer = (wchar_t *)malloc(sizeof(wchar_t)*(tagNameLen+5));
			buffer[0]=L'<';
			wcsncpy(buffer+1,value, tagNameLen);
			buffer[tagNameLen+1]=L' ';
			buffer[tagNameLen+2]=L'/';
			buffer[tagNameLen+3]=L'>';
			buffer[tagNameLen+4]=0x0;
			fwprintf(stderr, L"tagNameLen: %d,#tag:%S\n",tagNameLen, buffer);
			return buffer;
		}
	}
	else if ( startsWith(text, L"#language:") )
	{
		const wchar_t* value = text + 10;
		if ( !*value )
			return NotHandledText(text);
		
		return wstrdup(value);
	}
	else if ( startsWith(text, L"lc:") )
	{
		int length = wcslen(text) - 3;
		wchar_t value[length+1];
		
		if ( length )
			wcsncpy(value, text+3, length);
		value[length] = 0x0;
		to_lower(value);
		
		return wstrdup(value);
	}
	else if ( startsWith(text, L"lcfirst:") )
	{
		int length = wcslen(text) - 8;
		wchar_t value[length+1];
		
		if ( length )
			wcsncpy(value, text+8, length);
		value[length] = 0x0;
		
		value[0] = _to_wlower(value[0]);
		
		return wstrdup(value);
	}
	else if ( startsWith(text, L"uc:") )
	{
		int length = wcslen(text) - 3;
		wchar_t value[length+1];
		
		if ( length )
			wcsncpy(value, text+3, length);
		value[length] = 0x0;
		to_upper(value);
		
		return wstrdup(value);
	}
	else if ( startsWith(text, L"ucfirst:") )
	{
		int length = wcslen(text) - 8;
		wchar_t value[length+1];
		
		if ( length )
			wcsncpy(value, text+8, length);
		value[length] = 0x0;
		
		value[0] = _to_wupper(value[0]);
		
		return wstrdup(value);
	}                        
	else if ( startsWith(text, L"formatnum:") )
	{		
		int length = wcslen(text + 10);
		if ( ! length )
			return NotHandledText(text);

		wchar_t value[length + 1];
		wcscpy(value, text+10);
		trim(value);
	
		wstring result;
		
		wchar_t decimalSeperator = _languageConfig->GetSetting("decimalSeperator", ",")[0];
		wchar_t fractionSeperator;
		if ( decimalSeperator==',' )
			fractionSeperator = L'.';
		else
			fractionSeperator = L',';
		
		wchar_t buffer[2];
		buffer[0] = fractionSeperator;
		buffer[1] = 0x0;

		const wchar_t* help = wcsstr(value, buffer);
		if ( help )
		{
			result = help;
			help--;
		}
		else
			help = value + (wcslen(value) - 1);
	
		int count = 0;
		while ( help>=value )
		{
			if ( count==3 )
			{
				result = decimalSeperator + result;
				count = 0;
			}
			if ( *help>=0x30 && *help<=0x39 )
				count++;
			
			result = *help + result;
			help--;
		}
		
		return wstrdup(result.c_str());
	}
	else if ( startsWith(text, L"padleft:") )
	{
		return wstrdup(text+8);
	}
	else if ( startsWith(text, L"padright:") )
	{
		return wstrdup(text+8);
	}
	
	/* conversion */
	else if ( startsWith(text, L"convert|") )
	{
		return wstrdup(text+8);
	}
	else if ( startsWith(text, L"Dmoz|") )
	{
		return wstrdup(L"");
	}
	
	/* Date and Time functions */
	
	else if ( !wcscmp(text, L"CURRENTDAY") || !wcscmp(text, L"LOCALDAY") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%i", lt->tm_mday);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTDAY2") || !wcscmp(text, L"LOCALDAY2") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%02i", lt->tm_mday);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTDAYNAME") || !wcscmp(text, L"LOCALDAYNAME") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		return wstrdup(DayName(lt->tm_wday).c_str());
	}
	else if ( !wcscmp(text, L"CURRENTDOW") || !wcscmp(text, L"LOCALDOW") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%i", lt->tm_wday);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTMONTH") ||  !wcscmp(text, L"LOCALMONTH") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%02i", lt->tm_mon + 1);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTMONTHABBREV") || !wcscmp(text, L"LOCALMONTHABBREV") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		return wstrdup(AbbrMonthName(lt->tm_mon).c_str());
	}
	else if ( !wcscmp(text, L"CURRENTMONTHNAME") || !wcscmp(text, L"CURRENTMONTHNAMEGEN") || !wcscmp(text, L"LOCALMONTHNAME") || !wcscmp(text, L"LOCALMONTHNAMEGEN") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		return wstrdup(MonthName(lt->tm_mon).c_str());
	}
	else if ( !wcscmp(text, L"CURRENTTIME") || !wcscmp(text, L"LOCALTIME") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%02i:%02i", lt->tm_hour, lt->tm_min);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTHOUR") || !wcscmp(text, L"LOCALHOUR") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%02i", lt->tm_hour);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTMINUTE") || !wcscmp(text, L"LOCALMINUTE") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%02i", lt->tm_min);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTWEEK") || !wcscmp(text, L"LOCALWEEK") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);

		wchar_t buffer[16];
		swprintf(buffer, 16, L"%i", lt->tm_yday/7 + 1);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTYEAR") || !wcscmp(text, L"LOCALYEAR") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%04i", lt->tm_year + 1900);
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"CURRENTTIMESTAMP") || !wcscmp(text, L"LOCALTIMESTAMP") ) 
	{
		time_t t; time(&t); struct tm* lt; lt = localtime(&t);
		
		wchar_t buffer[16];
		swprintf(buffer, 16, L"%04i%02i%02i%02i%02i%02i", lt->tm_year + 1900, lt->tm_mon, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
		return wstrdup(buffer);
	}
	
	/* Page names and related info  */
	
	else if ( !wcscmp(text, L"PAGENAME") || !wcscmp(text, L"PAGENAMEE") )
	{
		if ( _pageName==NULL )
			return wstrdup(L"");
		else
			return wstrdup(_pageName);
	}
	else if ( !wcscmp(text, L"SUBPAGENAME") || !wcscmp(text, L"SUBPAGENAMEE") )
	{
		if ( _pageName==NULL )
			return wstrdup(L"");

		const wchar_t* slash = NULL;
		const wchar_t* help = _pageName;
		while ( help=wcsstr(help, L"/") )
		{
			help++;
			slash = help;
		}
		
		if ( slash )
			return wstrdup(slash);
		else
			return wstrdup(L"");
	}
	else if ( !wcscmp(text, L"BASEPAGENAME") || !wcscmp(text, L"BASEPAGENAMEE") )
	{
		if ( _pageName==NULL )
			return wstrdup(L"");
		
		const wchar_t* slash = wcsstr(_pageName, L"/");
		if ( slash )
			return wstrndup(_pageName, slash-_pageName);
		else
			return wstrdup(_pageName);
	}
	else if ( !wcscmp(text, L"NAMESPACE") || !wcscmp(text, L"NAMESPACEE") )
		return wstrdup(_languageCodeW);
	else if ( !wcscmp(text, L"FULLPAGENAME") || !wcscmp(text, L"FULLPAGENAMEE") )
	{
		wchar_t buffer[wcslen(_languageCodeW) + 1 + wcslen(_pageName) + 1];
		wcscpy(buffer, _languageCodeW);
		wcscat(buffer, L"/");
		wcscat(buffer, _pageName);

		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"TALKSPACE") || !wcscmp(text, L"TALKSPACEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"SUBJECTSPACE") || !wcscmp(text, L"SUBJECTSPACEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"ARTICLESPACE") || !wcscmp(text, L"ARTICLESPACEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"TALKPAGENAME") || !wcscmp(text, L"TALKPAGENAMEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"SUBJECTPAGENAME") || !wcscmp(text, L"SUBJECTPAGENAMEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"ARTICLEPAGENAME") || !wcscmp(text, L"ARTICLEPAGENAMEE") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"REVISIONID") ) 
		return wstrdup(L"0");
	else if ( !wcscmp(text, L"REVISIONDAY") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"REVISIONDAY2") ) 
		return wstrdup(L"01");
	else if ( !wcscmp(text, L"REVISIONMONTH") ) 
		return wstrdup(L"01");
	else if ( !wcscmp(text, L"REVISIONYEAR") ) 
		return wstrdup(L"2007");
	else if ( !wcscmp(text, L"REVISIONTIMESTAMP") ) 
		return wstrdup(L"20070101000000");
	else if ( !wcscmp(text, L"SITENAME") ) 
		return wstrdup(L"Offline-Wikipedia");
	else if ( !wcscmp(text, L"SERVER") ) 
		return wstrdup(L"http://127.0.0.1");
	else if ( !wcscmp(text, L"SCRIPTPATH") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"/scripts") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"SERVERNAME") ) 
		return wstrdup(L"127.0.0.1");

	/* statistics */ 
	else if ( !wcscmp(text, L"CURRENTVERSION") ) 
		return wstrdup(CPPStringUtils::to_wstring(settings.Version()).c_str());
	else if ( !wcscmp(text, L"NUMBEROFEDITS") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"NUMBEROFARTICLES") ) 
	{
		TitleIndex* titleIndex = settings.GetTitleIndex(CPPStringUtils::to_string(_languageCodeW));
		wchar_t buffer[32];
		swprintf(buffer, 32, L"%i", titleIndex->NumberOfArticles());
		return wstrdup(buffer);
	}
	else if ( !wcscmp(text, L"NUMBEROFPAGES") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"NUMBEROFFILES") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"NUMBEROFEDITS") ) 
		return wstrdup(L"1.0");
	else if ( !wcscmp(text, L"NUMBEROFUSERS") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"NUMBEROFADMINS") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"PAGESINNAMESPACE") ) 
		return wstrdup(L"1");
	else if ( !wcscmp(text, L"PAGESINNS:") ) 
		return wstrdup(L"1");

	/* Miscellany */
	else if ( startsWith(text, L"DISPLAYTITLE:") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"DIRMARK") || !wcscmp(text, L"DIRECTIONMARK") ) 
		return wstrdup(L"");
	else if ( !wcscmp(text, L"CONTENTLANGUAGE") ) 
		return wstrdup(_languageCodeW);
	else if ( startsWith(text, L"DEFAULTSORT") )
		return wstrdup(L""); 
	
	else if ( !wcscmp(text, L"reflist") )
		return wstrdup(L"<references />");	
		
	else
		return NULL; // not handled	
}
wchar_t* WikiMarkupParser::NotHandledText(const wchar_t* text)
{
	wstring buffer = L"<span class=\"wkUnknownTemplate\">";
	
	string templatePrefix = _languageConfig->GetSetting("templatePrefix", "Template:");
	buffer += CPPStringUtils::to_wstring(templatePrefix) + text;
	buffer += L"</span>";

	return wstrdup(buffer.c_str());	
}

const wchar_t* WikiMarkupParser::PosOfNextParamPipe(const wchar_t* pos)
{
	if ( !pos || !*pos )
		return pos;
	
	int openCurlyBrakets = 0;
	int openSquareBrakets = 0;
	while ( *pos ) 
	{
		switch ( *pos )
		{
			case L'|':
				if ( !openCurlyBrakets && !openSquareBrakets )
					return pos;
				break;
				
			case L'[':
				openSquareBrakets++;
				break;
				
			case L']':
				openSquareBrakets--;
				break;
			
			case L'{':
				openCurlyBrakets++;
				break;
				
			case L'}':
				openCurlyBrakets--;
				break;
		}
		
		pos++;
	}
	
	return pos;
}

wchar_t* WikiMarkupParser::GetTextInDoubleBrakets(wchar_t startBraket, wchar_t endBraket)
{
	// assuming the first braket was already read and we're not looking at the second one
	// the returned text is without the brakets

	wchar_t c;

	wchar_t* text = NULL;
	if ( Peek()!=startBraket )
		return 0;

	// skip the second braked
	GetNextChar();
	
	int braketCount = 2;
	int count = 0;
	while ( c=Peek(count) )
	{
		if ( c==startBraket )
			braketCount++;
		else if ( c==endBraket ) {
			braketCount--;
			if ( braketCount==1 && Peek(count+1)==endBraket )
				break;
		}
		
		count++;
	}
	
	text = (wchar_t*) malloc((count+1) * sizeof(wchar_t));
	wchar_t* pText = text;
		
	while (count--)
		*pText++ = GetNextChar();
	*pText = 0x0;

	// skip the last two closing brakets
	GetNextChar();
	GetNextChar();

	return text;
}

wchar_t* WikiMarkupParser::GetTextInSingleBrakets(wchar_t startBraket, wchar_t endBraket)
{
	// assuming the first braket was already read and we're not looking at the second one
	// the returned text is without the brakets
	
	wchar_t c;
	int braketCount = 1;
	int count = 0;
	while ( c=Peek(count) )
	{
		if ( c==startBraket )
			braketCount++;
		else if ( c==endBraket ) {
			braketCount--;
			if ( braketCount==0 )
				break;
		}
		
		count++;
	}
	
	wchar_t* text = (wchar_t*) malloc((count+1) * sizeof(wchar_t));
	wchar_t* pText = text;
	
	while (count--)
		*pText++ = GetNextChar();
	*pText = 0x0;
	
	// skip the last closing brakets
	GetNextChar();
	
	return text;
}

wchar_t* WikiMarkupParser::GetNextLine()
{
	int count = 0;
	wchar_t c;
	while ( c=Peek(count) )
	{
		if ( c=='\n' )
			break;
		
		count++;
	}
	
	wchar_t* line = (wchar_t*) malloc((count+1)*sizeof(wchar_t));
	wchar_t* pLine = line;
	
	while ( count-- )
		*pLine++ = GetNextChar();
	*pLine = 0x0;
	
	// This is the line feed
	Eat(1);
	_newLine++;
	
	return line;
}

wchar_t* WikiMarkupParser::GetTextUntilEndOfTag(const wchar_t *tagName)
{
	wchar_t* start = _pCurrentInput;
	wchar_t* stop = NULL;
	
	int tagNameLen=wcslen(tagName);

	wchar_t c;
	while ( c=GetNextChar() )
	{
		if ( c==L'<' && ( Peek()==L'/' || isalpha(Peek())) )
		{
			if(wcsncmp(_pCurrentInput+1,tagName,tagNameLen)!=0)
				continue;
			// advance the pointer back to the start of the tag
			_pCurrentInput--;

			stop = _pCurrentInput;
			break;
		}
	}
	
	if ( start>=stop )
		return NULL;
	
	int length = stop-start;
	wchar_t* result = (wchar_t*) malloc((length+1) * sizeof(wchar_t));
	wcsncpy(result, start, length);
	result[length] = 0x0;
	
	return result;
}


wchar_t* WikiMarkupParser::GetTextUntilNextTag()
{
	wchar_t* start = _pCurrentInput;
	wchar_t* stop = NULL;
	
	wchar_t c;
	while ( c=GetNextChar() )
	{
		if ( c==L'<' && ( Peek()==L'/' || isalpha(Peek())) )
		{
			// advance the pointer back to the start of the tag
			_pCurrentInput--;

			stop = _pCurrentInput;
			break;
		}
	}
	
	if ( start>=stop )
		return NULL;
	
	int length = stop-start;
	wchar_t* result = (wchar_t*) malloc((length+1) * sizeof(wchar_t));
	wcsncpy(result, start, length);
	result[length] = 0x0;
	
	return result;
}

bool WikiMarkupParser::GetPixelUnit(wchar_t* src, int* width, int* height)
{
	if ( !src || !*src )
		return false;
	
	while ( *src==' ' )
		src++;
	
	int w = 0;
	while ( isdigit(*src) )
	{
		w *= 10;
		w += *src - '0';
		src++;
	}
	
	if ( !src )
		return false;
	
	int h = -1;	
	if ( _to_wlower(*src)==L'x' )
	{
		src++;
		if ( !isdigit(*src) )
			return false;

		h = 0;
		while ( isdigit(*src) )
		{
			h *= 10;
			h += *src - '0';
			src++;
		}
	}
	
	if ( _to_wlower(*src)!=L'p' )
		return false;
	src++;

	if ( _to_wlower(*src)!=L'x' )
		return false;
	src++;
	
	while ( *src==' ' )
		src++;
	
	if ( *src )
		return false;
	
	if ( width )
		*width = w;
	
	if ( height && h>=0 )
		*height = h;
	
	return true;
}

inline void WikiMarkupParser::Append(wchar_t c) 
{
	if ( _iOutputRemain==0 ) {
		if ( _iOutputSize==0 ) {
			_iOutputSize = OUTPUT_GROWS;
			_pOutput = (wchar_t*) malloc( (_iOutputSize+1)*sizeof(wchar_t) );
			
			_pCurrentOutput = _pOutput;
			_iOutputRemain = _iOutputSize;
		}
		else {
			_pOutput = (wchar_t*) realloc(_pOutput, (_iOutputSize+OUTPUT_GROWS+1)*sizeof(wchar_t) );
			_pCurrentOutput = (_pOutput+_iOutputSize);
			_iOutputSize += OUTPUT_GROWS;
			_iOutputRemain = OUTPUT_GROWS;
		}
	}
	
	*_pCurrentOutput++ = c;
	*_pCurrentOutput = 0x0;
	_iOutputRemain--;
}

void WikiMarkupParser::Append(const wchar_t* msg) {
	if ( msg==NULL )
		return;
		
	while(*msg!=0x0)
		Append(*msg++);
}

void WikiMarkupParser::AppendHtml(const wchar_t* html) 
{
	if ( html==NULL )
		return;
	
	while(*html!=0x0)
		Append(*html++);
}

void WikiMarkupParser::PushTag(wchar_t* name, bool output)
{
	tagType* newTag = new tagType;
	newTag->name = (wchar_t*) malloc( (wcslen(name)+1)*sizeof(wchar_t) );
	wcscpy(newTag->name, name);
	newTag->position = _pCurrentOutput - _pOutput;
	 
	newTag->pPrevious = _pCurrentTag;
	newTag->pNext = NULL;
	
	_pCurrentTag = newTag;
	
	if ( output )
	{ 
		wchar_t help[256];
		wcscpy(help, L"<");
		wcscat(help, name);
		wcscat(help, L">");
		Append(help);
	}
}

void WikiMarkupParser::PopTag(wchar_t* name, bool output) 
{
	if ( _pCurrentTag==NULL ) {
		return;
		// throw "Running out of tags on the stack (to much closing tags)";
	}
	
	if ( wcscmp(_pCurrentTag->name, name)!=0) {
		return;
		// throw "Unmatisch start/end tags on stack";
	}

	tagType *oldTag = _pCurrentTag;
	_pCurrentTag = oldTag->pPrevious;
	if ( _pCurrentTag!=NULL ) 
		_pCurrentTag->pNext = NULL;
	
	free(oldTag->name);
	delete(oldTag);
	
	if ( output )
	{ 
		wchar_t help[wcslen(name)+4];
		wcscpy(help, L"</");
		wcscat(help, name);
		wcscat(help, L">");
		Append(help);
	}
}

bool WikiMarkupParser::TopTagIs(wchar_t* name)
{
	if ( !_pCurrentTag || !name ) 
		return false;
	
	return wcscmp(_pCurrentTag->name, name) ? false : true;
}

void WikiMarkupParser::HandleChar(wchar_t c)
{
	if (c == ' ')
		// eat multiple spaces
		return;
		
	if (c != '\n')
		Append(c);
}

void WikiMarkupParser::HandleParagraph()
{
	CloseOpenWikiTags();
	Append(L"<p />");
}

void WikiMarkupParser::HandleInternalLink(const wchar_t* linkText)
{
	bool valid = true;
	
	if ( linkText==NULL )
		return;
	
	wchar_t link[wcslen(linkText)+1];
	wcscpy(link, linkText);
	wchar_t* linkDescription = wcsstr(link, L"|");
	if ( linkDescription ) 
		*linkDescription++ = 0x0;
	else
		linkDescription = link;
	
	int count = 0;
	wchar_t* pLink = link;
	while ( *pLink )
	{
		switch ( *pLink++ )
		{
			case L'&':
				count += 9;
				break;
				
			default:
				count++;
		}
	}
		
	pLink = link;
	wchar_t expandedLink[count+1];
	if ( wcslen(link)!=count )
	{
		wchar_t * pExpandedLink = expandedLink;
		while ( *pLink )
		{
			switch ( *pLink )
			{
				case L'&':
					*pExpandedLink++ = L'&';
					*pExpandedLink++ = L'a';
					*pExpandedLink++ = L'm';
					*pExpandedLink++ = L'p';
					*pExpandedLink++ = L';';
					*pExpandedLink++ = L'a';
					*pExpandedLink++ = L'm';
					*pExpandedLink++ = L'p';
					*pExpandedLink++ = L';';
					break;
					
				default:
					*pExpandedLink++ = *pLink;
					break;
			}
			
			pLink++;
		}
		
		*pExpandedLink = 0x0;
		pLink = expandedLink;
	}
		
	bool hasPrefix = false;
	wchar_t* pos = NULL;
	if ( (pos=wcsstr(link, L":"))>link ) 
	{
		hasPrefix = true;
		
		wchar_t special[pos-link+1];
		wcsncpy(special, link, pos-link);
		special[pos-link] = 0x0;
		
		wchar_t lowerSpecial[pos-link+1];
		wchar_t* src = special;
		wchar_t* dest = lowerSpecial;
		while ( *src )
			*dest++ = tolower(*src++);
		*dest = 0x0;
			
		// skip some "special" links
		if ( !wcscmp(lowerSpecial, L"kategorie") || !wcscmp(lowerSpecial, L"category") ) 
		{
			const wchar_t* categoryName = pos + 1;
			if ( categoryName )
			{
				if ( !_categories )
				{
					_categories = (wchar_t*) malloc((wcslen(categoryName) + 1)*sizeof(wchar_t));
					wcscpy(_categories, categoryName);
				}
				else
				{
					_categories = (wchar_t*) realloc(_categories, (wcslen(_categories) + 3 + wcslen(categoryName) + 1)*sizeof(wchar_t));
					wcscat(_categories, L" | ");
					wcscat(_categories, categoryName);
				}
			}
			return;
		} 
		else if (!wcscmp(lowerSpecial, _imageNamespace) || !wcscmp(lowerSpecial, L"image") )
		{
			wchar_t* imageFilename = wstrdup(pos + 1);
			trim(imageFilename);

			// get a pointer to the last real "|" (the description):
			wchar_t* imageDescription = L"";
			
			wchar_t** params = split(linkDescription, L'|');
			
			bool thumb = false;
			bool frame = false;
			bool border = false;
			
			int position = -1; // -1=not set, 0=none, 1=left, 2=center, 3=right
			
			int width = 0;
			int height = 0;
			
			int i = 0;
			while ( params[i] )
			{
				if ( *params[i] )
				{
					if ( !wcscmp(params[i], L"thumb") || !wcscmp(params[i], L"thumbnail") )
						thumb = true;
					else if ( !wcscmp(params[i], L"frame") )
						frame = true;
					else if ( !wcscmp(params[i], L"border") )
						border = true;
					else if ( !wcscmp(params[i], L"none") )
						position = 0;
					else if ( !wcscmp(params[i], L"left") )
						position = 1;
					else if ( !wcscmp(params[i], L"center") )
						position = 2;
					else if ( !wcscmp(params[i], L"right") )
						position = 3;
					else if ( !GetPixelUnit(params[i], &width, &height) ) 
						imageDescription = params[i];
				}
				
				i++;
			}
			
			trim(imageDescription);
			
			if ( !_imagesInstalled )
			{
				if ( !thumb || !*imageDescription )
				{
					// skip image code because we don't have images or an description
					free_split_result(params);
					free(imageFilename);
				
					return;
				}
				
				if ( width>180 )
					width = 180;
			}
			
			wchar_t* cssClass = NULL;
			
			wchar_t buffer[64];
			
			if ( frame )
				cssClass = L"thumbImage";
			else if ( thumb )
			{
				cssClass = L"thumbborder";
				if ( position<0 )
					position = 3;
			}
			else if ( border )
				cssClass = L"thumbborder";
			
			// build the image tag
			wchar_t imageTag[1024];
			
			// swprintf has problems with %S and chars > 255 so don't use it here
			wcscpy(imageTag, L"<img alt=\"");
			wcscat(imageTag, imageDescription);
			wcscat(imageTag, L"\" src=\"./Image:");
			wcscat(imageTag, imageFilename);
			wcscat(imageTag, L"\"");
			
			/*
			if ( width || height )
			{
				wcscat(imageTag, L" style=\"");
				if ( width )
				{
					swprintf(buffer, 64, L"max-width:%ipx;", width);
					wcscat(imageTag, buffer);
				}
			
				if ( height )
				{
					swprintf(buffer, 64, L"max-height:%ipx;", height);
					Append(buffer);
				}
				wcscat(imageTag, L"\"");
			}
			 */
			
			if ( true && !frame )
			{
				// Upscaling on for thumbs, it's not working for frame
				if ( width )
				{
					swprintf(buffer, 64, L" width=\"%ipx\"", width);
					wcscat(imageTag, buffer);
				}
				else
				{
					/* experimental */
					width = 180;
					swprintf(buffer, 64, L" width=\"%ipx\"", width);
					wcscat(imageTag, buffer);
				}
					
				if ( height )
				{
					swprintf(buffer, 64, L" height=\"%ipx\"", width);
					wcscat(imageTag, buffer);
				}
			}	
			if ( cssClass )
			{
				swprintf(buffer, 64, L" class=\"%S\"", cssClass);
				wcscat(imageTag, buffer);
			}	
			wcscat(imageTag, L" border=\"0\" />");
						
			if ( frame || thumb )
			{
				switch (position)
				{
					case 0:
						cssClass = L"thumb tnone";
						break;
						
					case 1:
						cssClass = L"thumb tleft";
						break;
						
					case 2:
						cssClass = L"thumb tnone";
						break;
						
					case 3:
						cssClass = L"thumb tright";
						break;
						
				}
				
				if ( position==2 )
					Append(L"<center>\r\n");
				
				Append(L"<div class=\"");
				Append(cssClass);
				Append(L"\">\r\n");

				Append(L"<div class=\"thumbinner\" style=\"padding: 2px;\">");
				Append(imageTag);

				int tcwidth = 120;
				if ( width>tcwidth )
					tcwidth = width;

				swprintf(buffer, 64, L"<div class=\"thumbcaption\" style=\"max-width:%ipx;\">", tcwidth);
				Append(buffer);
				
				if ( *imageDescription ) 
				{
					WikiMarkupParser wikiMarkupParser(_languageCodeW, _pageName, false);
					wikiMarkupParser.SetInput(imageDescription);
					wikiMarkupParser.SetRecursionCount(_recursionCount);
					wikiMarkupParser.Parse();
					
					AppendHtml(wikiMarkupParser.GetOutput());
				}
				
				Append(L"</div>\r\n</div>\r\n</div>\r\n");

				if ( position==2 )
					Append(L"</center>\r\n");
			}
			else if ( position<0 )
			{
				Append(imageTag);
			}
			else {
				switch (position)
				{					
					case 0: // none
						cssClass = L"floatnone";
						break;
							
					case 1: // left
						cssClass = L"floatleft";
						break;
						
					case 2: // center
						cssClass = L"floatnone";
						break;
						
					case 3: // right
						cssClass = L"floatright";
						break;
						
				}
				
				if ( position==2 )
					Append(L"<div class=\"center\">\r\n");
				
				Append(L"<div class=\"");
				Append(cssClass);
				Append(L"\"><span>");
				Append(imageTag);
				Append(L"</span></div>");

				if ( position==2 )
					Append(L"</div>\r\n");
			}
			
			free_split_result(params);
			free(imageFilename);

			return;
		}
		else if ( !wcscmp(lowerSpecial, L"wikipedia") )
		{
			Append(linkDescription);
			return;
		}
		else if ( !wcscmp(lowerSpecial, L"media") )
		{
			Append(linkDescription);
			return;
		}
		else if ( !settings.IsLanguageInstalled(CPPStringUtils::to_string(wstring(lowerSpecial))) )
		{
			// Not installed laguages will not be become a link
			return;
		}
	}
	else
	{
		/*
		TitleIndex* titleIndex = __settings->GetTitleIndex(CPPStringUtils::to_string(_languageCodeW));
		ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(CPPStringUtils::to_utf8(wstring(link)), false);
																		   
		if ( articleSearchResult )
			titleIndex->DeleteSearchResult(articleSearchResult);
		else
			valid = false; 
		*/
		
		/*
		 that takes to long, a binary search should be a lot faster
		WikiMarkupGetter wikiMarkupGetter;
		string name = wikiMarkupGetter.FindArticle(CPPStringUtils::to_utf8(wstring(link)));
		valid = !name.empty();
		 */
	}
	
	if ( link!=linkDescription ) 
	{		
		WikiMarkupParser wikiMarkupParser(_languageCodeW, _pageName, false);
		wikiMarkupParser.SetInput(linkDescription);
		wikiMarkupParser.SetRecursionCount(_recursionCount);
		wikiMarkupParser.Parse();

		Append(L"<a href=\"");

		if ( hasPrefix )
			Append(L"../");
		/*
		 // Append(L"./");
		if ( !hasPrefix ) 
		{
			Append(_languageCodeW);
			Append(L":");
		}
		 */
		Append(pLink);
		
		if ( valid )
			Append(L"\" class=\"wkInternalLink\">");
		else
			Append(L"\" class=\"wkInternalLinkNotExisting\">");			
		AppendHtml(wikiMarkupParser.GetOutput());

		// ok, ok, if bold/italic is set this fails
		while ( isintalpha(Peek()) ) 
			Append(GetNextChar());
		 
		Append(L"</a>");
	}
	else 
	{
		Append(L"<a href=\"");
		if ( hasPrefix )
			Append(L"../");

		/*
		Append(L"<./");
		if ( !hasPrefix ) 
		{
			Append(_languageCodeW);
			Append(L":");
		}
		 */
		Append(pLink);
		if ( valid )
			Append(L"\" class=\"wkInternalLink\">");
		else
			Append(L"\" class=\"wkInternalLinkNotExisting\">");			
		Append(link);

		DBH Test (link);
		
		while ( isintalpha(Peek()) ) 
			Append(GetNextChar());
		
		Append(L"</a>");
	}
}

void WikiMarkupParser::HandleExternalLink(const wchar_t* linkText)
{
	if ( linkText==NULL )
		return;

	wchar_t* link = wstrdup(linkText);
	wchar_t* linkDescription = wcsstr(link, L" ");
	if ( linkDescription ) 
	{
		*linkDescription++ = 0x0;

		WikiMarkupParser wikiMarkupParser(_languageCodeW, _pageName, false);
		wikiMarkupParser.SetInput(linkDescription);
		wikiMarkupParser.SetRecursionCount(_recursionCount);

		wikiMarkupParser.Parse();

		Append(L"<a href=\"");
		Append(link);
		Append(L"\" class=\"wkExternalLink external\" target=\"_blank\">");
		AppendHtml(linkDescription);
		Append(L"</a>");		
	}
	else
	{
		_externalLinkNo++;
		wchar_t description[16];
		swprintf(description, 16, L"[%i]", _externalLinkNo);

		Append(L"<a href=\"");
		Append(link);
		Append(L"\" class=\"wkExternalLink external\"  target=\"_blank\">");
		Append(description);
		Append(L"</a>");
	}
	
	free(link);
}

void WikiMarkupParser::HandleHeadline(const wchar_t* headlineText, int level)
{
	CloseOpenWikiTags();

	if ( _tocPosition<0 )
		_tocPosition = (_pCurrentOutput - _pOutput);
	
	WikiMarkupParser wikiMarkupParser(_languageCodeW, _pageName, false);
	wikiMarkupParser.SetInput(headlineText);
	wikiMarkupParser.SetRecursionCount(_recursionCount);
	wikiMarkupParser.Parse();
	
	// create a TOC entry
	TOC* toc = new TOC();
	toc->name = wstrdup(headlineText);
	toc->level = level;
	toc->next = NULL;
	
	if ( _toc )
	{
		TOC* help = (TOC*) _toc;
		while ( help->next ) 
			help = help->next;
		
		help->next = toc;
	}
	else 
		_toc = toc;	
	
	wstring ancorName = wstring(headlineText);
	size_t pos;
	while ( (pos=ancorName.find(L" "))!=string::npos )
		ancorName.replace(pos, 1, L"_", 1);
	ancorName = CPPStringUtils::url_encode(ancorName);
	
	int bufferSize = 128;
	wchar_t* buffer = (wchar_t*) malloc(bufferSize * sizeof(wchar_t));	
	
	Append(L"<a name=\"");
	Append(ancorName.c_str());
	Append(L"\"></a>");
	
	Append(L"<H");
	swprintf(buffer, bufferSize, L"%i", level+1);
	Append(buffer);
	Append(L" class=\"wkHeadline wkHeadline");
	Append(buffer);
	Append(L"\">");
	AppendHtml(wikiMarkupParser.GetOutput());
	Append(L"</H");
	Append(buffer);
	Append(L">");
	
	free(buffer);
	
	// every headline is followed by a paragraph, so another is not necessary
	_newLine = 2;
}

wchar_t* WikiMarkupParser::GetParams(bool stopAtExclamtionAlso)
{
	int count = 0;
	
	int curlyBrakets = 0;
	
	wchar_t c;
	while ( c=Peek(count) )
	{
		if ( !curlyBrakets )
		{
			if ( c=='\n' || c=='[' )
				return NULL;
			else if (c=='|') 
			{
				if ( Peek(count+1)=='|' )
					return NULL;
				else
					break;
			}
			else if ( stopAtExclamtionAlso && c=='!' )
			{
				if ( Peek(count+1)=='!' )
					return NULL;
			}
		}
		
		if ( c==L'{' )
			curlyBrakets++;
		else if ( c==L'}' )
			curlyBrakets--;
		
		count++;
	}
	
	wchar_t* params = (wchar_t*) malloc( (count + 1) * sizeof(wchar_t) );
	wchar_t* dest = params;
	while ( count-- ) 
		*dest++ = GetNextChar();
	*dest = 0x0;
	
	// this is the divider
	Eat(1);
		
	return params;
}
		
void WikiMarkupParser::Parse() 
{
	// StopWatch("Parsing");
	if ( _doExpandTemplates )
	{		
		// wprintf(L"%S\r\n", _pInput);

		wchar_t* newInput = ExpandTemplates(_pInput);
		
		if ( newInput!=_pInput )
		{
			free(_pInput);
			
			_pInput = newInput;
			// unstrip2
			wchar_t *newInput2 = unstrip2(_pInput);
			if(newInput2!=_pInput)
			{
				free(_pInput);
				_pInput=newInput2;
			}
			_pCurrentInput = _pInput;
			_inputLength = wcslen(_pInput);
		}
		
		// wprintf(L"\r\n%S", _pInput);
	}
	
	_pCurrentInput = _pInput;
	
	_pCurrentOutput = _pOutput;
	_iOutputRemain = _iOutputSize;

	_newLine = 1;
	
	_tocPosition = -1;
	_noToc = false;
	_forceToc = false;
	
	_orderedEnumeration = 0;
	_unorderedEnumeration = 0;
	_definitionList = 0;

	_italic = -1;
	_bold = -1;
	
	_externalLinkNo = 0;
	
	_stop = false;
		
	wchar_t c;
	while ( (c=GetNextChar())!=0x0 ) 
	{
		bool handled = false;
	
		if ( _newLine ) 
		{
			// start of a new line, remove spaces
			while ( c==0x20 )
				c = GetNextChar();
			
			// close any unordered enumeration
			if ( c!='*') 
			{
				while ( _unorderedEnumeration ) {
					Append(L"</li></ul>");
					_unorderedEnumeration--;
				}
			}
			// close any ordered enumeration
			if ( c!='#') 
			{
				while ( _orderedEnumeration ) {
					Append(L"</li></ol>");
					_orderedEnumeration--;
				}
			}
			
			if ( c!=';' && c!=':' )
			{
				if ( _definitionList )
				{
					if ( _definitionList==2 )
						Append(L"</dd>\r\n");
					else if ( _definitionList==1 )
						Append(L"</dt>\r\n");
				
					Append(L"</dl>\r\n");
					
					_definitionList = 0;
				}
			}
			
			if ( c!='\n' )
				_newLine = 0;
			
			// check for some special chars at the start of the line
			switch (c)
			{
				case '\n':
					// paragraph ?
					if ( _newLine==1 )
						HandleParagraph();					
					break;
					
				case '=':
					if (Peek() == '=')
					{
						// Headline, let's see at which level
						int level = 1;
						while (Peek(level) == '=')
							level++;
						Eat(level);
						
						wchar_t* line = GetNextLine();
						wchar_t* help = line;
						
						// find level number of = chars
						int count = 0;
						while ( *help ) 
						{
							if ( *help=='=' ) 
							{
								count++;
								if ( count == (level+1) ) 
								{
									// cut at the first = chars
									help -= level;
									*help = 0x0;
									break;
								}
							}
							else
								count = 0;
							help++;
						}
						trim(line);

						HandleHeadline(line, level);
						free(line);
						handled = true;
					}
					break;
										
				case '*':
				{
					int count = 1;
					int i = 0;
					wchar_t c;
					while ( c=Peek(i) )
					{
						if ( c=='*' )
							count++;
						else if ( c!=' ' )
							break;
						i++;
					}
					
					if ( count>_unorderedEnumeration )
					{
						while ( _unorderedEnumeration<count )
						{
							Append(L"<ul class=\"wkUnorderedList\">");
							_unorderedEnumeration++;
							if ( _unorderedEnumeration<count )
							{
								Append(L"<li class=\"wkUnorderedListItem wkUnorderedListItem");
								wchar_t buffer[16];
								swprintf(buffer, 16, L"%i", _unorderedEnumeration);
								Append(buffer);
								Append(L"\">");
							}
						}
					}
					else if ( _unorderedEnumeration>count )
					{
						while ( _unorderedEnumeration>count ) 
						{
							Append(L"</li></ul>");
							_unorderedEnumeration--;
						}
					} 
					else
						Append(L"</li>");
					
					Append(L"<li class=\"wkUnorderedListItem wkUnorderedListItem");
					wchar_t buffer[16];
					swprintf(buffer, 16, L"%i", _unorderedEnumeration);
					Append(buffer);
					Append(L"\">");
					
					// skip the asterisk
					Eat(i);

					handled = true;
					break;
				}
					
				case '#':
				{
					int count = 1;
					int i = 0;
					wchar_t c;
					while ( c=Peek(i) )
					{
						if ( c=='#' )
							count++;
						else if ( c!=' ' )
							break;
						i++;
					}
					
					if ( count>_orderedEnumeration )
					{
						while ( _orderedEnumeration<count )
						{
							Append(L"<ol class=\"wkOrrderedList\">");
							_orderedEnumeration++;
							if ( _orderedEnumeration<count )
							{
								Append(L"<li class=\"wkOrderedListItem wkOrderedListItem");
								wchar_t buffer[16];
								swprintf(buffer, 16, L"%i", _orderedEnumeration);
								Append(buffer);
								Append(L"\">");
							}
						}
					}
					else if ( _orderedEnumeration>count )
					{
						while ( _orderedEnumeration>count ) 
						{
							Append(L"</li></ol>");
							_orderedEnumeration--;
						}
					} 
					else
						Append(L"</li>");
					
					Append(L"<li class=\"wkOrderedListItem wkOrderedListItem");
					wchar_t buffer[16];
					swprintf(buffer, 16, L"%i", _orderedEnumeration);
					Append(buffer);
					Append(L"\">");
					
					// skip the number signs
					Eat(i);
					
					handled = true;
					break;
				}

				case ';' :
				{
					if ( _definitionList==0 )
						Append(L"<dl class=\"wkDefintionList\">\r\n");
					else if ( _definitionList == 1 )
						Append(L"</dt>\r\n");
					else if ( _definitionList == 2 )
						Append(L"</dd>\r\n");
					
					Append(L"<dt class=\"wkDefinitionListTitle\">");
					_definitionList = 1;
					
					handled = true;
					break;
				}
					
				case ':' :
				{
					if ( _definitionList==0 )
						Append(L"<dl class=\"wkDefintionList\">\r\n");					
					else if ( _definitionList == 1 )
						Append(L"</dt>\r\n");
					else if ( _definitionList == 2 )
						Append(L"</dd>\r\n");
					
					Append(L"<dd class=\"wkDefinitionListEntry\">");
					_definitionList = 2;
					
					handled = true;
					break;
				}
					
				case '{':
				{
					if ( Peek()!='|' )
						break;
					
					Eat(1);
					
					// a table start tag
					wchar_t* line = GetNextLine();
					if ( !line )
						continue;
					
					trim(line);
					if ( *line ) 
					{
						// Kill trainling pipes
						wchar_t* pos = wcsstr(line, L"|");
						if ( pos )
							*pos = 0x0;
						
						Append(L"<table ");
						AppendHtml(line);
						Append(L">\n");
					}
					else
						Append(L"<table>\n");

					PushTag(L"table", false);
					
					free(line);

					handled = true;
					break;
				}
					
				case '|':
				{
					if ( Peek()=='}' ) 
					{
						// end of table
						Eat(1);
						if ( TopTagIs(L"td") )
						{
							Append(L"</td>\n");
							PopTag(L"td", false);
						}
						if ( TopTagIs(L"tr") )
						{
							Append(L"</tr>\n");
							PopTag(L"tr", false);
						}
						if ( TopTagIs(L"table") ) {
							Append(L"</table>\n");
							PopTag(L"table", false);
						}

						handled = true;
						break;
					}
					else if ( Peek()=='+' )
					{
						// Kopf der Tabelle
						wchar_t* line = GetNextLine();
						if ( !line )
							continue;
						
						trim(line);

						// Ignorieren
						free(line);

						handled = true;
						break;
					}
					else if ( Peek()=='-' )
					{
						// Neue Zeile der Tabelle
						
						Eat(1);
						wchar_t* line = GetNextLine();
						if ( !line )
							continue;
						
						if ( TopTagIs(L"td") )
						{
							Append(L"</td>\n");
							PopTag(L"td", false);
						}
						if ( TopTagIs(L"tr") )
						{
							Append(L"</tr>\n");
							PopTag(L"tr", false);
						}
						
						trim(line);
						if ( *line ) 
						{
							Append(L"<tr ");
							Append(line);
							Append(L">\n");
						}
						else
							Append(L"<tr>\n");
						
						PushTag(L"tr", false);
						
						free(line);

						handled = true;
						break;
					}
					else 
					{
						// Tabellenzellen
						if ( TopTagIs(L"td") )
						{
							Append(L"</td>");
							PopTag(L"td", false);
						}
						if ( !TopTagIs(L"tr") ) 
						{
							PushTag(L"tr", false);
							Append(L"<tr>\n");
						}
									
						wchar_t* params = GetParams(false);
						if ( params ) 
						{
							Append(L"<td ");
							Append(params);
							Append(L">");
						
							free(params);
						}
						else
							Append(L"<td>");
						
						PushTag(L"td", false);
						
						handled = true;
						break;
					}
				}

				case '!':
				{
					// Tabellenkopf
					if ( TopTagIs(L"td") )
					{
						Append(L"</td>\n");
						PopTag(L"td", false);
					}
					if ( !TopTagIs(L"tr") ) 
					{
						PushTag(L"tr", false);
						Append(L"<tr>\n");
					}
					
					wchar_t* params = GetParams(true);
					DBH Params(params);
					if ( params ) 
					{
						Append(L"<td ");
						Append(params);
						Append(L">");
						
						free(params);
					}
					else
						Append(L"<td>");
					
					PushTag(L"td", false);
					
					handled = true;
					break;
				}
			}
		}
		
		if ( !handled )
		{
			switch (c)
			{
				case '\n':
					_newLine++;
					break;
					
				case '\'':
				{
					int count = 1;
					if ( Peek(0)=='\'' ) {
						count = 2;
						if ( Peek(1)=='\'' )
							count = 3;
					}

					if ( count==1 ) {
						Append('\'');
					}
					else if (count == 2) {
						// italic, start or end?
						if ( _italic<0 ) {
							// start
							_italic = (_pCurrentOutput - _pOutput);
							Append(L"<i>");
						}
						else {
							// end, but was there an opening bold tag after the italic one?
							if ( _bold>_italic ) {
								// yes, close and reopen it
								Append(L"</b></i>");
								_bold = (_pCurrentOutput - _pOutput);
								Append(L"<b>");
								_italic = -1;
							}
							else {
								Append(L"</i>");
								_italic = -1;
							}
						}
						Eat(1);
					}
					else {
						// bold, start or end?
						if ( _bold<0 ) {
							// start
							_bold = (_pCurrentOutput - _pOutput);
							Append(L"<b>");
						}
						else {
							// end, but was there an opening italic tag after the bold one?
							if ( _italic>_bold ) {
								// yes, close and reopen it
								Append(L"</i></b>");
								_italic = (_pCurrentOutput - _pOutput);
								Append(L"<i>");
								_bold = -1;
							}
							else {
								Append(L"</b>");
								_bold = -1;
							}
						}
						Eat(2);
					}
					
					break;
				}
			
				case '<':
				{
					if ( Peek()!='/' && !isalpha(Peek()) )
					{
						Append(L"<");
						break;
					}
					
					// get the tag name and type
					int count = 0;
					bool endTag = (Peek(count) == '/');
					if (endTag)
						count++;

					wchar_t tagName[512];
					wchar_t* pTagName = tagName;
					int countTagChars = 0;
					
					wchar_t attributes[512];
					wchar_t* pAttributes = attributes;
					int countAttributChars = 0;
					
					bool inTagName = true;
					while (Peek(count)!='>' && Peek(count) != '\0')
					{
						wchar_t help = Peek(count);

						if ( !isalnum(help) )
							inTagName = false;

						if (inTagName) 
						{
							if ( countTagChars++<511 )
								*pTagName++ = _to_wlower(help);
						}
						else {
							if ( countAttributChars++<511 )
								*pAttributes++ = help;
						}
						count++;
					}
					*pTagName = 0x0;
					*pAttributes = 0x0;
					
					// check for illegal tags
					if ( (count==0) || (Peek(count)!='>') )
					{
						Append(L"<");
						break;
					}
						
					bool emptyTag = (Peek(count-1)=='/');

					int tagType = 0;
					if ( (tagType=IsWikiTag(tagName)) ) 
					{
						// take the chars out 
						Eat(count + 1);
						
						switch ( tagType )
						{
							case 1: // nowiki
								if ( !endTag && !emptyTag ) 
									ParseNoWikiArea(tagType);
								break;							
							case 2: // pre
								if ( !endTag && !emptyTag ){ 
									Append(L"<pre");
									if (attributes[0])
										Append(attributes);
									Append(L">");
									ParseNoWikiArea(tagType);
									Append(L"</pre>");
								}
								break;							

							case 3: // source
								if ( !endTag && !emptyTag )
								{
									// remove the first line
									if ( Peek()==L'\n' )
										Eat(1);
									
									ParseNoWikiArea(tagType);
									_newLine = 1;
								}
								break;
								
							case 4: // imagemap
								if ( !endTag && !emptyTag )
								{
									wchar_t* params = GetTextUntilNextTag();
									if ( params )
										free(params);
								}
								break;
								
							case 5: // code
								if ( !emptyTag )
								{
									if ( !endTag )
										Append(L"<tt>");
									else
										Append(L"</tt>");
								}
								break;
								
							case 6: // ref
								if ( !endTag && !emptyTag )
								{
									wchar_t* start = _pCurrentInput;
									//wchar_t* params = GetTextUntilNextTag();
									wchar_t* params = GetTextUntilEndOfTag(L"ref");
									
									if ( params && *params )
									{
										int count = 1;

										REF* ref = new REF();
										ref->start = start;
										ref->length = wcslen(params);
										ref->next = NULL;
										
										if ( _references )
										{
											count++;
											
											REF* help = (REF*) _references;
											while ( help->next ) 
											{
												count++;
												help = help->next;
											}
											
											help->next = ref;
										}
										else
											_references = ref;
											
										free(params);
										wchar_t number[16];
										swprintf(number, 16, L"%i", count);
										Append(L"<sup><a href=\"#_note-");
										Append(number);
										Append(L"\" class=\"wbReference\" name=\"_ref-");
										Append(number);
										Append(L"\">[");
										Append(number);
										Append(L"]</a></sup>");
									}
								}
								break;
								
							case 7: // references
								if ( !endTag || emptyTag )
									InsertReferences();
								break;
									
							case 8: // math
								if ( !endTag || emptyTag )
								{
								//#ifdef MATH_SUPPORT
									//Hashing Math with MD5
									/*
									CC_LONG length;
									wchar_t* mathwch0; 
									wchar_t* mathwch;
									wchar_t* pwchr;
									wchar_t* raw=(wchar_t*)malloc(1024);

									mathwch=GetTextUntilNextTag();
									mathwch0=mathwch;
									raw=wcscpy(raw, mathwch);


									//Remove leading space
									while((*mathwch == L' ')||(*mathwch == L'\n')) mathwch ++;
									//Remove space from the end, if any.
									pwchr = mathwch + wcslen(mathwch) -1;
									while((*pwchr == L' ')||(*pwchr==L'\n')) {*pwchr = L'\0'; pwchr --;};

									

									string mathutf8 = CPPStringUtils::to_utf8(wstring(mathwch));
									
									length = mathutf8.size();
									unsigned char *md5 =(unsigned char*) malloc(18*(sizeof(unsigned char)));
									CC_MD5((void*)(mathutf8.data()),length,md5);
									//md5[16]=0x0;
									//Append(mathwch);
									wchar_t *md5wstr=(wchar_t *)malloc(64*sizeof(wchar_t));
									wchar_t *md5imgtags=(wchar_t *)malloc((128)*sizeof(wchar_t));
									swprintf(md5wstr,(size_t)64,L"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
										md5[0],md5[1],md5[2],md5[3],
										md5[4],md5[5],md5[6],md5[7],
										md5[8],md5[9],md5[10],md5[11],
										md5[12],md5[13],md5[14],md5[15]);
									swprintf(md5imgtags,(size_t)128,L"<img src=\"./math:%S.png\" />",md5wstr);
									//Append((CPPStringUtils::to_wstring(std::string(md5str))).data());
									Append(L"<!-- raw:");
									Append(raw);
									//Append(L":alt:");
									//Append(mathwch);
									Append(L":-->");
									Append(md5imgtags);
									//Append(L" alt=\"");
									//Append(mathwch);
									//Append(L"\" />");
									
									free(md5);
									free(md5wstr);
									free(md5imgtags);
									free(raw);
									free(mathwch0);
									*/
									if(!settings.IsJSMathEnabled())
									{
									CC_LONG length;
									wchar_t* mathwch; 
									wchar_t* pwchr;
									wchar_t* raw=(wchar_t*)malloc(2048);
									wchar_t* alt=(wchar_t*)malloc(2048);

									mathwch = GetTextUntilNextTag();
									wcsncpy(raw, mathwch,2047);
									wcsncpy(alt, mathwch,2047);

									pwchr = mathwch;

									//Remove leading space
									//while((*mathwch == L' ')||(*mathwch == L'\n' )) mathwch ++;
									while((*alt == L' ')||(*alt == L'\n' )) alt ++;
									//Remove space from the end, if any.
									pwchr = alt + wcslen(alt) -1;
									while((*pwchr == L' ')||(*pwchr == L'\n')) {*pwchr = L'\0'; pwchr --;};

									
									string mathutf8 = CPPStringUtils::to_utf8(wstring(alt));
									//mathutf8 = "&lt;math&gt;"+mathutf8+"&lt;\\math&gt;";
									
									length = mathutf8.size();
									unsigned char *md5 =(unsigned char*) malloc(18*(sizeof(unsigned char)));
									CC_MD5((void*)(mathutf8.data()),length,md5);
									wstring formatted_math= CPPStringUtils::js_format(wstring(alt));
									wchar_t *md5wstr=(wchar_t *)malloc(64*sizeof(wchar_t));
									wchar_t *md5imgtags=(wchar_t *)malloc((160+formatted_math.size())*sizeof(wchar_t));
									//wchar_t *md5imgtags=(wchar_t *)malloc(64*sizeof(wchar_t));
									swprintf(md5wstr,(size_t)64,L"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
										md5[0],md5[1],md5[2],md5[3],
										md5[4],md5[5],md5[6],md5[7],
										md5[8],md5[9],md5[10],md5[11],
										md5[12],md5[13],md5[14],md5[15]);
									//swprintf(md5imgtags,(size_t)64,L"<img src=\"./math:%S.png\" />",md5wstr);

									swprintf(md5imgtags,(size_t)(160+formatted_math.size()),L"<img onerror=\"mathImgAlternative('%S',this);\" src=\"./math:%S.png\"/>\0",formatted_math.c_str(),md5wstr);
									//Append((CPPStringUtils::to_wstring(std::string(md5str))).data());
									Append(L"<!-- raw:");
									Append(raw);
									Append(L":alt:");
									Append(alt);
									Append(L":-->");
									Append(md5imgtags);

									free(md5);
									free(md5wstr);
									free(md5imgtags);
									free(raw);
									free(alt);
									} // if(!IsJSMathEnabled)
									else
									{
										wchar_t *mathcontent=GetTextUntilEndOfTag(L"math");
										bool isSimpleMath=false;
										if(!wcschr(mathcontent,L'\\')){
											Append(L"<span class='math'>");
											isSimpleMath=true;
										}
										else{
											Append(L"<span class='premath' onclick='javascript:renderMath(this);'>");
										}
										wchar_t *pMathcontent=mathcontent;
										wchar_t c;
										while(c=*pMathcontent++){
											switch(c){
											case L'<':
												Append(L"&lt;");
												break;
											case L'>':
												Append(L"&gt;");
												break;
											case L'&':
												Append(L"&amp;");
												break;
											default:
												Append(c);
												break;
											}
										}
										Append((isSimpleMath?L"</span>":L"<span style='color:red;'>(click to show maths...)</span></span>"));
									}
								//#else
								//	Append(GetTextUntilNextTag());
								//#endif //MATH_SUPPORT
								}
								break;
							default:
								break;
								
						} // end of switch tag type
					} // is wiki tag
					else 
					{
						// do nothing with this tag, simply append it
						Append(L"<");
					
						// add the trailing ">" also
						count++;
						while (count--)
							Append(GetNextChar());
					}
					break;
				}
		
				case '[':
				{
					// this is a link
					wchar_t* linkText = NULL;
					
					if ( Peek()=='[' ) 
					{
						linkText = GetTextInDoubleBrakets(L'[', L']');
						HandleInternalLink(linkText);
					}
					else if ( startsWithCase(_pCurrentInput, L"http://") || startsWithCase(_pCurrentInput, L"https://") || startsWithCase(_pCurrentInput, L"gopher://") || 
							  startsWithCase(_pCurrentInput, L"mailto:") || startsWithCase(_pCurrentInput, L"news://") || startsWithCase(_pCurrentInput, L"ftp://") ||
							  startsWithCase(_pCurrentInput, L"irc://") ) 
					{
						// truely a link
						linkText = GetTextInSingleBrakets(L'[', L']');
						HandleExternalLink(linkText);
					}
					else
						// forget the link
						Append(L'[');
					
					// cleanup
					if ( linkText )
						free(linkText);

					break;
				}
					
				case '_':
				{
					if ( Peek()!='_' )
					{
						Append(L'_');
						break;
					}
					
					int countUnderscore = 2;
					int count = 1;
					
					wchar_t c;
					while ( (c=Peek(count)) && countUnderscore )
					{
						if ( c=='_' )
						   countUnderscore--;
						else if ( c<'A' || c>L'Z' )
							break;
						
						count++;
					}
					
					int length = count - 3;
					if ( countUnderscore || length<=0 )
						break;
					
					wchar_t buffer[length+1];
					Eat(1);
					count = 0;
					while ( count<length )
						buffer[count++] = GetNextChar();
					Eat(2);
					buffer[length] = 0x0;
					
					DBH Buffer(buffer);
					
					if ( !wcscmp(buffer, L"NOTOC") )
					{
						if ( !_forceToc )
							_noToc = true;
					}
					else if (!wcscmp(buffer, L"FORCETOC") )
					{
						_forceToc = true;
						_noToc = false;
					}
					else if (!wcscmp(buffer, L"TOC") )
					{
						_tocPosition = (_pCurrentOutput - _pOutput);
						_forceToc = true;
						_noToc = false;
					}
					
					break;
				}
				
				case '|':
				{
					if ( Peek()!='|' )
					{
						Append(L'|');
						break;
					}
					else
						Eat(1);
						
					if ( Peek()=='\n' ) 
					{
						// empty cell will not be not produced
						break;
					}
					
					if ( TopTagIs(L"td") )
					{
						Append(L"</td>\n");
						PopTag(L"td", false);
					}
					if ( !TopTagIs(L"tr") )
					{
						PushTag(L"tr", false);
						Append(L"<tr>\n");
					}
					
					wchar_t* params = GetParams(false);
					if ( params ) 
					{
						Append(L"<td ");
						Append(params);
						Append(L">");
						
						free(params);
					}
					else
						Append(L"<td>");
					
					PushTag(L"td", false);
					break;
				}

				case '!':
				{
					if ( Peek()!='!' || !TopTagIs(L"td") )
					{
						Append(L'|');
						break;
					}
					else
						Eat(1);
					
					if ( TopTagIs(L"td") )
					{
						Append(L"</td>\n");
						PopTag(L"td", false);
					}
					if ( !TopTagIs(L"tr") )
					{
						PushTag(L"tr", false);
						Append(L"<tr>\n");
					}
					
					wchar_t* params = GetParams(true);
					if ( params ) 
					{
						Append(L"<td ");
						Append(params);
						Append(L">");
						
						free(params);
					}
					else
						Append(L"<td>");
										
					PushTag(L"td", false);
					break;
				}
					
				case ':':
				{
					if ( _definitionList==1 )
					{
						Append(L"</dt>\r\n<dd class=\"wkDefinitionListEntry\">");
						_definitionList = 2;
					}
					else
						Append(L":");
					break;
				}
#if 0
//#ifdef MATH_SUPPORT
				case '&':
				{
					int count=0;
					if (Peek()!='l')
					{
						Append(L"l");
						break;
					}
					else if(wcsncmp(_pCurrentInput,L"lt;math&gt;",11)==0)
					{
						count++;
						int offset=11;
						for(;offset<4095;offset++)
						{
							if(Peek(offset)=='&' && wcsncmp(_pCurrentInput+offset,L"&lg;\\math&gt;",13))
							{
								count--;

							}
						}
						break;

					}
					else 
					{
						break;
					}
					
				}
#endif //MATH_SUPPORT
		
				default:
					Append(c);
					break;
			} // case
		} // not handled
	} // Parse
	
	CloseOpenWikiTags();
	InsertCategories();
	Append(L"\r\n");
	
	InsertToc();
	
	// clean the toc
	while ( _toc )
	{
		TOC* toc = (TOC*) _toc;
		_toc = toc->next;
		
		if ( toc->name )
			free(toc->name);
		
		delete(toc);
	}	
	
	// clean the references
	while ( _references )
	{
		REF* ref = (REF*) _references;
		_references = ref->next;
		
		delete(ref);
	}	
	
	if ( _categories )
	{
		free(_categories);
		_categories = NULL;
	}	
}

// the "expand template" method has allready removed any comment, so don't deal with comments here
// The next char points to the first char after the opening tag
void WikiMarkupParser::ParseNoWikiArea(int tagType)
{
	wchar_t* startOfTagName = NULL;
	wchar_t* endOfTagName = NULL;
	wchar_t* startOfComment = NULL;
	int state = 0;
	int endTag = 0;
	
	wchar_t c;
	wchar_t lastChar = -1;
	// add <div> element for <source>
	if(tagType == 3){
		Append(L"<div style=\"wkSourceTag\"><pre>");
	}
	while ( !_stop && (c=GetNextChar()) )
	{
		switch ( state )
		{
			case 0:
				// in text
				if ( c=='<' ) 
				{
					if ( tagType == 3 ){
						if(Peek()!='/'){
							//normal text
							Append(L"&lt;");
							break;
						}
						else if(Peek(1)=='s' && Peek(2)=='o' && Peek(3)=='u'
							&& Peek(4)=='r' && Peek(5)=='c' && Peek(6)=='e' && Peek(7)=='>'){
							//end of tag:</source>
							if(tagType==3)
								Append(L"</pre></div>");
							Eat(8);
							return;
						}
						else{
							Append(c);
							break;
						}
					}
					lastChar = -1;
					
					if ( Peek()=='!' && Peek(1)=='-' && Peek(2)=='-' )
					{
						// this is a comment
						startOfComment = _pCurrentInput - 1;
						Eat(3);
						
						state = 2;
						break;
					}
					
					// this is a tag
					startOfTagName = _pCurrentInput;
					endOfTagName = NULL;
					
					endTag = 0;
					if ( Peek()=='/' ) 
					{
						endTag = 1;
						Eat(1);
						startOfTagName++;
					}	
					
					state = 1;
				}
				else
				{
					if ( tagType==1 ) // nowiki
					{
						if ( c==L' ' && lastChar!=L' ' )
							Append(c);
						else if ( c==L'\n' && lastChar!='\n' )
							Append(c);
						else
							Append(c);
					}
					else if ( tagType==3 ) // source
					{
						switch (c) 
						{
							case L' ':
								Append(L"&nbsp;");
								break;
								
							case L'\n':
								Append(L"<br />");
								break;
							case L'>':
								Append(L"&gt;");
								break;
							case L'&':
								Append(L"&amp;");
								break;	
							default: 
								Append(c);
								break;
						}
					}
					else
						Append(c);
				}
				
				lastChar = c;
				break;
				
				case 1:
				{
					// inside a tag
					if ( c=='>' ) 
					{						
						if ( !endOfTagName )
							endOfTagName = _pCurrentInput - 1;
											
						int length = endOfTagName - startOfTagName;
						if ( length>0 )
						{
							wchar_t tag[length+1];
							wcsncpy(tag, startOfTagName, length);
							to_lower(tag);
							tag[length] = 0x0;
							
							if ( endTag )
							{
								// is this the end tag of the requested tagType?
								if ( !wcscmp(tag, wikiTags[tagType]) )
								{
									// all done
									return;
								}
							}
							
							// as this is not our wanted stop tag put it to the output
							Append(L"&lt;");
							if ( endTag )
								Append(L"/");
							
							while ( startOfTagName<endOfTagName )
								Append(*startOfTagName++);
							Append(L"&gt;");
						}
						state = 0;
					}
					else if ( c==L' ' )
					{
						if ( !endOfTagName )
							endOfTagName = _pCurrentInput - 1;
					}
				}
				break;
				
				case 2:
				// inside a comment
				if ( c=='-' && Peek()=='-' && Peek(1)=='>' )
				{
					// end of comment
					Eat(2);
					// Hmm: if ( !nowiki && !pre ) 
					{
						ReplaceInput(NULL, (startOfComment-_pInput), (_pCurrentInput-startOfComment));
					}
					
					state = 0;
				}
				break;
		}
	}
}

void WikiMarkupParser::CloseOpenWikiTags()
{
	if ( _italic!=-1 )
	{
		if ( _bold<_italic ) {
			Append(L"</i>");
			_italic = -1;
		}
	}
	
	if ( _bold!=-1 )
		Append(L"</b>");
	
	if ( _italic!=-1 )
		Append(L"</i>");
}

void WikiMarkupParser::InsertToc()
{
	if ( _tocPosition<0 || _noToc )
		return;
	
	if ( !_toc )
		return;
	
	wstring tocTitle = CPPStringUtils::from_utf8w(_languageConfig->GetSetting("tocTitle", "Contents"));
	
	wstring toc = L"<p><table id=\"toc\" class=\"toc\" summary=\"" + tocTitle + L"\">\r\n";
	toc += L"<tr><td><div id=\"toctitle\"><h2>"  + tocTitle + L"</h2></div>\r\n";
	
	int level = 0;
	int count = 0;
	
	TOC* entry = (TOC*) _toc;
	while ( entry )
	{
		while ( level<entry->level )
		{
			toc += L"<ul>\r\n";
			level++;
		}
		while ( level>entry->level )
		{
			toc += L"</ul>\r\n";
			level--;
		}
		
		wstring targetName = wstring(entry->name);
		size_t pos;
		while ( (pos=targetName.find(L" "))!=string::npos )
			targetName.replace(pos, 1, L"_", 1);
		targetName = CPPStringUtils::url_encode(targetName);
	
		int bufferSize = 128;
		wchar_t* buffer = (wchar_t*) malloc(bufferSize*sizeof(wchar_t));	
		
		swprintf(buffer, bufferSize, L"<li class=\"toclevel-%i\">", level);
		
		toc += L"<a href=\"#";
		toc += targetName.c_str();
		toc += L"\"><span class=\"toctext\">";
		
		toc += buffer;
		toc += entry->name;
		toc += L"</span></a></li>\r\n";
		
		free(buffer);
		
		count++;
		entry = entry->next;
	}
	while ( level>1 )
	{
		toc += L"</ul>\r\n";
		level--;
	}
	toc += L"</td></tr></table></p>";
	
	if ( count<=3 && !_forceToc )
		return;
	
	int length = wcslen(_pOutput) + toc.length();
	wchar_t* dst = (wchar_t*) malloc((length+1)*sizeof(wchar_t));
	
	wcsncpy(dst, _pOutput, _tocPosition);
	*(dst+_tocPosition) = 0x0;
	wcscat(dst, toc.c_str());
	wcscat(dst, (_pOutput+_tocPosition));

	free(_pOutput);
	_pOutput = dst;
	_iOutputSize = wcslen(_pOutput);
	_iOutputRemain = 0;
	
	_pCurrentOutput = _pOutput + _iOutputSize;
}

void WikiMarkupParser::InsertReferences()
{
	if ( !_references )
		return;
	
	Append(L"<ol class=\"wkReferenceList\"\r\n");
	int count = 1;
	REF* ref = (REF*) _references;
	while ( ref )
	{
		wchar_t number[16];
		swprintf(number, 16, L"%i", count);
		Append(L"<li class=\"wkReferenceListItem\">");
		Append(L"<a href=\"#_ref-");
		Append(number);
		Append(L"\" class=\"wkReferenceListAnchor\" name=\"_note-");
		Append(number);
		Append(L"\">&uarr;</a>&nbsp;");
		
		WikiMarkupParser wikiMarkupParser(_languageCodeW, _pageName, false);
		
		wchar_t reftext[ref->length+1];
		wcsncpy(reftext, ref->start, ref->length);
		reftext[ref->length] = 0x0;

		wikiMarkupParser.SetInput(reftext);
		wikiMarkupParser.SetRecursionCount(_recursionCount);
		wikiMarkupParser.Parse();
		AppendHtml(wikiMarkupParser.GetOutput());
		
		Append(L"</li>");
		
		count++;
		ref = ref->next;
	}
	Append(L"</ol>\r\n");
}
void WikiMarkupParser::InsertCategories()
{
	if ( !_categories )
		return;
	
	wstring categoriesName = CPPStringUtils::from_utf8w(_languageConfig->GetSetting("categoriesName", "Categories: "));
	
	Append(L"<p class=\"wkCategories\"> ");
	Append(categoriesName.c_str());
	Append(_categories);
	Append(L"</p>");
}

wstring WikiMarkupParser::DayName(int dayNo)
{
	if ( dayNo<0 || dayNo>6 )
		return wstring();
	
	string key = CPPStringUtils::to_string(wstring(dayName[dayNo]));
	
	return CPPStringUtils::from_utf8w(_languageConfig->GetSetting(key, key));
}

wstring WikiMarkupParser::AbbrMonthName(int monthNo)
{
	if ( monthNo<0 || monthNo>11 )
		return wstring();
	
	string key = CPPStringUtils::to_string(wstring(monNameAbbr[monthNo]));
	
	return CPPStringUtils::from_utf8w(_languageConfig->GetSetting(key, key));
}

wstring WikiMarkupParser::MonthName(int monthNo)
{
	if ( monthNo<0 || monthNo>11 )
		return wstring();
	
	string key = CPPStringUtils::to_string(wstring(monName[monthNo]));
	
	return CPPStringUtils::from_utf8w(_languageConfig->GetSetting(key, key));
}

int WikiMarkupParser::IsWikiTag(wchar_t* tagName) 
{
	int number = 0;
	if ( !tagName || !*tagName )
		return number;
	
	number++;	
	while ( wikiTags[number] ) 
	{
		if ( !wcscmp(wikiTags[number], tagName) )
			return number;
		
		number++;
	}
	
	return 0;
}


wstring WikiMarkupParser::randomTag(){
	char tag[16];
	sprintf(tag, "%08x%08x", random(),random());
	return CPPStringUtils::to_wstring(tag);
}

//const wstring patternBegin[]={L"<nowiki>",L"<pre>",L"<source>"};
const wstring patternBegin[]={L"<nowiki",L"<pre",L"<source"};
const wstring patternEnd[]={L"</nowiki>",L"</pre>",L"</source>"};
const wstring specials[]={L"{{!}}",L"{{!-}}",L"{{!!}}",
			L"{{!-!}}",L"{{!+}}",L"{{!~}}",
			L"{{(!}}",L"{{!)}}",L"{{((}}",L"{{))}}"};
const wstring specialRepl[]={L"01",L"02",L"03",L"04",L"05",L"06",L"07",L"08",L"09",L"10"};
const wstring specialRepl2[]={L"|",L"|-",L"||",
			L"|-\n|",L"|+",L"|-\n!",
			L"{|",L"|}",L"{{",L"}}"};

wstring WikiMarkupParser::strip(wstring src, map<wstring,wstring> *stripMap){
	if( src.length() ==0)
		return wstring();
	//wstring patternBegin[3];
	//patternBegin[0]=CPPStringUtils::to_wstring("<nowiki>");
	//patternBegin[1]=CPPStringUtils::to_wstring("<pre>");
	//patternBegin[2]=CPPStringUtils::to_wstring("<source>");
	int i;
	int p1=0,p2=0;
	for(i=0;i<3;i++){
		//assume the the tags are well-structured
		while( ((p1=src.find(patternBegin[i], p1)) != string::npos)
			&& (src[p1]==L' '||src[p1]==L'>')){
			p2=src.find(patternEnd[i],p1);
			if(p2!=string::npos){
				int plen=p2-p1+patternEnd[i].length();
				wstring tag=randomTag();
				wstring match=src.substr(p1,plen);
				//stripMap.insert(pair<wstring,wstring>(tag,match));
				(*stripMap)[tag]=match;
				src=src.replace(p1,plen,L"___STRIP__"+tag+L"__PIRTS___");
			}
			else{
				break;
			}
		};
	}
	p1=0;
	for(i=0;i<10;i++){
		while( (p1=src.find(specials[i],p1)) != string::npos ){
			
			src=src.replace(p1,specials[i].length(), L"___S_STRIP__"+specialRepl[i]+L"__PIRTS_S___");
		};
	}
	return src;
}
wstring WikiMarkupParser::unstrip1(wstring src, map<wstring,wstring> stripMap){
	if(src.length()==0)
		return wstring();
	map<wstring,wstring>::iterator it;
	wstring dst=src;
	for (it=stripMap.begin();it!=stripMap.end();it++){
		//cout<<CPPStringUtils::to_string(it->first)<<endl;
		wstring match=L"___STRIP__"+it->first+L"__PIRTS___";
		size_t p=dst.find(match);
		if(p!=string::npos)
			dst=dst.replace(p,match.length(),it->second);
	}
	return dst;
}


wchar_t* WikiMarkupParser::unstrip2(wchar_t *src){
	if(src==NULL)
		return NULL;
	wstring s=wstring(src);
	
	//wchar_t *psrc=src; *pdst=dst;
	bool unstripFlag=false;
	int p=0;
	for(int i=0;i<10;i++)
		while((p=s.find(L"___S_STRIP__"+specialRepl[i]+L"__PIRTS_S___"))!= string::npos)
		{
			s=s.replace(p,12+2+12,specialRepl2[i]);
			unstripFlag=true;
		}
	if(unstripFlag)
	{
		int l=s.length();
		wchar_t *dst=(wchar_t*)malloc(sizeof(wchar_t)*(l+1));
		wcsncpy(dst, s.c_str(), l);
		dst[l]=0x0;
		return dst;
	}
	else
	{
		return src;
	}
}


int WikiMarkupParser::GetRecursionCount(){
	return _recursionCount;
}
void WikiMarkupParser::SetRecursionCount(int count){
	if (count>=0)
		_recursionCount=count;
	else
		fprintf(stderr, "error:WikiMarkupParser::SetRecursionCount:negative value!\n");
} 
