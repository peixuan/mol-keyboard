// SPDX-License-Identifier: Apache-2.0
#import "MOLViewController.h"

#import <WebKit/WebKit.h>

#import "MOLNativeAudioController.h"

#include <cstdint>
#include <cstdio>

namespace {

NSString* const kBridgeName = @"MolKeyboardNative";
NSString* const kApplicationScheme = @"mol-keyboard";
NSString* const kApplicationHost = @"app";
constexpr std::uint64_t kHardwareGesturePrefix = 1ULL << 52U;

NSInteger note_for_usage(UIKeyboardHIDUsage usage) {
  switch (usage) {
    case UIKeyboardHIDUsageKeyboardZ:
      return 60;
    case UIKeyboardHIDUsageKeyboardS:
      return 61;
    case UIKeyboardHIDUsageKeyboardX:
      return 62;
    case UIKeyboardHIDUsageKeyboardD:
      return 63;
    case UIKeyboardHIDUsageKeyboardC:
      return 64;
    case UIKeyboardHIDUsageKeyboardV:
      return 65;
    case UIKeyboardHIDUsageKeyboardG:
      return 66;
    case UIKeyboardHIDUsageKeyboardB:
      return 67;
    case UIKeyboardHIDUsageKeyboardH:
      return 68;
    case UIKeyboardHIDUsageKeyboardN:
      return 69;
    case UIKeyboardHIDUsageKeyboardJ:
      return 70;
    case UIKeyboardHIDUsageKeyboardM:
      return 71;
    case UIKeyboardHIDUsageKeyboardQ:
      return 72;
    case UIKeyboardHIDUsageKeyboard2:
      return 73;
    case UIKeyboardHIDUsageKeyboardW:
      return 74;
    case UIKeyboardHIDUsageKeyboard3:
      return 75;
    case UIKeyboardHIDUsageKeyboardE:
      return 76;
    case UIKeyboardHIDUsageKeyboardR:
      return 77;
    case UIKeyboardHIDUsageKeyboard5:
      return 78;
    case UIKeyboardHIDUsageKeyboardT:
      return 79;
    case UIKeyboardHIDUsageKeyboard6:
      return 80;
    case UIKeyboardHIDUsageKeyboardY:
      return 81;
    case UIKeyboardHIDUsageKeyboard7:
      return 82;
    case UIKeyboardHIDUsageKeyboardU:
      return 83;
    case UIKeyboardHIDUsageKeyboardI:
      return 84;
    case UIKeyboardHIDUsageKeyboard9:
      return 85;
    case UIKeyboardHIDUsageKeyboardO:
      return 86;
    case UIKeyboardHIDUsageKeyboard0:
      return 87;
    case UIKeyboardHIDUsageKeyboardP:
      return 88;
    case UIKeyboardHIDUsageKeyboardOpenBracket:
      return 89;
    default:
      return -1;
  }
}

NSString* mime_type(NSString* path) {
  NSString* extension = path.pathExtension.lowercaseString;
  if ([extension isEqualToString:@"html"]) return @"text/html";
  if ([extension isEqualToString:@"css"]) return @"text/css";
  if ([extension isEqualToString:@"js"]) return @"application/javascript";
  if ([extension isEqualToString:@"json"] || [extension isEqualToString:@"map"]) {
    return @"application/json";
  }
  if ([extension isEqualToString:@"webmanifest"]) return @"application/manifest+json";
  if ([extension isEqualToString:@"svg"]) return @"image/svg+xml";
  if ([extension isEqualToString:@"wasm"]) return @"application/wasm";
  if ([extension isEqualToString:@"png"]) return @"image/png";
  return @"application/octet-stream";
}

}  // namespace

@protocol MOLKeyboardWebViewDelegate <NSObject>

- (BOOL)handleHardwareUsage:(UIKeyboardHIDUsage)usage pressed:(BOOL)pressed;

@end

@interface MOLKeyboardWebView : WKWebView

@property(nonatomic, weak) id<MOLKeyboardWebViewDelegate> keyboardDelegate;
@property(nonatomic, readonly) NSMutableSet<NSNumber*>* activeUsages;

- (void)releaseAllHardwareKeys;
- (void)finishPresses:(NSSet<UIPress*>*)presses
                event:(UIPressesEvent*)event
            cancelled:(BOOL)cancelled;

