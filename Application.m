#import "Application.h"
#define PI (3.1415926f)

Application* app = nil;

typedef enum {
	MySearchView,
	MyContentView,
	MyHistView,
	MyLangView,
} MyViews;

@implementation Application


@synthesize currentLang;
-(void)reloadLang{
	_langs=[self getInstalledLanguages];
	[self setCurrentLang:[[[_langs objectAtIndex:0] componentsSeparatedByString:@":"] objectAtIndex:0]];
	[_langlistview setLangsWithItems:_langs];
	return;
}
#ifdef ENABLE_ROTATE
-(void) initWebviewOrientation{
	_webviewoleft=[[UIView alloc] initWithFrame:CGRectMake(0.0f,0.0f,320.0f,420.0f)];
	[_webviewoleft setTransform:CGAffineTransformMakeRotation(PI/2.0f)];
	[_webviewoleft setAutoresizesSubviews:YES];
	_webviewoleft.autoresizingMask=UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

	_webvieworight=[[UIView alloc] initWithFrame:CGRectMake(0.0f,0.0f,320.0f,420.0f)];
	[_webvieworight setTransform:CGAffineTransformMakeRotation(-PI/2.0f)];
	[_webvieworight setAutoresizesSubviews:YES];
	_webvieworight.autoresizingMask=UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

	_webviewoportrait=[[UIView alloc] initWithFrame:CGRectMake(0.0f,0.0f,320.0f,420.0f)];
	[_webviewoportrait setAutoresizesSubviews:YES];
	_webviewoportrait.autoresizingMask=UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

	_webviewo=_webviewoportrait;
}
-(void) didRotate:(NSNotification *)notification
{
	UIDeviceOrientation ori=[[UIDevice currentDevice] orientation];

	[_webview removeFromSuperview];
	[_webview setScalesPageToFit:NO];
	[_webviewo removeFromSuperview];
	switch(ori)
	{
	case UIDeviceOrientationLandscapeLeft:
	
		_webviewo = _webviewoleft;
		break;
	case UIDeviceOrientationLandscapeRight:
		_webviewo = _webvieworight;
		break;
	default:
		_webviewo = _webviewoportrait;
	}
	[_webviewo addSubview:_webview];
	[_webview setScalesPageToFit:YES];
	[_webview setBounds:[_webviewo frame]];
	if(_currentview==MyContentView)
		[window addSubview:_webviewo];
	
	/*NSString *msg=[NSString stringWithFormat:@"webview:bounds:%f,%f,%f,%f",[_webview bounds].origin.x,[_webview bounds].origin.y, [_webview bounds].size.width, [_webview bounds].size.height];
	UIAlertView *myalert =[[UIAlertView alloc] initWithTitle:@"Orientation" message:msg delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
	[myalert show];
	[myalert release];
	*/
}
#endif //ENABLE_ROTATE
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	Application *app = self;
	[app setStatusBarHidden:NO animated:NO];

	CGRect outer = [[UIScreen mainScreen] applicationFrame];
	//setup window
	window = [[UIWindow alloc] initWithFrame:outer];
	[window setAutoresizesSubviews:YES];
	window.autoresizingMask=UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
			

        CGRect inner = [window bounds];
	CGRect webviewframe=CGRectMake(inner.origin.x,inner.origin.y,inner.size.width,inner.size.height-40);
	
#ifdef ENABLE_ROTATE
	[self initWebviewOrientation];
	_webview = [[UIWebView alloc] initWithFrame:[_webviewo bounds]];
	[_webviewo addSubview:_webview];

	[[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications]; 
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didRotate:) name:UIDeviceOrientationDidChangeNotification object:nil];

	[_webview setAutoresizesSubviews:YES];
	_webview.autoresizingMask=UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
#else
	_webviewo = [[UIView alloc] initWithFrame:webviewframe];
	_webview = [[UIWebView alloc] initWithFrame:[_webviewo bounds]];
	[_webviewo addSubview:_webview];
