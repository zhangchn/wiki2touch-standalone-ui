/*
 *  WikiMarkupGetter.h
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

#include <string>
#include "TitleIndex.h"

using namespace std;

class WikiMarkupGetter
{	
public:
	WikiMarkupGetter(string language_code);
	~WikiMarkupGetter();	
	
	string GetSearchResults(const string utf8SearchString, int maxHits);

	wstring GetMarkupForArticle(const wstring articleName);	
	wstring GetMarkupForArticle(const string utf8ArticleName);
	wstring GetMarkupForArticle(ArticleSearchResult* articleSearchResult);
	wstring GetMarkupForArticle2(ArticleSearchResult* articleSearchResult, TitleIndex *titleIndex);

	string GetLastArticleTitle();

	wstring GetTemplate(const wstring templateName, string templatePrefix);
	wstring GetTemplate(const string utf8TemplateName, string templatePrefix);
	
private:
	string _languageCode;	
	string _lastArticleTitle;
	char *DecompressBlockWithBits(off_t bBegin, off_t bEnd, FILE *f, int *pdSize);
	int miniFilter(char *pDecomp, off_t len_max);
};

