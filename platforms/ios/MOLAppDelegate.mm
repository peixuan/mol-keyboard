// SPDX-License-Identifier: Apache-2.0
#import "MOLAppDelegate.h"

#import "MOLViewController.h"

@implementation MOLAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
  (void)application;
  (void)launchOptions;
  self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
  self.window.rootViewController = [[MOLViewController alloc] init];
  [self.window makeKeyAndVisible];
  return YES;
}

@end
