/*****************************************************************************
 *
 *   main() of the SkyEmu app bundles for macOS and iOS. In this fork sokol's
 *   entry points are named main_macos() and main_ios() so that host apps can
 *   embed the static libraries (see docs/EMBEDDING.md); the app bundles start
 *   them from here, like SkyEmu.exe starts SkyEmu.dll on Windows.
 *
**/
#if defined(SE_PLATFORM_IOS)
int main_ios(int argc, char* argv[]);
int main(int argc, char* argv[]){return main_ios(argc,argv);}
#else
int main_macos(int argc, char* argv[]);
int main(int argc, char* argv[]){return main_macos(argc,argv);}
#endif
