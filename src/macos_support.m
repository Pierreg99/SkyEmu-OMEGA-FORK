// macOS versions of the host callbacks of ios_support.m: the HTTP control server calls them from its
// own thread, and sokol passes them on to the app delegate on the main thread (a host app that embeds
// SkyEmu implements handleRemoteKeycodeWithData1:data2:, handlePing and handleOpenExternalMenu).
#include "ios_support.h"
#include "sokol_app.h"
#include <string.h>
#include <stdlib.h>
#import <Foundation/Foundation.h>

void se_ios_remote_keycode_callback(const char *data1, const char* data2){
  // The strings belong to the HTTP request, copy them for the main thread
  char* key = strdup(data1? data1 : "");
  char* value = strdup(data2? data2 : "");
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    sapp_ios_remote_keycode_callback(key, value);
    free(key);
    free(value);
  }];
}

void se_ios_ping(void){
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    sapp_ios_ping_callback();
  }];
}

void se_ios_open_external_menu(void){
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    sapp_ios_open_external_menu_callback();
  }];
}
