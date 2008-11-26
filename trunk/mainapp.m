
#import <UIKit/UIApplication.h>
#import <Foundation/NSObject.h>
#import "Application.h"
int myargc;
char **myargv;

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    myargc=argc;
    myargv=argv;
    return UIApplicationMain(argc, argv, @"Application", @"Application");
}


