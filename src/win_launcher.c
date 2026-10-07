/*****************************************************************************
 *
 *   SkyEmu.exe: starts SkyEmu.dll, the Windows build of SkyEmu that host apps
 *   load too (see docs/EMBEDDING.md), so it can be run like any other app.
 *   Command line arguments, such as a ROM to open, are passed on as UTF-8
 *   like sokol does for the other platforms.
 *
**/
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include "skyemu_dll.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show){
  (void)instance; (void)previous; (void)command_line; (void)show;
  int argc = 0;
  LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(),&argc);
  if(!wide_argv)return win_main(__argc,__argv);
  char** argv = (char**)calloc((size_t)argc+1,sizeof(char*));
  for(int i=0;argv&&i<argc;++i){
    int size = WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,NULL,0,NULL,NULL);
    argv[i] = (char*)calloc(size>0? (size_t)size : 1,1);
    if(argv[i]&&size>0)WideCharToMultiByte(CP_UTF8,0,wide_argv[i],-1,argv[i],size,NULL,NULL);
  }
  LocalFree(wide_argv);
  if(!argv)return win_main(__argc,__argv);
  // The emulator keeps argv, so it lives until the process ends
  return win_main(argc,argv);
}
