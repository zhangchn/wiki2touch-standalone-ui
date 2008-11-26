#include <Foundation/Foundation.h>
#include <UIKit/UIKit.h>

@interface LangList: NSObject <UITableViewDelegate,UITableViewDataSource>{
        NSArray *_langs;
        id _target;

}
-(void)setLangsWithItems:(NSArray *)langs;
-(id)initWithItems:(NSArray *)langs target:(id)target;
-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section;
-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath;
-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath;

@end