#endif

	[_webview setDelegate:self];
	[_webview setScalesPageToFit:YES];
	

	[ window makeKeyAndVisible ];

	CGRect tabbarRect = CGRectMake(0, inner.size.height - 40, inner.size.width, (CGFloat)(40.0));
	CGRect navbarRect = CGRectMake(0, 0 , inner.size.width, (CGFloat)(40.0));
	CGRect histlistRect = CGRectMake(0, 40 , outer.size.width, outer.size.height-40);
	CGRect histviewRect = CGRectMake(0, 0, outer.size.width, outer.size.height);
	CGRect queryboxRect = CGRectMake(0, 0 , inner.size.width, (CGFloat)(30.0));
	
	_tabbar=[[UITabBar alloc] initWithFrame:tabbarRect];
	UITabBarItem *_histButton = [[UITabBarItem alloc] initWithTabBarSystemItem:UITabBarSystemItemHistory tag:MyHistView];
	UITabBarItem *_searchButton = [[UITabBarItem alloc] initWithTabBarSystemItem:UITabBarSystemItemSearch tag:MySearchView];
	UITabBarItem *_contentButton = [[UITabBarItem alloc] initWithTitle:@"Content" image:[UIImage imageNamed:@"content.png"] tag:MyContentView];
	UITabBarItem *_langButton = [[UITabBarItem alloc] initWithTitle:@"Language" image:[UIImage imageNamed:@"globe.png"] tag:MyLangView];
	[_tabbar setItems:[NSArray arrayWithObjects:_searchButton,_contentButton,_histButton,_langButton,nil] animated:YES];
	[_tabbar setDelegate:self];
	UITableView *histlistview=[[UITableView alloc] initWithFrame:webviewframe];
	_histlistview=histlistview;
	_histlistname=[[NSMutableArray array] retain]; 
	_histlisturl=[[NSMutableArray array] retain];
	MyHistoryList *histDelegate=[[MyHistoryList alloc] initWithItems:_histlistname URLs:_histlisturl target:self];

	[_histlistview setDelegate:histDelegate];
	[_histlistview setDataSource:histDelegate];
	

	//Init a searchbar view 
	
	
	_searchview=[[UIView alloc] initWithFrame:inner];
	_searchbar=[[UISearchBar alloc] initWithFrame:CGRectMake(0,0,320,50)];
	_searchlistview=[[UITableView alloc] initWithFrame:CGRectMake(0,50,320,370)];
	((UITextField *)[(NSArray *)[_searchbar subviews] objectAtIndex:0]).enablesReturnKeyAutomatically = NO;
	((UITextField *)[(NSArray *)[_searchbar subviews] objectAtIndex:0]).delegate = self;
	[_searchlistview setOpaque:NO];
	[_searchlistview setAlpha:0.5];
	[_searchlistview setBackgroundColor:[UIColor colorWithRed:255 green:255 blue:255 alpha:127]];

	[_searchbar setDelegate:self];
	[_searchlistview setDelegate:self];
	[_searchlistview setDataSource:self];

	[_searchview addSubview:_searchbar];
	[_searchview addSubview:_searchlistview];
	[window addSubview:_searchview];
	
	[window addSubview:_tabbar];
	_currentview=MySearchView;

	//Start srvThread and load index page!
	WikiServer *wikisrv=[[WikiServer alloc] initWithPortNumber:8082];
	[NSThread detachNewThreadSelector:@selector(startSrvThread:) toTarget:wikisrv withObject:nil];
	//Language support
	
	_langs=[self getInstalledLanguages];
	[self setCurrentLang:[[[_langs objectAtIndex:0] componentsSeparatedByString:@":"] objectAtIndex:0]];
	_langview=[[UITableView alloc] initWithFrame:webviewframe];
	LangList *langlistview=[[LangList alloc] initWithItems:_langs target:self];
	_langlistview=langlistview;
	[_langview setDelegate:langlistview];
	[_langview setDataSource:langlistview];

	
	
	[_webview loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://127.0.0.1:8082/"]]];
	//Init Loading icon
	_isLoadingViewShown=NO;
	_loadingview=[[UIImageView alloc] initWithImage:[UIImage imageNamed:@"loading.png"]];
}

-(void) testThread:(id)parm
{

}

-(void) dealloc
{
	[window release];
	[super dealloc];
}