@end

@implementation MOLKeyboardWebView {
  NSMutableSet<NSNumber*>* _activeUsages;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration*)configuration {
  self = [super initWithFrame:frame configuration:configuration];
  if (self != nil) _activeUsages = [[NSMutableSet alloc] initWithCapacity:31U];
  return self;
}

- (NSMutableSet<NSNumber*>*)activeUsages {
  return _activeUsages;
}

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
  NSMutableSet<UIPress*>* unhandled = [presses mutableCopy];
  for (UIPress* press in presses) {
    UIKey* key = press.key;
    if (key == nil) continue;
    NSNumber* usage = @(key.keyCode);
    const BOOL supported =
        note_for_usage(key.keyCode) >= 0 || key.keyCode == UIKeyboardHIDUsageKeyboardSpacebar;
    if (!supported) continue;
    if (![_activeUsages containsObject:usage]) {
      if (![self.keyboardDelegate handleHardwareUsage:key.keyCode pressed:YES]) continue;
      [_activeUsages addObject:usage];
    }
    [unhandled removeObject:press];
  }
  if (unhandled.count > 0U) [super pressesBegan:unhandled withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
  [self finishPresses:presses event:event cancelled:NO];
}

- (void)pressesCancelled:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
  [self finishPresses:presses event:event cancelled:YES];
}

- (void)finishPresses:(NSSet<UIPress*>*)presses
                event:(UIPressesEvent*)event
            cancelled:(BOOL)cancelled {
  NSMutableSet<UIPress*>* unhandled = [presses mutableCopy];
  for (UIPress* press in presses) {
    UIKey* key = press.key;
    if (key == nil) continue;
    NSNumber* usage = @(key.keyCode);
    if (![_activeUsages containsObject:usage]) continue;
    (void)[self.keyboardDelegate handleHardwareUsage:key.keyCode pressed:NO];
    [_activeUsages removeObject:usage];
    [unhandled removeObject:press];
  }
  if (unhandled.count == 0U) return;
  if (cancelled) {
    [super pressesCancelled:unhandled withEvent:event];
  } else {
    [super pressesEnded:unhandled withEvent:event];
  }
}

- (void)releaseAllHardwareKeys {
  for (NSNumber* value in _activeUsages.allObjects) {
    (void)[self.keyboardDelegate
        handleHardwareUsage:static_cast<UIKeyboardHIDUsage>(value.integerValue)
                    pressed:NO];
  }
  [_activeUsages removeAllObjects];
}

@end

@interface MOLLocalSchemeHandler : NSObject <WKURLSchemeHandler>

- (nullable NSString*)textEncodingForPath:(NSString*)path;
- (NSError*)notFoundError;
- (void)failTask:(id<WKURLSchemeTask>)task;

@end

@interface MOLWeakScriptMessageHandler : NSObject <WKScriptMessageHandlerWithReply>

@property(nonatomic, weak) id<WKScriptMessageHandlerWithReply> delegate;

- (instancetype)initWithDelegate:(id<WKScriptMessageHandlerWithReply>)delegate;

@end

@implementation MOLWeakScriptMessageHandler

