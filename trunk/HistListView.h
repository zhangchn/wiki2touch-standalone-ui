#include <Foundation/Foundation.h>
#include <UIKit/UIKit.h>

@interface MyHistoryList: NSObject <UITableViewDelegate,UITableViewDataSource>{
        NSMutableArray *_historyItems;
        NSMutableArray *_urls;
        id _target;

}
-(id)initWithItems:(NSMutableArray *)items URLs:(NSMutableArray *)urls target:(id)target;
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section;
-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath;
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath;

@end
