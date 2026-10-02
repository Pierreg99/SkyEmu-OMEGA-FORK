// macOS versions of the host callbacks of ios_support.m. The HTTP control server calls them from its own
// thread; they are passed on to the app delegate on the main thread, where a host app that embeds SkyEmu
// implements handleRemoteKeycodeWithData1:data2:, handlePing and handleOpenExternalMenu (sokol's own app
// delegate ignores them). sokol's sapp_ios_* versions of these only exist on iOS.
#include "ios_support.h"
#include <string.h>
#include <stdlib.h>
#import <AppKit/AppKit.h>

static void se_macos_notify_delegate(SEL selector){
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    id delegate = [NSApplication sharedApplication].delegate;
    if([delegate respondsToSelector:selector])[delegate performSelector:selector];
  }];
}

void se_ios_remote_keycode_callback(const char *data1, const char* data2){
  // The strings belong to the HTTP request, copy them for the main thread
  char* key = strdup(data1? data1 : "");
  char* value = strdup(data2? data2 : "");
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    id delegate = [NSApplication sharedApplication].delegate;
    SEL selector = @selector(handleRemoteKeycodeWithData1:data2:);
    if([delegate respondsToSelector:selector]){
      [delegate performSelector:selector withObject:[NSString stringWithUTF8String:key]
                                     withObject:[NSString stringWithUTF8String:value]];
    }
    free(key);
    free(value);
  }];
}

void se_ios_ping(void){se_macos_notify_delegate(@selector(handlePing));}

void se_ios_open_external_menu(void){se_macos_notify_delegate(@selector(handleOpenExternalMenu));}
