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
#include <cstdlib>
#include <memory.h>
#include <wchar.h>
//#include <cwchar>
#include <bzlib.h>
#include <sys/stat.h>

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
	
	ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(utf8ArticleName,true);
	
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

	TitleIndex* titleIndex = settings.GetTitleIndex(_languageCode);
	if(titleIndex->UseManifest())
		return GetMarkupForArticle2(articleSearchResult, titleIndex);
	
	fpos_t blockPos = articleSearchResult->BlockPos(); 
	fpos_t articlePos = articleSearchResult->ArticlePos();
	int articleLength = articleSearchResult->ArticleLength();
	
	_lastArticleTitle = string(articleSearchResult->TitleInArchive());

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
			size_t len = (read-articlePos);
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
wstring WikiMarkupGetter::GetMarkupForArticle2(ArticleSearchResult* articleSearchResult, TitleIndex *titleIndex)
{
	fpos_t blockPos = articleSearchResult->BlockPos(); 
	fpos_t articlePos = articleSearchResult->ArticlePos();
	int articleLength = articleSearchResult->ArticleLength();
	
	_lastArticleTitle = string(articleSearchResult->TitleInArchive());

	string filename = titleIndex->DataFileName();	
	string path = titleIndex->PathToDataFile();
	
	FILE *fpBlkOffset=fopen((path+"/blockoffset").c_str(),"rb");
	if(!fpBlkOffset)
		return wstring();
	FILE *f=fopen((path+"/"+filename).c_str(),"rb");
	if(!f)
		return wstring();
	fseeko(fpBlkOffset, blockPos, SEEK_SET);
	fpos_t bBegin, bEnd;
	fread(&bBegin, sizeof(off_t), 1, fpBlkOffset);
	fseeko(fpBlkOffset, blockPos+sizeof(off_t), SEEK_SET);
	fread(&bEnd, sizeof(off_t), 1,fpBlkOffset);
	int dSize;
	char *decompBuff=DecompressBlockWithBits(bBegin,bEnd,f,&dSize);
	wstring content;
	if(decompBuff==NULL)
	{
		fprintf(stderr,"primary decomp failed.\n");
		fclose(fpBlkOffset);
		fclose(f);
		return wstring();
	}
	else
	{
		int len1;
		char *pDecomp=decompBuff+articlePos;
		if(articlePos+articleLength>dSize)
		{
			len1=miniFilter(pDecomp,dSize-articlePos+1);
			content = CPPStringUtils::from_utf8w(pDecomp);
			
			fseeko(fpBlkOffset, blockPos+sizeof(off_t)*2, SEEK_SET);
			fread(&bBegin,sizeof(fpos_t),1,fpBlkOffset);
			fseeko(fpBlkOffset, blockPos+sizeof(off_t)*3, SEEK_SET);
			fread(&bEnd, sizeof(fpos_t),1,fpBlkOffset);
			free(decompBuff);
			decompBuff=DecompressBlockWithBits(bBegin,bEnd,f,&dSize);
			if(decompBuff==NULL)
			{
				fprintf(stderr, "secondary decomp failed.\n");
				fclose(fpBlkOffset);
				fclose(f);
				return content;
			}
			//pDecomp=decompBuff;
			int len2=miniFilter(decompBuff,articleLength-len1);
				content+=CPPStringUtils::from_utf8w(decompBuff);
			if(len2<articleLength-len1)
				fprintf(stderr,"total length < articlelength\n");
		}
		else
		{
			len1=miniFilter(pDecomp,articleLength);
			content= CPPStringUtils::from_utf8w(pDecomp);
		}
		free(decompBuff);
	}
	fclose(fpBlkOffset);
	fclose(f);
	content=content.substr(0,content.find(L"</text>"));
	return content;
	
}
int WikiMarkupGetter::miniFilter(char *pDecomp, fpos_t len_max)
{
		int len=0;
		char *pFinal=pDecomp;
		while(len_max--){
			char c=*pDecomp++;
			len++;
			if(c=='&'){
				if(pDecomp[0]=='a'
				&& pDecomp[1]=='m'
				&& pDecomp[2]=='p'
				&& pDecomp[3]==';')
				{
					pDecomp+=4;
				}
				else if(pDecomp[0]=='q'
				&& pDecomp[1]=='u'
				&& pDecomp[2]=='o'
				&& pDecomp[3]=='t'
				&& pDecomp[4]==';'
				)
				{
					pDecomp+=5;
					c='\'';
				}
				else if(pDecomp[0]=='l' 
				&& pDecomp[1]=='t'
				&& pDecomp[2]==';')
				{
					pDecomp+=3;
					c='<';
				}
				else if(pDecomp[0]=='g' 
				&& pDecomp[1]=='t'
				&& pDecomp[2]==';')
				{
					pDecomp+=3;
					c='>';
				}

			}
			*pFinal=c;
			pFinal++;
		}
		*pFinal='\0';
	return len;
}

