#include "LangListView.h"

@implementation LangList
-(void)setLangsWithItems:(NSArray *)langs{
	_langs=langs;
	return;
}
-(id)initWithItems:(NSMutableArray *)langs target:(id)target{
        _langs=langs;
        _target=target;
        return self;
}
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
        return [_langs count];
}
-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
        UITableViewCell *cell=[tableView dequeueReusableCellWithIdentifier:@"lang"];
        if(cell == nil)
        {
                cell = [[[UITableViewCell alloc] initWithFrame:CGRectZero reuseIdentifier:@"lang"] autorelease];
        }
        [cell setText:[[_langs objectAtIndex:indexPath.row] retain]];
        return cell;

}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	[_target setCurrentLang:[[[_langs objectAtIndex:indexPath.row] componentsSeparatedByString:@":"] objectAtIndex:0]];
	[_target reloadSearch];
}

@end
