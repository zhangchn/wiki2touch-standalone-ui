/*
 * =====================================================================================
 *
 *       Filename:  servmain.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  11/23/08 02:55:41
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  first_name last_name (fl), fl@my-company.com
 *        Company:  my-company
 *
 * =====================================================================================
 */
#ifndef SERVMAIN
#define SERVMAIN
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#include "Settings.h"

#include "WikiArticle.h"
#include "CPPStringUtils.h"
#include "WikiMarkupGetter.h"
#include "WikiMarkupParser.h"

#define SERVER "wikiserver/1.0"
#define PROTOCOL "HTTP/1.1"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define DIRECTORY_LISTING_ALLOWED false
#import <Foundation/Foundation.h>
@interface WikiServer:NSObject{
        Settings *_settings;
}
-(id) init;
-(id) initWithPortNumber:(NSInteger)port;
-(id) initWithPortNumber:(NSInteger)port enableTemplate:(BOOL)enableTemplate;
-(void) startSrvThread:(id)param;

@end

#endif //SERVMAIN
