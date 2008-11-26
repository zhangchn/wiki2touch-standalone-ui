/*
 *  TitleIndex.h
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

#ifndef TITLEINDEX_H
#define TITLEINDEX_H

#include <string>
using namespace std;

class ArticleSearchResult
{
public:
	ArticleSearchResult(string title, string titleInArchive, fpos_t blockPos, int articlePos, int articleLength);
	
	string Title();
	string TitleInArchive();
	
	fpos_t BlockPos();
	int ArticlePos();
	int ArticleLength();
	
	ArticleSearchResult* Next;
	
private:
	string _title;
	string _titleInArchive;
	fpos_t _blockPos;
	int _articlePos;
	int _articleLength;
};

class TitleIndex
{
public:
	TitleIndex(string pathToDataFile);
	~TitleIndex();
	
	ArticleSearchResult* FindArticle(string title, bool multiple=false);
	void DeleteSearchResult(ArticleSearchResult* articleSearchResult);
	string DataFileName();
	int NumberOfArticles();
	
	string GetSuggestions(string phrase, int maxSuggestions);
	string GetRandomArticleTitle();
	
	string ImageNamespace();
	string TemplateNamespace();	

private:
	string  _dataFileName;
	int		_numberOfArticles;
	bool	isChinese;
	
	fpos_t	_titlesPos;
	fpos_t	_indexPos_0;
	fpos_t	_indexPos_1;
		
	string GetTitle(FILE* f, int articleNumber, int indexNo);
	string PrepareSearchPhrase(string phrase);
	
	fpos_t	_lastBlockPos;
	int	_lastArticlePos;
	int _lastArticleLength;
	
	string _imageNamespace;
	string _templateNamespace;
};

#endif
