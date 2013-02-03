/*
 *  WikiMarkupParser.h
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

#ifndef WIKIMARKUPPARSER_H
#define WIKIMARKUPPARSER_H

#include "ConfigFile.h"
#include <map>

struct tagType {
	wchar_t* name;
	int position;
	tagType *pPrevious;
	tagType *pNext;
};

class WikiMarkupParser {

public:
	WikiMarkupParser(const wchar_t* languageCode, const wchar_t* pageName=NULL, bool doExpandtemplates=true);
	~WikiMarkupParser();
	
	void SetInput(const wchar_t* pInput);
	const wchar_t* GetOutput();
	void Parse();
		
private:
	const wchar_t* _languageCodeW;
	
	/* input buffer handling */
	wchar_t*		_pInput;
	wchar_t*		_pCurrentInput;
	size_t			_inputLength;

	/* output buffer handling */
	wchar_t*		_pOutput;
	wchar_t*		_pCurrentOutput;
	size_t			_iOutputSize;
	size_t			_iOutputRemain;
	
	/* should templates be expanded, usually this is only necessary for the first start	*/
	bool _doExpandTemplates;
	
	/* do we deal with images ? */
	bool _imagesInstalled;

	/* are we currentyl at the start of a new line */
	int _newLine;

	/* some open constructs */
	int _orderedEnumeration;
	int _unorderedEnumeration;
	int _italic;
	int _bold;
	int _definitionList;
	int _externalLinkNo;
		
 	/* this is a stack for tags, mainly usied for tables */
	tagType* _pCurrentTag;

	/* our current page name, can be null */
	const wchar_t *_pageName;
	
	/* if set to true this stops parsing */
	bool _stop;
	
	/* table of contents position */
	long _tocPosition;
	
	/* do we have a toc */
	bool _noToc;
	
	/* toc forced */
	bool _forceToc;
	
	/* table of contents list */
	void* _toc;
	
	/* references list */
	void* _references;
	
	/* simply a list of categories */
	wchar_t* _categories;
	
	/* prefix of images, "image" is check everytime */
	wchar_t* _imageNamespace;
	/* count for recursions, preventing infinite loops */
	int _recursionCount;
		
	ConfigFile* _languageConfig;
	TitleIndex* _titleIndex;
	
	double EvaluateExpression(const wchar_t* expression);
	
	void ReplaceInput(const wchar_t* text, long position, long length);
	
	wchar_t GetNextChar();
	wchar_t Peek();
	wchar_t Peek(int count);
	void Eat(int count);
	
	const wchar_t* PosOfNextParamPipe(const wchar_t* pos);

	wchar_t* RemoveComments(const wchar_t* src);
	wchar_t* ExpandTemplates(const wchar_t* src);
	wchar_t* ExpandTemplate(const wchar_t* templateText);
	wchar_t* HandleKnownTemplatesAndVariables(const wchar_t* text);
	wchar_t* NotHandledText(const wchar_t* text);
		
	wchar_t* GetTextInDoubleBrakets(wchar_t startBraket, wchar_t endBraket);
	wchar_t* GetTextInSingleBrakets(wchar_t startBraket, wchar_t endBraket);
	wchar_t* GetNextLine();
	wchar_t* GetTextUntilNextTag();
	wchar_t* GetTextUntilEndOfTag(const wchar_t *tagName);
	
	bool GetPixelUnit(wchar_t* src, int* width, int* height);
	
	void PushTag(const wchar_t* name, bool output=true);
	void PopTag(const wchar_t* name, bool output=true);
	bool TopTagIs(const wchar_t* name);
	
	void Append(wchar_t c);
	void Append(const wchar_t* msg);
	void AppendHtml(const wchar_t* html);
	void AppendPre(const wchar_t* pre);

	void HandleInternalLink(const wchar_t* linkText);
	void HandleExternalLink(const wchar_t* linkText);
	void HandleChar(wchar_t c);
	void HandleHeadline(const wchar_t* headlineText, int level);
	void HandleParagraph();
	wchar_t* GetParams(bool stopAtExclamtionAlso);

	void ParseNoWikiArea(int tagType);
	void CloseOpenWikiTags();
	void InsertToc();
	void InsertReferences();
	void InsertCategories();
	
	wstring DayName(int dayNo);
	wstring AbbrMonthName(int monthNo);
	wstring MonthName(int monthNo);
	
	int IsWikiTag(const wchar_t* tagName);
	
	wstring randomTag();
	wstring strip(wstring src, map<wstring,wstring> *stripMap);
	wstring unstrip1(wstring src, map<wstring,wstring> stripMap);
	wchar_t* unstrip2(wchar_t *src);
	int GetRecursionCount();
	void SetRecursionCount(int count);
};

#endif //WIKIMARKUPPARSER_H
