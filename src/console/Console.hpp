#ifndef CONSOLE_CONSOLE_HPP
#define CONSOLE_CONSOLE_HPP

#include "console/Types.hpp"
#include "event/Types.hpp"

#define CONSOLE_LINES_MAX     256
#define CONSOLE_LINE_LENGTH   1024
#define CONSOLE_LINE_PREALLOC 16

enum HIGHLIGHTSTATE {
  HS_NONE         = 0,
  HS_HIGHLIGHTING = 1,
  HS_ENDHIGHLIGHT = 2,
  NUM_HIGHLIGHTSTATES
};

//=============================================================================
int   ConsoleAccessGetEnabled ();
void  ConsoleAccessSetEnabled (int enable);
int   ConsoleGetActive ();
float ConsoleGetFontHeight ();
float ConsoleGetLines ();
float ConsoleGetHeight ();
KEY   ConsoleGetHotKey ();

CONSOLERESIZESTATE ConsoleGetResizeState ();


//=============================================================================
void  ConsoleSetActive (int active);
void  ConsoleSetHotKey (KEY hotkey);
void  ConsoleSetHeight (float height);
void  ConsoleSetResizeState (CONSOLERESIZESTATE state);

void  ConsolePostClose ();
void  ConsoleWrite (const char* str, COLOR_T color);
void  ConsoleWriteA (const char* str, COLOR_T color, ...);
void  ConsoleClear ();
void  ConsoleScreenAnimate (float elapsedSec);
void  ConsoleScreenInitialize (const char* title);

void  RegisterHandlers ();

#endif  // ifndef CONSOLE_CONSOLE_HPP