- (instancetype)initWithDelegate:(id<WKScriptMessageHandlerWithReply>)delegate {
  self = [super init];
  if (self != nil) self.delegate = delegate;
  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message
                 replyHandler:(void (^)(id _Nullable, NSString* _Nullable))replyHandler {
  id<WKScriptMessageHandlerWithReply> delegate = self.delegate;
  if (delegate == nil) {
    replyHandler(nil, @"Native bridge is unavailable");
    return;
  }
  [delegate userContentController:userContentController
          didReceiveScriptMessage:message
                     replyHandler:replyHandler];
}

@end

@implementation MOLLocalSchemeHandler

- (void)webView:(WKWebView*)webView startURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  (void)webView;
  NSURL* url = urlSchemeTask.request.URL;
  if (![url.scheme isEqualToString:kApplicationScheme] ||
      ![url.host isEqualToString:kApplicationHost]) {
    [self failTask:urlSchemeTask];
    return;
  }
  NSString* path = url.path.stringByRemovingPercentEncoding;
  if ([path hasPrefix:@"/"]) path = [path substringFromIndex:1U];
  NSArray<NSString*>* components = [path componentsSeparatedByString:@"/"];
  if (path.length == 0U || [path containsString:@"\\"] || [components containsObject:@"."] ||
      [components containsObject:@".."] || [components containsObject:@""]) {
    [self failTask:urlSchemeTask];
    return;
  }
  NSURL* root = [NSBundle.mainBundle URLForResource:@"web" withExtension:nil];
  if (root == nil) {
    [self failTask:urlSchemeTask];
    return;
  }
  NSString* rootPath = root.URLByResolvingSymlinksInPath.path.stringByStandardizingPath;
  NSURL* file = [root URLByAppendingPathComponent:path isDirectory:NO];
  NSString* filePath = file.URLByResolvingSymlinksInPath.path.stringByStandardizingPath;
  NSString* requiredPrefix = [rootPath stringByAppendingString:@"/"];
  if (![filePath hasPrefix:requiredPrefix]) {
    [self failTask:urlSchemeTask];
    return;
  }
  NSError* error = nil;
  NSData* data = [NSData dataWithContentsOfURL:file options:NSDataReadingMappedIfSafe error:&error];
  if (data == nil) {
    [urlSchemeTask didFailWithError:error ?: [self notFoundError]];
    return;
  }
  NSURLResponse* response = [[NSURLResponse alloc] initWithURL:url
                                                      MIMEType:mime_type(path)
                                         expectedContentLength:static_cast<NSInteger>(data.length)
                                              textEncodingName:[self textEncodingForPath:path]];
  [urlSchemeTask didReceiveResponse:response];
  [urlSchemeTask didReceiveData:data];
  [urlSchemeTask didFinish];
}

- (void)webView:(WKWebView*)webView stopURLSchemeTask:(id<WKURLSchemeTask>)urlSchemeTask {
  (void)webView;
  (void)urlSchemeTask;
}

- (nullable NSString*)textEncodingForPath:(NSString*)path {
  NSString* type = mime_type(path);
  return [type hasPrefix:@"text/"] || [type isEqualToString:@"application/javascript"] ||
                 [type isEqualToString:@"application/json"] ||
                 [type isEqualToString:@"application/manifest+json"]
             ? @"utf-8"
             : nil;
}

- (NSError*)notFoundError {
  return [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil];
}

- (void)failTask:(id<WKURLSchemeTask>)task {
  [task didFailWithError:[self notFoundError]];
}

@end

@interface MOLViewController () <MOLKeyboardWebViewDelegate,
                                 WKNavigationDelegate,
                                 WKScriptMessageHandlerWithReply>

- (void)configureWebViewWithRuleList:(WKContentRuleList*)ruleList;
- (void)showConfigurationFailure;
- (void)applicationDidBecomeActive:(NSNotification*)notification;
- (void)applicationWillResignActive:(NSNotification*)notification;
- (void)applicationDidEnterBackground:(NSNotification*)notification;
- (void)applicationWillTerminate:(NSNotification*)notification;
- (void)completeSimulatorSmokeWithResult:(nullable id)result error:(nullable NSError*)error;

@end

@implementation MOLViewController {
  MOLNativeAudioController* _audioController;
  MOLKeyboardWebView* _webView;
  BOOL _simulatorSmokeStarted;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorWithRed:0.949F green:0.937F blue:0.906F alpha:1.0F];
  _audioController = [[MOLNativeAudioController alloc] init];

  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  [center addObserver:self
             selector:@selector(applicationDidBecomeActive:)
                 name:UIApplicationDidBecomeActiveNotification
               object:nil];
  [center addObserver:self
             selector:@selector(applicationWillResignActive:)
                 name:UIApplicationWillResignActiveNotification
               object:nil];
  [center addObserver:self
             selector:@selector(applicationDidEnterBackground:)
                 name:UIApplicationDidEnterBackgroundNotification
               object:nil];
  [center addObserver:self
             selector:@selector(applicationWillTerminate:)
                 name:UIApplicationWillTerminateNotification
               object:nil];

  NSString* rules =
      @"[{\"trigger\":{\"url-filter\":\"^https?://.*\"},\"action\":{\"type\":\"block\"}},"
       "{\"trigger\":{\"url-filter\":\"^wss?://.*\"},\"action\":{\"type\":\"block\"}}]";
  __weak MOLViewController* weakSelf = self;
  [WKContentRuleListStore.defaultStore
      compileContentRuleListForIdentifier:@"cn.zhangpeixuan.molkeyboard.offline-only"
                   encodedContentRuleList:rules
                        completionHandler:^(WKContentRuleList* ruleList, NSError* error) {
                          dispatch_async(dispatch_get_main_queue(), ^{
                            MOLViewController* strongSelf = weakSelf;
                            if (strongSelf == nil) return;
                            if (ruleList == nil || error != nil) {
                              [strongSelf showConfigurationFailure];
                              return;
                            }
                            [strongSelf configureWebViewWithRuleList:ruleList];
                          });
                        }];
}

