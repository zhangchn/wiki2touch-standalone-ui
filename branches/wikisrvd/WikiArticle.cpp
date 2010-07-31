/*
 *  WikiArticle.cpp
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

#include "WikiArticle.h"
#include "WikiMarkupGetter.h"
#include "WikiMarkupParser.h"
#include "CPPStringUtils.h"
#include "Settings.h"
#include "StringUtils.h"

WikiArticle::WikiArticle(string languageCode)
{
	_languageCode = string(languageCode);
	_articleName = string();
}

WikiArticle::~WikiArticle()
{
}

string WikiArticle::GetArticleName()
{
	return _articleName;
}

wstring WikiArticle::GetArticle(ArticleSearchResult* articleSearchResult)
{
	WikiMarkupGetter wikiMarkupGetter(_languageCode);
	wstring article = wikiMarkupGetter.GetMarkupForArticle(articleSearchResult);
	
	return ProcessArticle(article, wikiMarkupGetter.GetLastArticleTitle());
}

wstring WikiArticle::GetArticle(string utf8articleName)
{
	WikiMarkupGetter wikiMarkupGetter(_languageCode);
	wstring article = wikiMarkupGetter.GetMarkupForArticle(utf8articleName);
	
	return ProcessArticle(article, wikiMarkupGetter.GetLastArticleTitle());
}	

wstring WikiArticle::ProcessArticle(wstring article, string articleTitle)
{
	if ( article.empty() )
		return article;
	_articleName = articleTitle;
	
	wstring redirected = wstring();
	// check if we're redirected
	if ( article.length()<200 )
	{
		// for speed reason, make no sense to scan a 1 MB articles
		wstring lowercaseArticle = CPPStringUtils::to_lower(article);
		
		size_t pos;
		if ( (pos=lowercaseArticle.find(L"#redirect"))!=string::npos )
		{
			string newUtf8ArticleName = CPPStringUtils::to_utf8(article.substr(pos + 9)); 
			while ( newUtf8ArticleName.length()>0 && newUtf8ArticleName[0]!='[' )
				newUtf8ArticleName = newUtf8ArticleName.substr(1);

			while ( newUtf8ArticleName.length()>0 && newUtf8ArticleName[0]=='[' )
				newUtf8ArticleName = newUtf8ArticleName.substr(1);
			
			pos = newUtf8ArticleName.find("]]");
			size_t posOfNumMark = newUtf8ArticleName.find("#");
			string anchorName;
			if ( pos!=string::npos )
			{
				if (posOfNumMark!=string::npos) {
					anchorName = newUtf8ArticleName.substr(posOfNumMark, pos-posOfNumMark);
					newUtf8ArticleName = newUtf8ArticleName.substr(0, posOfNumMark);
				}else {
					newUtf8ArticleName = newUtf8ArticleName.substr(0, pos);
				}
				
				while ( (pos=newUtf8ArticleName.find("_"))!=string::npos )
					newUtf8ArticleName.replace(pos, 1, " ", 1);
				
				WikiMarkupGetter wikiMarkupGetter(_languageCode);
				article =  wikiMarkupGetter.GetMarkupForArticle(newUtf8ArticleName);
				redirected = L"<span class=\"wkRedirected\">(Redirected from " + CPPStringUtils::from_utf8w(_articleName) + L")</span>\r\n";
				if (posOfNumMark!=string::npos) {
					redirected += L"<script>setTimeout(\"document.location.href='"+CPPStringUtils::from_utf8w(anchorName)+L"';\",80);</script>";
				}
				_articleName = wikiMarkupGetter.GetLastArticleTitle();
			}			
		}
	}
		
	wstring pageName = CPPStringUtils::to_wstring(_articleName);
	WikiMarkupParser wikiMarkupParser(CPPStringUtils::to_wstring(_languageCode).c_str(), pageName.c_str());
	wikiMarkupParser.SetInput(article.c_str());
	wikiMarkupParser.Parse();

	article = wstring(wikiMarkupParser.GetOutput()); 
	
	// Prepare everything what should go before the article body itself
	wstring preArticleHtml;
	wstring articleTitleW = CPPStringUtils::from_utf8w(_articleName);
	
	string filename = settings.WebContentPath() + "PreArticle.html";
	void* contents = LoadFile(filename.c_str());
	if ( contents )
	{
		preArticleHtml = CPPStringUtils::from_utf8w(string((char*) contents));
		
		// replace some params
		size_t pos;
		
		wstring placeholder = L"%ArticleTitle%";
		while ( (pos=preArticleHtml.find(placeholder))!=string::npos )
			preArticleHtml.replace(pos, placeholder.length(), articleTitleW);
			   
		placeholder = L"%RedirectedFrom%";
		while ( (pos=preArticleHtml.find(placeholder))!=string::npos )
			preArticleHtml.replace(pos, placeholder.length(), redirected);
		
		free(contents);
	}
	else
	{
		preArticleHtml = L"<html><head>\r\n";

		preArticleHtml.append(L"<meta id=\"viewport\" name=\"viewport\" content=\"width=320; initial-scale=0.6667; maximum-scale=1.0; minimum-scale=0.6667 \"/>\r\n");

		preArticleHtml.append(L"<LINK href=\"/stylesheets/shared.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
		preArticleHtml.append(L"<LINK href=\"/stylesheets/main.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
		preArticleHtml.append(L"<LINK href=\"/stylesheets/mediawiki_common.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
		preArticleHtml.append(L"<LINK href=\"/stylesheets/mediawiki_monobook.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
		preArticleHtml.append(L"<LINK href=\"/stylesheets/wikisrv.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
		preArticleHtml.append(L"<title>");
		preArticleHtml.append(articleTitleW);
		preArticleHtml.append(L"</title>\r\n");
	
		preArticleHtml.append(L"</head>\r\n<body class=\"wkBody\">\r\n");
		preArticleHtml.append(L"<div class=\"wkTitle\">");
		preArticleHtml.append(L"<a href=\"/\" class=\"wkTitleLink\"><img src=\"/icon_search.gif\"/>&nbsp;");
		preArticleHtml.append(articleTitleW);
		preArticleHtml.append(L"</a></div>\r\n");
		if ( !redirected.empty() )
			preArticleHtml.append(redirected);
		preArticleHtml.append(L"<p />\r\n");
	}

	// prepare everything what should go after the article html
	wstring postArticleHtml;
	
	filename = settings.WebContentPath() + "PostArticle.html";
	contents = LoadFile(filename.c_str());
	if ( contents )
	{
		postArticleHtml = CPPStringUtils::from_utf8w(string((char*) contents));
		free(contents);
	}
	else	
		postArticleHtml = L"\r\n</body></html>";
	
	// Insert it to the start of the article
	article.insert(0, preArticleHtml.c_str());
	article.append(postArticleHtml);

	return article;
}

wstring WikiArticle::FormatSearchResults(ArticleSearchResult* articleSearchResult)
{
	wstring html = wstring();
	html.append(L"<html><head><title>Search results</title>\r\n");
	html.append(L"<meta name=\"viewport\" content=\"width=device-width; initial-scale=1.0; maximum-scale=1.0; user-scalable=0;\"/>\r\n");
	html.append(L"<LINK href=\"/stylesheets/wikisrv.css\" type=\"text/css\" rel=\"stylesheet\">\r\n");
	html.append(L"</head>\r\n<body class=\"wkSearchBody\">\r\n<div class=\"wkSearchTitle\">Search results:</div><p />");
	html.append(L"<div class=\"wkSearchResult\">\r\n");

	while ( articleSearchResult )
	{
		html.append(L"<a href=\"./");
		html.append(CPPStringUtils::to_wstring(articleSearchResult->TitleInArchive()));
		html.append(L"\" class=\"wkSearchResultLink\">");
		html.append(CPPStringUtils::to_wstring(articleSearchResult->TitleInArchive()));
		html.append(L"</a><br />\r\n");
		
		articleSearchResult = articleSearchResult->Next;
	}
	html.append(L"</div></body>\r\n</html>");
	
	return html;
}