- (void)tabBar:(UITabBar *)tabBar didSelectItem:(UITabBarItem *)item{
	NSInteger barTag=[(UIBarItem *)item tag];
	switch(barTag){
	case MyHistView:
		[_searchview removeFromSuperview];
		[_webviewo removeFromSuperview];
		[_langview removeFromSuperview];
		[window addSubview:_histlistview];
		[window bringSubviewToFront:_tabbar];
		//[_histview bringSubviewToFront:_histlistview];
		break;
	case MySearchView:
		[_histlistview  removeFromSuperview];
		[_webviewo removeFromSuperview];
		[_langview removeFromSuperview];
		//[_searchview removeFromSuperview];
		[window addSubview:_searchview];
		[window bringSubviewToFront:_tabbar];
		break;
	case MyContentView:
		[_histview removeFromSuperview];
		[_searchview removeFromSuperview];
		[_langview removeFromSuperview];
		[window addSubview:_webviewo];
		[window bringSubviewToFront:_tabbar];
		NSLog(@"tabBar:didSel:Content");
		break;
	case MyLangView:
		[self reloadLang];
		[_histview removeFromSuperview];
		[_searchview removeFromSuperview];
		[_webviewo removeFromSuperview];
		[window addSubview:_langview];
		[window bringSubviewToFront:_tabbar];
		break;
	}
	_currentview=barTag;
	[tabBar setSelectedItem:[[tabBar items] objectAtIndex:barTag]];
	return;	

}
-(BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType{
	
	if(_isLoadingViewShown==NO){
		[window addSubview:_loadingview];
		_isLoadingViewShown=YES;
	}
	return (YES);
}



- (void)webViewDidFinishLoad:(UIWebView *)webView
{
	NSString *wikiURL=[NSString stringWithString:[webView stringByEvaluatingJavaScriptFromString:@"document.location.href"]];
	NSString *wikiTitle=[NSString stringWithString:[webView stringByEvaluatingJavaScriptFromString:@"document.title"]];
	[_histlistname addObject:wikiTitle];
	[_histlisturl addObject:wikiURL];
	[_histlistview reloadData];
	_isLoadingViewShown=NO;
	[_loadingview removeFromSuperview];
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	UITableViewCell *cell=[tableView dequeueReusableCellWithIdentifier:@"search"];
	if(cell == nil)
	{
		cell = [[[UITableViewCell alloc] initWithFrame:CGRectZero reuseIdentifier:@"search"] autorelease];
		[cell setBackgroundColor:[UIColor colorWithRed:255 green:255 blue:255 alpha:255]];

	}
	[cell setText:[[_searchhitlist objectAtIndex:indexPath.row] retain]];
	return cell;
}

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
		return [_searchhitlist count];
}


- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	[self switchToContentWithHit:[[_searchhitlist objectAtIndex:indexPath.row]  stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

}


- (void)switchToContentWithHit:(NSString *)hitString {
	[self switchToContentWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:8082/wiki/%@/%@", [self currentLang],hitString]]];
}

- (void)switchToContentWithURL:(NSURL *)url {
	//remove previous view from window
	switch(_currentview){
	case MyHistView:
	//histview
		[_histview removeFromSuperview];
		break;
	case MySearchView:
	//searchview
		[_searchview removeFromSuperview];
		break;
	}
	[window addSubview:_webviewo];
	[window bringSubviewToFront:_webviewo];
	NSLog(@"switchToContentWithURL:%@",[url absoluteString]);
	//Show loading view
	[window addSubview:_loadingview];
	_isLoadingViewShown=YES;
	[_webview loadRequest:[NSURLRequest requestWithURL:url]];
	_currentview=MyContentView;
	[_tabbar setSelectedItem:[[_tabbar items] objectAtIndex:_currentview]];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
	// Minimizes UIKeyboard on screen
	[textField resignFirstResponder];
	return YES;
}
- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)searchText
{
	[_searchhitlist release];
	_searchhitlist = [[NSMutableArray alloc] init];
	if([searchText localizedCaseInsensitiveCompare:@""] != NSOrderedSame)
	{
		[_searchhitlist addObjectsFromArray:[self resultForSearch:searchText inLang:[self currentLang]]];
	}
	[_searchlistview reloadData];
}


- (NSArray *)resultForSearch:(NSString *)searchText inLang:(NSString *)langcode
{
	NSString *requrl=[[NSString stringWithFormat:@"http://127.0.0.1:8082/ajax/search:%@:%@",langcode,searchText] stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
	NSURLRequest *req=[NSURLRequest requestWithURL:[NSURL URLWithString:requrl] cachePolicy:NSURLRequestUseProtocolCachePolicy timeoutInterval:10.0];
	NSURLResponse *resp=[[NSURLResponse alloc] init];
	NSURLResponse **presp=&resp;
	NSError *urlerr=nil;
	NSData *content=[NSURLConnection sendSynchronousRequest:req returningResponse:&resp error:nil];
	char *contentbytes=(char *)[content bytes];
	NSString *strcont = [[NSString alloc] initWithData:content encoding:NSUTF8StringEncoding];

	NSLog(@"req:%@;cont:%@",requrl,strcont );
	NSLog(@"req:NSError:code:%d:domain:%@",[urlerr code],[urlerr domain]);
	return [[strcont componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]] retain];
}


- (NSArray *)getInstalledLanguages 
{
	NSURLRequest *req=[[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"http://127.0.0.1:8082/ajax/GetInstalledLanguages"]];
	NSURLResponse *resp=[[NSURLResponse alloc] init];
	NSURLResponse **presp=&resp;
	NSData *content=[NSURLConnection sendSynchronousRequest:req returningResponse:presp error:nil];
	char *contentbytes=(char *)[content bytes];
	NSString *strcont = [[NSString alloc] initWithData:content encoding:NSUTF8StringEncoding];
	//return array of 'langcode:langname'
	return [[strcont componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]] retain];

}


-(void)reloadSearch {
	[self searchBar:_searchbar textDidChange:[_searchbar text]];
	return;
}
@end