- (void)dealloc {
  [_webView.configuration.userContentController
      removeScriptMessageHandlerForName:kBridgeName
                           contentWorld:WKContentWorld.pageWorld];
  [NSNotificationCenter.defaultCenter removeObserver:self];
  [_audioController stopUserAudio];
}

- (void)configureWebViewWithRuleList:(WKContentRuleList*)ruleList {
  if (_webView != nil) return;
  WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
  configuration.websiteDataStore = WKWebsiteDataStore.defaultDataStore;
  configuration.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAll;
  configuration.defaultWebpagePreferences.allowsContentJavaScript = YES;
  MOLLocalSchemeHandler* schemeHandler = [[MOLLocalSchemeHandler alloc] init];
  [configuration setURLSchemeHandler:schemeHandler forURLScheme:kApplicationScheme];
  [configuration.userContentController addContentRuleList:ruleList];
  NSString* source =
      @"Object.defineProperty(window,'MolKeyboardNative',{value:Object.freeze({"
       "dispatch(request){return window.webkit.messageHandlers.MolKeyboardNative"
       ".postMessage(request);}}),writable:false,configurable:false});"
       "Object.defineProperty(window,'MolKeyboardPlatform',{value:'ios',writable:false,"
       "configurable:false});";
  WKUserScript* bridgeScript =
      [[WKUserScript alloc] initWithSource:source
                             injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                          forMainFrameOnly:YES];
  [configuration.userContentController addUserScript:bridgeScript];
  MOLWeakScriptMessageHandler* messageHandler =
      [[MOLWeakScriptMessageHandler alloc] initWithDelegate:self];
  [configuration.userContentController addScriptMessageHandlerWithReply:messageHandler
                                                           contentWorld:WKContentWorld.pageWorld
                                                                   name:kBridgeName];

  _webView = [[MOLKeyboardWebView alloc] initWithFrame:CGRectZero configuration:configuration];
  _webView.keyboardDelegate = self;
  _webView.navigationDelegate = self;
  _webView.opaque = NO;
  _webView.backgroundColor = self.view.backgroundColor;
  _webView.scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_webView];
  [NSLayoutConstraint activateConstraints:@[
    [_webView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_webView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [_webView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_webView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
  NSURL* url = [NSURL URLWithString:@"mol-keyboard://app/index.html"];
  [_webView loadRequest:[NSURLRequest requestWithURL:url
                                         cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                     timeoutInterval:10.0]];
}

- (void)showConfigurationFailure {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.text = @"MoL Keyboard could not initialize its offline content policy.";
  label.numberOfLines = 0;
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:label];
  [NSLayoutConstraint activateConstraints:@[
    [label.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                                        constant:24.0],
    [label.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                                         constant:-24.0],
    [label.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message
                 replyHandler:(void (^)(id _Nullable, NSString* _Nullable))replyHandler {
  (void)userContentController;
  WKSecurityOrigin* origin = message.frameInfo.securityOrigin;
  if (!message.frameInfo.mainFrame || ![message.name isEqualToString:kBridgeName] ||
      ![origin.protocol isEqualToString:kApplicationScheme] ||
      ![origin.host isEqualToString:kApplicationHost] ||
      ![message.body isKindOfClass:NSString.class]) {
    replyHandler(nil, @"Native bridge origin or payload is invalid");
    return;
  }
  replyHandler([_audioController handleRequestText:message.body], nil);
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
  (void)webView;
  NSURL* url = navigationAction.request.URL;
  const BOOL allowed = [url.scheme isEqualToString:kApplicationScheme] &&
                       [url.host isEqualToString:kApplicationHost];
  decisionHandler(allowed ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [webView loadRequest:[NSURLRequest
                           requestWithURL:[NSURL URLWithString:@"mol-keyboard://app/index.html"]]];
}

- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
  (void)navigation;
  if (_simulatorSmokeStarted ||
      ![NSProcessInfo.processInfo.arguments containsObject:@"--mol-simulator-smoke"]) {
    return;
  }
  _simulatorSmokeStarted = YES;
  NSString* script =
      @"const bridge = window.MolKeyboardNative;"
       "const app = document.querySelector('mol-keyboard-app');"
       "const uiReady = app !== null && app.querySelector('[data-action=\"start\"]') !== null;"
       "const statusText = await bridge.dispatch("
       "'{\"version\":1,\"method\":\"runtime.status\",\"params\":{}}');"
       "const rejectedText = await bridge.dispatch("
       "'{\"version\":2,\"method\":\"runtime.status\",\"params\":{}}');"
       "return {platform: window.MolKeyboardPlatform, uiReady, "
       "status: JSON.parse(statusText), rejected: JSON.parse(rejectedText)};";
  __weak MOLViewController* weakSelf = self;
  [webView callAsyncJavaScript:script
                     arguments:@{}
                       inFrame:nil
                  contentWorld:WKContentWorld.pageWorld
             completionHandler:^(id result, NSError* error) {
               [weakSelf completeSimulatorSmokeWithResult:result error:error];
             }];
}

- (void)completeSimulatorSmokeWithResult:(id)result error:(NSError*)error {
  NSString* failure = nil;
  if (error != nil) {
    failure = error.localizedDescription;
  } else if (![result isKindOfClass:NSDictionary.class]) {
    failure = @"JavaScript result is not an object";
  } else {
    NSDictionary<NSString*, id>* payload = result;
    NSDictionary<NSString*, id>* status =
        [payload[@"status"] isKindOfClass:NSDictionary.class] ? payload[@"status"] : nil;
    NSDictionary<NSString*, id>* rejected =
        [payload[@"rejected"] isKindOfClass:NSDictionary.class] ? payload[@"rejected"] : nil;
    if (![payload[@"platform"] isEqual:@"ios"]) {
      failure = @"native platform injection is missing";
    } else if (![payload[@"uiReady"] boolValue]) {
      failure = @"packaged production UI did not initialize";
    } else if (![status[@"ok"] boolValue] || [status[@"audioApi"] integerValue] != 1 ||
               ![status[@"callbackCount"] isKindOfClass:NSNumber.class]) {
      failure = @"valid runtime.status bridge response is invalid";
    } else if ([rejected[@"ok"] boolValue] || ![rejected[@"error"] isKindOfClass:NSString.class] ||
               [rejected[@"error"] length] == 0U) {
      failure = @"invalid bridge version was not rejected";
    }
  }
  if (failure == nil) {
    std::fputs("MOL_IOS_SIMULATOR_SMOKE_PASS\n", stderr);
  } else {
    NSString* singleLine = [failure stringByReplacingOccurrencesOfString:@"\n" withString:@" "];
    std::fprintf(stderr, "MOL_IOS_SIMULATOR_SMOKE_FAIL %s\n", singleLine.UTF8String);
  }
  std::fflush(stderr);
}

- (BOOL)handleHardwareUsage:(UIKeyboardHIDUsage)usage pressed:(BOOL)pressed {
  const std::uint64_t gestureId =
      kHardwareGesturePrefix | (static_cast<std::uint64_t>(usage) & 0xFFFFULL);
  if (usage == UIKeyboardHIDUsageKeyboardSpacebar) {
    return [_audioController submitHardwareSustain:pressed gestureId:gestureId];
  }
  const NSInteger note = note_for_usage(usage);
  if (note < 0) return NO;
  return [_audioController submitHardwareNote:static_cast<uint8_t>(note)
                                           on:pressed
                                    gestureId:gestureId];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  (void)notification;
  [_audioController applicationDidBecomeActive];
}

- (void)applicationWillResignActive:(NSNotification*)notification {
  (void)notification;
  [_webView releaseAllHardwareKeys];
  [_audioController applicationWillResignActive];
}

- (void)applicationDidEnterBackground:(NSNotification*)notification {
  (void)notification;
  [_webView releaseAllHardwareKeys];
  [_audioController applicationDidEnterBackground];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
  (void)notification;
  [_webView releaseAllHardwareKeys];
  [_audioController stopUserAudio];
}

@end