char *WikiMarkupGetter::DecompressBlockWithBits(off_t bBegin, off_t bEnd, FILE *f, int *pdSize)
{
	//struct stat64 *buf;
	//stat64(filename.c_str(), &buf);
	//off_t size_of_f=buf->st_size *8;
	//fprintf(stderr, "bBegin,bEnd:%llx %llx\n",bBegin,bEnd);
	FILE *t=fopen ("tmp.bz2","wb");
	if(!(bBegin<bEnd))
	{
		return NULL;
	}
	bEnd++;//stupid, but i'd rather follow bzip2recovery
	int remBegin=bBegin%8;
	off_t byteBegin=(bBegin-remBegin)/8;
	int remEnd=bEnd%8;
	off_t byteEnd=(bEnd-remEnd)/8;
	fseeko(f,byteBegin, SEEK_SET);
	int i;
	//fprintf(stderr, "rb, bb,re, be:\n%d, %lld, %d, %lld\n",
	//	remBegin, byteBegin, remEnd, byteEnd);
	char *buff=(char *)malloc (byteEnd-byteBegin+30);
	buff[0]='B';buff[1]='Z';buff[2]='h';buff[3]='9';
	buff[4]=0x31;buff[5]=0x41;buff[6]=0x59; 
	buff[7]=0x26;buff[8]=0x53;buff[9]=0x59;
	char *pBuff=buff+10;
	char a,b,c;
	c=fgetc(f);
	for(i=0;i<byteEnd-byteBegin;i++)
	{
	/*
		if(i<512){
			fprintf(stderr,"c:%x",c);
		}
	*/	
		a=c<<(remBegin);
		c=fgetc(f);
		c=(c&0x000000ff);
		b=((c&0x000000ff)>>(8-remBegin));
		*pBuff=(a|b);
	/*
		if(i<512){
			fprintf(stderr," ,c':%x , a:%x, b:%x, *pbuff:%x\n",
				c,a,b,*pBuff);
		}
	*/
		pBuff++;
	}
	

	int remainderBits=remEnd-remBegin;
	//fprintf(stderr, "remainderBits=%d\n",remainderBits);
	if(remainderBits>0)
	{
		a=(c<<(remBegin));
		//fprintf(stderr,"a: %x",a);
		*pBuff=(a & (0xff<<(8-remainderBits)));
		//fprintf(stderr," pBuff:%x\n",*pBuff);
	}
	else if(remainderBits<0)
	{
		pBuff--;
		remainderBits=8+remainderBits;
		a=*pBuff;
		*pBuff=(a & (0xff<<(8-remainderBits)));
	}
	uint8_t endseq[10]={0x17,0x72,0x45,0x38,0x50,0x90,0x0,0x0,0x0,0x0};
	endseq[6]=buff[10];
	endseq[7]=buff[11];
	endseq[8]=buff[12];
	endseq[9]=buff[13];
	/*
	c=endseq[0];
	a=*pBuff;
	a=(a&0x000000ff);
	fprintf(stderr, " endseq[0]=%x a=%x",endseq[0],a);
	b=((c&0x000000ff)>>(8-remainderBits));
	*pBuff=(a|b);
		fprintf(stderr," pBuff:%x\n",*pBuff);
	pBuff++;
	*/
	c=((*pBuff)&0x000000ff)>>(8-remainderBits);
	for (i=0;i<=9;i++){
		a=((c&0x000000ff)<<(8-remainderBits));
		c=endseq[i];
		b=((c&0x000000ff)>>(remainderBits));
		pBuff[i]=(a|b);
		//pBuff++;
	}
	pBuff+=10;
	a=((c<<(8-remainderBits))&0x000000ff);
	*pBuff=a;
	for(i=0;i<=pBuff-buff;i++)
		putc(buff[i],t);
	fclose(t);
	char *decompBuff=(char *)malloc(1000000);
	unsigned int dSize=999999;
	int sSize=(int)(pBuff-buff+1);
	int r=BZ2_bzBuffToBuffDecompress(decompBuff,
                                                &dSize,
                                                buff,
                                                sSize,
                                                0,0);
	free(buff);
	
	if(r!=BZ_OK)
	{
		fprintf(stderr, "BZ2:error %d\n",r);
		free(decompBuff);
		return NULL;
	}
	*pdSize=dSize;
	return decompBuff;
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
			
			size_t read = 0;
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
			size_t length = wikiTemplate.length();
			
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


