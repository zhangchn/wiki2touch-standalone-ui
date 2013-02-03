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
#include "ConfigFile.h"
using namespace std;

class ArticleSearchResult
{
public:
	ArticleSearchResult(string title, string titleInArchive, off_t blockPos, off_t articlePos, int articleLength);
	
	string Title();
	string TitleInArchive();
	
	off_t BlockPos();
	off_t ArticlePos();
	int ArticleLength();
	
	ArticleSearchResult* Next;
	
private:
	string _title;
	string _titleInArchive;
	off_t _blockPos;
	off_t _articlePos;
	int _articleLength;
};

class TitleIndex
{
public:
	TitleIndex(string pathToDataFile);
	~TitleIndex();
	
	ArticleSearchResult* FindArticle(string title, bool multiple=false);
	ArticleSearchResult* FindArticle2(string title, bool multiple=false);
	void DeleteSearchResult(ArticleSearchResult* articleSearchResult);
	string DataFileName();
	string PathToDataFile();
	size_t NumberOfArticles();
	bool UseManifest();
	
	string GetSuggestions(string phrase, int maxSuggestions);
	string GetSuggestions2(string phrase, int maxSuggestions);
	string GetRandomArticleTitle();
	string GetRandomArticleTitle2();
	
	string ImageNamespace();
	string TemplateNamespace();	

private:
	string  _dataFileName;
	string	_pathToDataFile;
	size_t	_numberOfArticles;
	int	_numberOfBlocks;
	bool	isChinese;
	
	off_t	_titlesPos;
	off_t	_indexPos_0;
	off_t	_indexPos_1;
		
	string GetTitle(FILE* f, int articleNumber, int indexNo);
    string GetTitle(FILE* f, size_t articleNumber, size_t indexNo);
	string GetTitle2(FILE* fpIdxRecord, FILE* fpIdxSort, long articleNumber, size_t indexNo);
	string PrepareSearchPhrase(string phrase);
	
	off_t	_lastBlockPos;
	int	_lastArticlePos;
	int _lastArticleLength;

	unsigned long int _lastBlockNumber;
	
	string _imageNamespace;
	string _templateNamespace;

	bool _useManifest;
	ConfigFile *_Manifest;
};

#endif