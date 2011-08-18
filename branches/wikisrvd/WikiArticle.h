/*
 *  WikiArticle.h
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

#ifndef WIKIARTICLE_H
#define WIKIARTICLE_H

#include <string>
#include "TitleIndex.h"

using namespace std;

class WikiArticle {
	
public:
	WikiArticle(string languageCode="de");
	~WikiArticle();
	
	string GetArticleName();
	wstring GetArticle(string utf8ArticleName);
	wstring GetArticle(ArticleSearchResult* articleSearchResult);
	
	wstring FormatSearchResults(ArticleSearchResult* articleSearchResult);
	wstring ProcessArticle(wstring article, string articleTitle);

	wstring RawSearchResults(ArticleSearchResult* articleSearchResult);
	wstring GetRawArticle(ArticleSearchResult* articleSearchResult);
	wstring GetRawArticle(string utf8articleName);
private: 
	string _articleName;
	string _languageCode;
};

#endif // WIKIARTICLE_H
