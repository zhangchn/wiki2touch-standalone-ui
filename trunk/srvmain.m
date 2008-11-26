#include "srvmain.h"
extern volatile int myargc;
extern char **myargv;
int _sock = -1;
char* _webContentDir;
Settings *__settings;

char *get_mime_type(char *name)
{
        char *ext = strrchr(name, '.');
        if (!ext) return NULL;
        if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) return "text/html";
        if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return "image/jpeg";
        if (strcasecmp(ext, ".gif") == 0) return "image/gif";
        if (strcasecmp(ext, ".png") == 0) return "image/png";
        if (strcasecmp(ext, ".svg") == 0) return "image/svg+xml";      
        if (strcasecmp(ext, ".css") == 0) return "text/css";
        if (strcasecmp(ext, ".au") == 0) return "audio/basic";
        if (strcasecmp(ext, ".wav") == 0) return "audio/wav";
        if (strcasecmp(ext, ".mp3") == 0) return "audio/mpeg";
        if (strcasecmp(ext, ".avi") == 0) return "video/x-msvideo";
        if (strcasecmp(ext, ".mpeg") == 0 || strcasecmp(ext, ".mpg") == 0) return "video/mpeg";
        if (strcasecmp(ext, ".mp4") == 0) return "video/mp4";  
        return NULL;
}

void send_headers(FILE *f, int status, char *title, char *extra, char *mime, int length, time_t date=-1)
{
        time_t now;
        char timebuf[128];
       
        fprintf(f, "%s %d %s\r\n", PROTOCOL, status, title);
        fprintf(f, "Server: %s\r\n", SERVER);
        now = time(NULL);
        strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
        fprintf(f, "Date: %s\r\n", timebuf);
        if (extra) fprintf(f, "%s\r\n", extra);
        if (mime) fprintf(f, "Content-Type: %s\r\n", mime);
        if (length >= 0) fprintf(f, "Content-Length: %d\r\n", length);
        if (date != -1)
        {
                strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&date));
                fprintf(f, "Last-Modified: %s\r\n", timebuf);
        }
       
        fprintf(f, "Connection: close\r\n");
        fprintf(f, "\r\n");
}

