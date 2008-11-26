#import "HistListView.h"

@implementation MyHistoryList
-(id)initWithItems:(NSMutableArray *)items URLs:(NSMutableArray *)urls target:(id)target {
        _historyItems=items;
        _urls=urls;
        _target=target;
        return self;
}
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
        return [_historyItems count];
}
-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
        UITableViewCell *cell=[tableView dequeueReusableCellWithIdentifier:@"history"];
        if(cell == nil)
        {
                cell = [[[UITableViewCell alloc] initWithFrame:CGRectZero reuseIdentifier:@"history"] autorelease];
        }
        [cell setText:[[_historyItems objectAtIndex:indexPath.row] retain]];
        return cell;

}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	NSLog(@"MyHistoryList:tableView:didselectrow:url string:%@",[_urls objectAtIndex:indexPath.row]);	
        [_target switchToContentWithURL:[NSURL URLWithString:[[_urls objectAtIndex:indexPath.row] stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]]];
}

@end
