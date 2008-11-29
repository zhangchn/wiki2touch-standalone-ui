#import <stdio.h>
#import <stdlib.h>
#import <sys/types.h>
#import <unistd.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLResponse.h>
#import <Foundation/NSURLRequest.h>
#import <UIKit/UIKit.h>

#import "HistListView.h"
#import "LangListView.h"
#import "srvmain.h"
@class WikiServer;


@interface Application : UIApplication <UIWebViewDelegate,UITableViewDelegate,UITableViewDataSource,UITextFieldDelegate,UISearchBarDelegate,UITabBarDelegate> 
{
	UIWindow			*window;
	UIView	*_webviewo;
	UIView	*_webviewoleft;
	UIView	*_webvieworight;
	UIView	*_webviewoportrait;

	UIWebView	*_webview;
	UITabBar	*_tabbar;
	NSString	*_strURL;
	UITableView	*_histlistview;
	UITableView	*_langview;
	LangList	*_langlistview;
	UIView	*_histview;
	NSMutableArray	*_histlistname;
	NSMutableArray	*_histlisturl;
	NSArray	*_langs;
	//NSString	*_currentlang;
	int	_currentview;
	NSMutableArray	*_searchhitlist;
	UIView	*_searchview;
	UITableView	*_searchlistview;
	UISearchBar	*_searchbar;
	//Status of Loading Icon
	BOOL	_isLoadingViewShown;
	UIImageView	*_loadingview;
}
@property(nonatomic,retain) NSString *currentLang;

- (void)tabBar:(UITabBar *)tabBar didSelectItem:(UITabBarItem *)item;
- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType;
- (void)webViewDidFinishLoad:(UIWebView *)webView;
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section;
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath;
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath;
- (BOOL)textFieldShouldReturn:(UITextField *)textField;
- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)searchText;
- (NSArray *)resultForSearch:(NSString *)searchText inLang:(NSString *)langcode;
- (void)switchToContentWithURL:(NSURL *)url;
- (void)switchToContentWithHit:(NSString *)hitString;
- (NSArray *)getInstalledLanguages ;
-(void)reloadSearch;
@end