void send_error(FILE *f, int status, char *title, char *extra, char *text)
{
        send_headers(f, status, title, extra, "text/html", -1, -1);
        fprintf(f, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n", status, title);
        fprintf(f, "<BODY><H4>%d %s</H4>\r\n", status, title);
        fprintf(f, "%s\r\n", text);
        fprintf(f, "</BODY></HTML>\r\n");
       
        //if ( settings.Verbose() )
                printf("error: %d %s\n\r", status, title);
}

void redirect_to(FILE *f, const char* target)
{
        char extra[512];
        sprintf(extra, "Location: %s", target);
        send_headers(f, 301, "moved permanently", extra, "text/html", 
-1, -1);

        fprintf(f, "<Please follow <a href=\"%s\">%s</a>\r\n", target, 
target);

        //if ( settings.Verbose() )
                printf("redirected to %s\r\n", target);
}

void send_file(FILE *f, char *path, struct stat *statbuf)
{
        char data[4096];
        int n;
       
        FILE *file = fopen(path, "r");
        if (!file)
                send_error(f, 403, "Forbidden", NULL, "Access denied.");
        else
        {
                int length = S_ISREG(statbuf->st_mode) ? statbuf->st_size : -1;
                send_headers(f, 200, "OK", NULL, get_mime_type(path), length, statbuf->st_mtime);
               
                while ((n = fread(data, 1, sizeof(data), file)) > 0)
                        if ( fwrite(data, 1, n, f)!=n )
                                break;
               
                fclose(file);
        }
}

void send_article(FILE *f, char* name)
{
        char help[strlen(name)+1];
        char* pHelp = help;
        char* pName = name;
       
        while ( *pName )
        {
                if ( *pName=='%' )
                {
                        pName++;
                       
                        int number = 0;
                       
                        unsigned char digit = (unsigned char) *pName;
                        digit = toupper(digit);
                        if ( digit<='9' )
                                digit -= 48;
                        else
                                digit -= 55;
                        number = digit;
                       
                        if (*pName)
                                pName++;

                        digit = (unsigned char) *pName;
                        digit = toupper(digit);
                        if ( digit<='9' )
                                digit -= 48;
                        else
                                digit -= 55;
                       
                        number = number*16 + digit;
                       
                        *pHelp++ = number;
                }
                else
                        *pHelp++ = *pName;
               
                pName++;
        }
        *pHelp = 0x0;

        char languageCode[3];  
        pHelp = help;
        if ( strlen(pHelp)>=3 && pHelp[2]==':' )
        {
                // change the "namespace" to a subfolder
                pHelp[2] = '/';
                redirect_to(f, (string("/wiki/") + string(pHelp)).c_str());
        }
        else if ( strlen(pHelp)<3 || pHelp[2]!='/' )
        {
                // no prefix, try to use the default
                redirect_to(f, (string("/wiki/") + __settings->DefaultLanguageCode() + "/" + string(name)).c_str());
                return;
        }
        else
        {
                strncpy(languageCode, pHelp, 2);
                languageCode[2] = 0;
       
                strcpy(help, pHelp+3);
                if ( !__settings->IsLanguageInstalled(languageCode) )
                {
                        if ( !strcmp(languageCode, "xx") )
                                send_error(f, 404, "Not found", NULL, "Language not installed.");
                        else
                                redirect_to(f, "/wiki/xx/Language not installed");
                        return;
                }
        }
       
        // the article name is already utf-8 encoded (by the browser?)
        std::string articleName = help;
       
        if ( articleName=="testpage.txt" )
        {
                string name = __settings->Path() + "testpage.txt";
                const char* path = name.c_str();
                FILE *file = fopen(path, "r");
                if (!file)
                        send_error(f, 403, "Forbidden", NULL, "Access denied.");
                else
                {                      
                        int error = fseek(file, 0, SEEK_END);
                        fpos_t length;
                       
                        if ( !error )
                                error = fgetpos(file, &length);
                       
                        if ( !error )
                                error = fseek(file, 0, SEEK_SET);
                       
                        char* contents = (char*) malloc(length+1);
                        fread(contents, 1, length, file);
                        contents[length] = 0x0;
                        fclose(file);
                       
                        WikiMarkupParser wikiMarkupParser(CPPStringUtils::to_wstring(languageCode).c_str(), L"Testpage");
                       
                        wstring article = CPPStringUtils::from_utf8w(string(contents));
                        free(contents);
                       
                        wikiMarkupParser.SetInput(article.c_str());
                        wikiMarkupParser.Parse();
                       
                        string data = "<html><body>\r\n";
                        data += CPPStringUtils::to_utf8(wikiMarkupParser.GetOutput());
                        data += "</html></body>";
                        length = data.length();
                       
                        send_headers(f, 200, "OK", NULL, "text/html; charset=utf-8", length, -1);
                        fwrite(data.c_str(), 1, length, f);
                }              
        }
        else
        {
                WikiArticle* wikiArticle = new WikiArticle(languageCode);
               
                TitleIndex* titleIndex = __settings->GetTitleIndex(languageCode);
               
                ArticleSearchResult* articleSearchResult = titleIndex->FindArticle(articleName, true);
               
                if ( articleSearchResult )
                {
                        if ( articleSearchResult->Next )
                        {
                                wstring searchResults = wikiArticle->FormatSearchResults(articleSearchResult);
                                string data = CPPStringUtils::to_utf8(searchResults);
                                int length = data.length();
                                send_headers(f, 200, "OK", NULL, "text/html; charset=utf-8", length, -1);              
                                fwrite(data.c_str(), 1, length, f);
                        }
                        else if ( articleSearchResult->Title()!=articleSearchResult->TitleInArchive() )
                                redirect_to(f, (string("/wiki/") + string(languageCode) + string(":") + articleSearchResult->TitleInArchive()).c_str());
                        else
                        {
                                std::wstring article = wikiArticle->GetArticle(articleSearchResult);    
                                if ( !article.empty() )
                                {
                                        string data = CPPStringUtils::to_utf8(article);
                               
                                        int length = data.length();
                                        send_headers(f, 200, "OK", NULL, "text/html; charset=utf-8", length, -1);


                                        fwrite(data.c_str(), 1, length, f);
                                }
                                else if ( !strcmp(languageCode, "xx") && articleName=="Article not found" )
                                        send_error(f, 404, "Not Found", NULL, "Article not found.");
                                else
                                        redirect_to(f, "/wiki/xx/Article not found");

                        }
                }
                else if ( !strcmp(languageCode, "xx") && articleName=="Article not found" )
                        send_error(f, 404, "Not Found", NULL, "Article not found.");
                else
                        redirect_to(f, "/wiki/xx/Article not found");
                titleIndex->DeleteSearchResult(articleSearchResult);
               
                delete(wikiArticle);
        }
}

int process(FILE *f)
{
        char buf[4096];
        char *method;
        char *relativ_path;
        char *protocol;
        struct stat statbuf;
        char pathbuf[4096];
        int len;
        char path[4096];
       
        if (!fgets(buf, sizeof(buf), f)) return -1;
        //if ( settings.Verbose() )
                printf("URL: %s", buf);
       
        method = strtok(buf, " ");
        relativ_path = strtok(NULL, " ");
        protocol = strtok(NULL, "\r");
        if (!method || !relativ_path || !protocol) return -1;

        // access is relative to the users media/wikipedia directory
        strcpy(path, __settings->WebContentPath().c_str());
        strcat(path, relativ_path);
       
        fseek(f, 0, SEEK_CUR); // Force change of stream direction
       
        if (strcasecmp(method, "GET") != 0)
                send_error(f, 501, "Not supported", NULL, "Method is not supported.");
        else if ( strlen(relativ_path)>=6 && strcasestr(relativ_path, "/wiki/")==relativ_path )
        {
                char* url = &relativ_path[6];

                char languageCode[3];
                strcpy(languageCode, __settings->DefaultLanguageCode().c_str());
               
                if ( strlen(url)>=3 && (url[2]=='/' || url[2]==':') )
                {
                        languageCode[0] = tolower(*url++);
                        languageCode[1] = tolower(*url++);
                        languageCode[2] = 0;
                        url++;
                }
               
                if ( strcasestr(url, "image:") )
                {
                        url += 6;
                       
                        string filename = CPPStringUtils::url_decode(url);
                       
                        size_t pos = 0;
                        while ( (pos=filename.find(" "))!=string::npos )
                                filename.replace(pos, 1, "_", 1);

                        // the index/data for the "local" file
                        ImageIndex* imageIndex = __settings->GetImageIndex(languageCode);
                        int length = 0;
                        unsigned char* imageData = imageIndex->GetImage(filename, &length);
                        if ( !imageData || !length )
                        {
                                // not found in the "local" data file, try the "coomons" one
                                imageIndex = __settings->GetImageIndex("xc");
                                imageData = imageIndex->GetImage(filename, &length);
                                if ( imageData )
                                {
                                        int i = 5;
                                        i = i + 6;
                                }
                        }
       
                        if ( imageData && length )
                        {
                                send_headers(f, 200, "OK", NULL, get_mime_type(url), length, -1);
                                fwrite(imageData, 1, length, f);
                                free(imageData);
                        }
                        else
                        {
                                // first try the web content folder in the package
                                strcpy(path, __settings->WebContentPath().c_str());
                                strcat(path, "/Images/");
                                strcat(path, url);
                                if ( stat(path, &statbuf)!=0 )
                                {
                                        // Nope
                                        strcpy(path, __settings->Path().c_str());
                                        strcat(path, "/Images/");
                                        strcat(path, url);
                                }
                                if ( stat(path, &statbuf)==0 )
                                        send_file(f, path, &statbuf);
                                else
                                {
                                        /*
                                        strcpy(path, 
settings.WebContentPath().c_str());
                                        strcat(path, 
"/Images/NotFound.gif");
                                        if ( stat(path, &statbuf)==0 )
                                                send_file(f, path, 
&statbuf);
                                        else
                                         */
                                        send_error(f, 404, "Not Found", NULL, "File not found.");
                                }
                        }
                }
                else if ( !*url )
                {
                        // redirect to the main page if possible
                        ConfigFile* configFile = __settings->LanguageConfig(languageCode);
                        if ( configFile )
                        {
                                string mainPage = configFile->GetSetting("mainPage");
                                if ( !mainPage.empty() )
                                {
                                        mainPage = string("/wiki/") + languageCode + "/" + mainPage;
                                        redirect_to(f, mainPage.c_str());
                                        return 0;
                                }
                        }
                       
                        redirect_to(f, "/wiki/xx/Article not found");
                }
                else
                        send_article(f, &relativ_path[6]);
        }
        else if ( strlen(relativ_path)>6 && strcasestr(relativ_path, "/ajax/")==relativ_path )
        {
                char* url = &relativ_path[6];
               
                if ( strcasestr(url, "search:") )
                {
                        url += 7;

                        char languageCode[3];
                        if ( strlen(url)>=3 && url[2]==':' )
                        {
                                languageCode[0] = *url++;
                                languageCode[1] = *url++;
                                languageCode[2] = 0x0;
                                url++;
                        }
                        else
                        {
                                // no language code in the url, use the default one
                                strcpy(languageCode, __settings->DefaultLanguageCode().c_str());
                        }
                       
                        if ( strlen(url)==0 )
                        {
                                send_error(f, 404, "Nothing to search for or search string to short.", NULL, "");
                                return 0;
                        }
                       
                        TitleIndex* titleIndex = __settings->GetTitleIndex(languageCode);
                        if ( !titleIndex )
                        {
                                send_error(f, 404, "No language code not installed", NULL, "");
                                return 0;
                        }
                       
                        string phrase = CPPStringUtils::url_decode(url);
                        string suggestions = titleIndex->GetSuggestions(phrase, 25);
                        int length = suggestions.length();
                       
                        send_headers(f, 200, "OK", NULL, "text/html; charset=utf-8", length, -1);
                        fwrite(suggestions.c_str(), 1, length, f);
                }                          
                else if ( strcasestr(url, "RedirectToRandomArticle") )
                {
                        url += 23;
                       
                        char languageCode[3];
                        if ( strlen(url)>=2 )
                        {
                                languageCode[0] = *url++;
                                languageCode[1] = *url++;
                                languageCode[2] = 0x0;
                        }
                        else
                        {
                                // no language code in the url, use the default one
                                strcpy(languageCode, __settings->DefaultLanguageCode().c_str());
				NSLog(@"setting->defaultlangcode().cstr():%s",languageCode);
                        }


                        TitleIndex* titleIndex = __settings->GetTitleIndex(languageCode);
                        if ( !titleIndex || titleIndex->NumberOfArticles()<=0 )
                        {
                                redirect_to(f, "/wiki/xx/Language not installed");
                                return 0;
                        }
                       
                        string articleTitle = titleIndex->GetRandomArticleTitle();
                        if ( articleTitle.empty() )
                        {
                                redirect_to(f, "/wiki/xx/Article not found");
                                return 0;
                        }
                       
                        string redirectUrl = "/wiki/" + string(languageCode) + ":" + CPPStringUtils::url_encode(articleTitle);
                        redirect_to(f, redirectUrl.c_str());
                }
                else if ( strcasestr(url, "GetInstalledLanguages") )
                {
                        // returns a list of installed languages, the default one is the first entry, the xx one is ignored
                        // if there are other languages
                       
                        // there is a bug somewhere so memory management gets corrupted
                       
                        string result;
                        ConfigFile* configFile = __settings->LanguageConfig(__settings->DefaultLanguageCode());
                        if ( configFile )
                                result = __settings->DefaultLanguageCode() + ":" + configFile->GetSetting("name", __settings->DefaultLanguageCode());
                       
                        string installedLanguages = __settings->InstalledLanguages();
                        if ( !installedLanguages.empty() )
                        {
                                size_t pos = 0;
                                while ( pos!=string::npos )
                                {
                                        size_t nextPos = installedLanguages.find(",", pos);
                                       
                                        size_t length = 0;
                                        if ( nextPos==string::npos )
                                                length = installedLanguages.length() - pos;
                                        else
                                                length = nextPos - pos;
                                       
                                        string languageCode = installedLanguages.substr(pos, length);
                                        if ( languageCode!=__settings->DefaultLanguageCode() && languageCode!="xx" )
                                        {
                                                ConfigFile* configFile = __settings->LanguageConfig(languageCode);
                                                if ( configFile )
                                                {
                                                        result += "\n";
                                                        result += languageCode + ":" + configFile->GetSetting("name", languageCode);
                                                }
                                        }
                                       
                                        pos = nextPos;
                                        if ( pos!=string::npos )
                                                pos++;
                                }
                        }
                       
                        send_headers(f, 200, "OK", NULL, "text/html; charset=utf-8", result.length(), -1);
                        fwrite(result.c_str(), 1, result.length(), f);
                }
                else
                {
                        send_error(f, 404, "Command not found", NULL, "File not found.");
                }
        }
        else
        {
                if ( stat(path, &statbuf) < 0 )
                {
                        // if this is not found switch to the users media dir
                        strcpy(path, settings.Path().c_str());
                        strcat(path, relativ_path);
                }
                if (stat(path, &statbuf) < 0) {
                        send_error(f, 404, "Not Found", NULL, "File not found.");
                }
                else if (S_ISDIR(statbuf.st_mode))
                {
                        len = strlen(path);
                        if (len == 0 || path[len - 1] != '/')
                        {
                                snprintf(pathbuf, sizeof(pathbuf), "%s/index.html", path);
                                if (stat(pathbuf, &statbuf) >= 0)
                                {
                                        char newLocation[512];
                                        snprintf(newLocation, sizeof(newLocation), "%s/index.html", relativ_path);      
                                       
                                        redirect_to(f, newLocation);
                                }
                                else {
                                        snprintf(pathbuf, sizeof(pathbuf), "Location: %s/", path);
                                        send_error(f, 302, "Found", pathbuf, "Directories must end with a slash.");
                                }
                        }
                        else
                        {
                                snprintf(pathbuf, sizeof(pathbuf), "%sindex.html", path);
                                if (stat(pathbuf, &statbuf) >= 0)
                                        send_file(f, pathbuf, &statbuf);
                                else if ( DIRECTORY_LISTING_ALLOWED )
                                {
                                        DIR *dir;
                                        struct dirent *de;
                                       
                                        send_headers(f, 200, "OK", NULL, "text/html", -1, statbuf.st_mtime);
                                        fprintf(f, "<HTML><HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n<BODY>", path);
                                        fprintf(f, "<H4>Index of %s</H4>\r\n<PRE>\n", path);
                                        fprintf(f, "Name Last Modified Size\r\n");
                                        fprintf(f, "<HR>\r\n");
                                        if (len > 1) fprintf(f, "<A HREF=\"..\">..</A>\r\n");
                                       
                                        dir = opendir(path);
                                        while ((de = readdir(dir)) != NULL)
                                        {
                                                char timebuf[32];
                                                struct tm *tm;
                                               
                                                strcpy(pathbuf, path);
                                                strcat(pathbuf, de->d_name);
                                               
                                                stat(pathbuf, &statbuf);
                                                tm = gmtime(&statbuf.st_mtime);
                                                strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M:%S", tm);
                                               
                                                fprintf(f, "<A HREF=\"%s%s\">", de->d_name, S_ISDIR(statbuf.st_mode) ? "/" : "");
                                                fprintf(f, "%s%s", de->d_name, S_ISDIR(statbuf.st_mode) ? "/</A>" : "</A> ");
                                                if (de->d_namlen < 32) fprintf(f, "%*s", 32 - de->d_namlen, "");
                                               
                                                if (S_ISDIR(statbuf.st_mode))
                                                        fprintf(f, "%s\r\n", timebuf);
                                                else
                                                        fprintf(f, "%s %10d\r\n", timebuf, statbuf.st_size);
                                        }
                                        closedir(dir);
                                       
                                        fprintf(f, "</PRE>\r\n<HR>\r\n<ADDRESS>%s</ADDRESS>\r\n</BODY></HTML>\r\n", SERVER);
                                }
                                else
                                        send_error(f, 403, "Directory Listing Denied", NULL, "This virtual directory does not allow contents to be listed.");
                        }
                }
                else
                        send_file(f, path, &statbuf);
        }
       
        return 0;
}

void sigpipe(int sig)
{
        fprintf(stderr, "Received SIGPIPE.\r\n");      
}

void sighup(int sig)
{
        fprintf(stderr, "Received SIGHUP.\r\n");        
       
        if ( _sock!=-1 )
                close(_sock);
       
        _sock = -1;
}

void sigterm(int sig)
{
        fprintf(stderr, "Received SIGTERM.\r\n");      
       
        if ( _sock!=-1 )
                close(_sock);
       
        _sock = -1;
}
#if 0
@interface WikiServer:NSObject{
	Settings *_settings;
}
-(id) init;
-(id) initWithPortNumber:(NSInteger)port;
-(id) initWithPortNumber:(NSInteger)port enableTemplate:(BOOL)enableTemplate;
-(void) startSrvThread:(id)param;

@end
#endif
@implementation WikiServer
-(id) init{
	return [self initWithPortNumber:8082 enableTemplate:NO];
}

-(id) initWithPortNumber:(NSInteger)port{
	return [self initWithPortNumber:8082 enableTemplate:NO];
}
-(id) initWithPortNumber:(NSInteger)port enableTemplate:(BOOL)enableTemplate{
	if(![super init]) return nil;
	_settings=new Settings;

	_settings->Init(myargc,myargv);
	__settings=_settings;
	if(__settings->IsLanguageInstalled("xx"))
		{NSLog(@"xx installed");}
	else
		{NSLog(@"xx not installed");}
	
	return self;
}
-(void) startSrvThread:(id)param{
	srandomdev();
	FILE *l;
	l=fopen ("/var/tmp/thread.log","w+");
	fprintf(l,"Wikisrvd:srvmain.m:startSrvThread\n");

	NSLog(@"Wikisrvd:srvmain.m:startSrvThread\n");
	signal(SIGPIPE,SIG_IGN);
	signal(SIGTERM,sigterm);
	struct sockaddr_in sin;
	_sock = socket(AF_INET, SOCK_STREAM, 0);

	int reuse=1;
	if(setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &reuse, sizeof(int)) < 0){
		NSLog(@"setsockopt() failed, maybe the socket is already in use?\n");
		return;
	}
	
	sin.sin_family = AF_INET;
	sin.sin_port = htons(settings.Port());
	sin.sin_addr.s_addr = settings.Addr();
	sin.sin_len = sizeof(sin);

	bind(_sock, (struct sockaddr *) &sin, sizeof(sin));

	if ( listen(_sock, 25)==0 )
	{
		while(_sock!=-1)
		{
			int s;
			FILE *f;

			s = accept(_sock, NULL, NULL);
			if(s<0)
				break;
			f = fdopen(s, "r+");
			process(f);

			fclose(f);
		}
	}
	else
		NSLog(@"Failed to listen on port %d on host %s\r\n", ntohs(sin.sin_port), inet_ntoa(sin.sin_addr));
	if(_sock!=-1)
		close(_sock);
	fclose(l);
	NSLog(@"srvThread:Terminated.\n");
	return;
}
@end
