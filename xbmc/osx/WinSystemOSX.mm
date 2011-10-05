/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef __APPLE__

//hack around problem with xbmc's typedef int BOOL
// and obj-c's typedef unsigned char BOOL
#define BOOL XBMC_BOOL 
#include "WinSystemOSX.h"
#include "Settings.h"
#include "GUISettings.h"
#include "KeyboardStat.h"
#include "utils/log.h"
#include "XBMCHelper.h"
#include "SystemInfo.h"
#undef BOOL

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <Carbon/Carbon.h>   // ShowMenuBar, HideMenuBar


#define MAX_DISPLAYS 32
static NSWindow* blankingWindows[MAX_DISPLAYS];

void* CWinSystemOSX::m_lastOwnedContext = 0;

//------------------------------------------------------------------------------------------
Boolean GetDictionaryBoolean(CFDictionaryRef theDict, const void* key)
{
        // get a boolean from the dictionary
        Boolean value = false;
        CFBooleanRef boolRef;
        boolRef = (CFBooleanRef)CFDictionaryGetValue(theDict, key);
        if (boolRef != NULL)
                value = CFBooleanGetValue(boolRef);
        return value;
}
//------------------------------------------------------------------------------------------
long GetDictionaryLong(CFDictionaryRef theDict, const void* key)
{
        // get a long from the dictionary
        long value = 0;
        CFNumberRef numRef;
        numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
        if (numRef != NULL)
                CFNumberGetValue(numRef, kCFNumberLongType, &value);
        return value;
}
//------------------------------------------------------------------------------------------
int GetDictionaryInt(CFDictionaryRef theDict, const void* key)
{
        // get a long from the dictionary
        int value = 0;
        CFNumberRef numRef;
        numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
        if (numRef != NULL)
                CFNumberGetValue(numRef, kCFNumberIntType, &value);
        return value;
}
//------------------------------------------------------------------------------------------
float GetDictionaryFloat(CFDictionaryRef theDict, const void* key)
{
        // get a long from the dictionary
        int value = 0;
        CFNumberRef numRef;
        numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
        if (numRef != NULL)
                CFNumberGetValue(numRef, kCFNumberFloatType, &value);
        return value;
}
//------------------------------------------------------------------------------------------
double GetDictionaryDouble(CFDictionaryRef theDict, const void* key)
{
        // get a long from the dictionary
        double value = 0.0;
        CFNumberRef numRef;
        numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
        if (numRef != NULL)
                CFNumberGetValue(numRef, kCFNumberDoubleType, &value);
        return value;
}

//---------------------------------------------------------------------------------
CGDirectDisplayID GetDisplayID(int screen_index)
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  return(displayArray[screen_index]);
}

CGDirectDisplayID GetDisplayIDFromScreen(NSScreen *screen)
{
  NSDictionary* screenInfo = [screen deviceDescription];
  NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"];
  
  return (CGDirectDisplayID)[screenID longValue];
}

int GetDisplayIndex(CGDirectDisplayID display)
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  while (numDisplays > 0)
  {
    if (display == displayArray[--numDisplays])
	  return numDisplays;
  }
  return -1;
}

void BlankOtherDisplays(int screen_index)
{
  int i;
  int numDisplays = [[NSScreen screens] count];
  
  // zero out blankingWindows for debugging
  for (i=0; i<MAX_DISPLAYS; i++)
  {
    blankingWindows[i] = 0;
  }
  
  // Blank.
  for (i=0; i<numDisplays; i++)
  {
    if (i != screen_index)
    {
      // Get the size.
      NSScreen* pScreen = [[NSScreen screens] objectAtIndex:i];
      NSRect    screenRect = [pScreen frame];
          
      // Build a blanking window.
      screenRect.origin = NSZeroPoint;
      blankingWindows[i] = [[NSWindow alloc] initWithContentRect:screenRect
        styleMask:NSBorderlessWindowMask
        backing:NSBackingStoreBuffered
        defer:NO 
        screen:pScreen];
                                            
      [blankingWindows[i] setBackgroundColor:[NSColor blackColor]];
      [blankingWindows[i] setLevel:CGShieldingWindowLevel()];
      [blankingWindows[i] makeKeyAndOrderFront:nil];
    }
  } 
}

void UnblankDisplays(void)
{
  int numDisplays = [[NSScreen screens] count];
  int i = 0;

  for (i=0; i<numDisplays; i++)
  {
    if (blankingWindows[i] != 0)
    {
      // Get rid of the blanking windows we created.
      [blankingWindows[i] close];
      [blankingWindows[i] release];
      blankingWindows[i] = 0;
    }
  }
}

CGDisplayFadeReservationToken DisplayFadeToBlack(void)
{
  // Fade to black to hide resolution-switching flicker and garbage.
  CGDisplayFadeReservationToken fade_token = kCGDisplayFadeReservationInvalidToken;
  if (CGAcquireDisplayFadeReservation (5, &fade_token) == kCGErrorSuccess )
    CGDisplayFade(fade_token, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, TRUE);

  return(fade_token);
}

void DisplayFadeFromBlack(CGDisplayFadeReservationToken fade_token)
{
  if (fade_token != kCGDisplayFadeReservationInvalidToken) 
  {
    CGDisplayFade(fade_token, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, FALSE);
    CGReleaseDisplayFadeReservation(fade_token);
  }
}


//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
CWinSystemOSX::CWinSystemOSX() : CWinSystemBase()
{
  m_eWindowSystem = WINDOW_SYSTEM_OSX;
  m_glContext = 0;
  m_SDLSurface = NULL;
}

CWinSystemOSX::~CWinSystemOSX()
{
};

bool CWinSystemOSX::InitWindowSystem()
{
  SDL_EnableUNICODE(1);

  // set repeat to 10ms to ensure repeat time < frame time
  // so that hold times can be reliably detected
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 10);
  
  if (!CWinSystemBase::InitWindowSystem())
    return false;
  
  return true;
}

bool CWinSystemOSX::DestroyWindowSystem()
{  
  if (m_glContext)
  {
    NSOpenGLContext* oldContext = (NSOpenGLContext*)m_glContext;
    [oldContext release];
    m_glContext = NULL;
  }
  return true;
}

bool CWinSystemOSX::CreateNewWindow(const CStdString& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  m_nWidth  = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  // Enable vertical sync to avoid any tearing.
  SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);  

  m_SDLSurface = SDL_SetVideoMode(m_nWidth, m_nHeight, 0, SDL_OPENGL | SDL_RESIZABLE);
  if (!m_SDLSurface)
    return false;

  // the context SDL creates isn't full screen compatible, so we create new one
  // first, find the current contect and make sure a view is attached
  NSOpenGLContext* cur_context = [NSOpenGLContext currentContext];
  NSView* view = [cur_context view];
  if (!view)
    return false;
  
  // disassociate view from context
  [cur_context clearDrawable];
  
  // release the context
  if (m_lastOwnedContext == cur_context)
  {
    [ NSOpenGLContext clearCurrentContext ];
    [ cur_context clearDrawable ];
    [ cur_context release ];
  }
  
  // create a new context
  NSOpenGLContext* new_context = (NSOpenGLContext*)CreateWindowedContext(nil);
  if (!new_context)
    return false;
  
  // associate with current view
  [new_context setView:view];
  [new_context makeCurrentContext];

  // set the window title
  NSString *string;
  string = [ [ NSString alloc ] initWithUTF8String:"XBMC Media Center" ];
  [ [ [new_context view] window] setTitle:string ];
  [ string release ];

  m_glContext = new_context;
  m_lastOwnedContext = new_context;
  m_bWindowCreated = true;

  return true;
}

bool CWinSystemOSX::DestroyWindow()
{
  return true;
}
    
extern "C" void SDL_SetWidthHeight(int w, int h);
bool CWinSystemOSX::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  if (!m_glContext)
    return false;
  
  NSOpenGLContext* context = [NSOpenGLContext currentContext];
  NSView* view;
  NSWindow* window;
  
  view = [context view];
  if (view && (newWidth > 0) && (newHeight > 0))
  {
    window = [view window];
    if (window)
    {
      [window setContentSize:NSMakeSize(newWidth, newHeight)];
      [window update];
      [view setFrameSize:NSMakeSize(newWidth, newHeight)];
      [context update];
    }
  }

  // HACK: resize SDL's view manually so that mouse bounds are correctly updated.
  // there are two parts to this, the internal SDL (current_video->screen) and
  // the cocoa view ( handled in SetFullScreen).
  SDL_SetWidthHeight(newWidth, newHeight);

  [context makeCurrentContext];
  
  m_nWidth = newWidth;
  m_nHeight = newHeight;
  m_glContext = context;
  
  return true;
}

bool CWinSystemOSX::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{  
  static NSWindow* windowedFullScreenwindow = NULL;
  static NSScreen* last_window_screen = NULL;
  static NSPoint last_window_origin;
  static NSView* last_view = NULL;
  static NSSize last_view_size;
  static NSPoint last_view_origin;
  bool was_fullscreen = m_bFullScreen;
  NSOpenGLContext* cur_context;
  
  // If we're already fullscreen then we must be moving to a different display.
  // Recurse to reset fullscreen mode and then continue.
  if (was_fullscreen && fullScreen)
  {
    RESOLUTION_INFO& window = g_settings.m_ResInfo[RES_WINDOW];
    CWinSystemOSX::SetFullScreen(false, window, blankOtherDisplays);
  }
  
  m_nWidth      = res.iWidth;
  m_nHeight     = res.iHeight;
  m_bFullScreen = fullScreen;

  cur_context = [NSOpenGLContext currentContext];
  if (!cur_context)
    return false;
  
  if (m_bFullScreen)
  {
    // FullScreen Mode
    NSOpenGLContext* newContext = NULL;
  
    // Fade to black to hide resolution-switching flicker and garbage.
    CGDisplayFadeReservationToken fade_token = DisplayFadeToBlack();
    
    // Save info about the windowed context so we can restore it when returning to windowed.
    last_view = [cur_context view];
    last_view_size = [last_view frame].size;
    last_view_origin = [last_view frame].origin;
    last_window_screen = [[last_view window] screen];
    last_window_origin = [[last_view window] frame].origin;
    
    if (g_guiSettings.GetBool("videoscreen.fakefullscreen"))
    {
      // This is Cocca Windowed FullScreen Mode
      // Get the screen rect of our current display
      NSScreen* pScreen = [[NSScreen screens] objectAtIndex:res.iScreen];
      NSRect    screenRect = [pScreen frame];
      
      // remove frame origin offset of orginal display
      screenRect.origin = NSZeroPoint;
      
      // make a new window to act as the windowedFullScreen
      windowedFullScreenwindow = [[NSWindow alloc] initWithContentRect:screenRect
        styleMask:NSBorderlessWindowMask
        backing:NSBackingStoreBuffered
        defer:NO 
        screen:pScreen];
                                              
      [windowedFullScreenwindow setBackgroundColor:[NSColor blackColor]];
      [windowedFullScreenwindow makeKeyAndOrderFront:nil];
      
      // make our window the same level as the rest to enable cmd+tab switching
      [windowedFullScreenwindow setLevel:NSNormalWindowLevel]; 
      // this will make our window topmost and hide all system messages
      //[windowedFullScreenwindow setLevel:CGShieldingWindowLevel()];

      // ...and the original one beneath it and on the same screen.
      [[last_view window] setLevel:NSNormalWindowLevel-1];
      [[last_view window] setFrameOrigin:[pScreen frame].origin];
      // expand the mouse bounds in SDL view to fullscreen
      [ last_view setFrameOrigin:NSMakePoint(0.0, 0.0)];
      [ last_view setFrameSize:NSMakeSize(m_nWidth, m_nHeight) ];
          
      NSView* blankView = [[NSView alloc] init];
      [windowedFullScreenwindow setContentView:blankView];
      [windowedFullScreenwindow setContentSize:NSMakeSize(m_nWidth, m_nHeight)];
      [windowedFullScreenwindow update];
      [blankView setFrameSize:NSMakeSize(m_nWidth, m_nHeight)];
      
      // Obtain windowed pixel format and create a new context.
      newContext = (NSOpenGLContext*)CreateWindowedContext((void* )cur_context);
      [newContext setView:blankView];
      
      // Hide the menu bar.
      if (GetDisplayID(res.iScreen) == kCGDirectMainDisplay)
        HideMenuBar();
          
      // Blank other displays if requested.
      if (blankOtherDisplays)
        BlankOtherDisplays(res.iScreen);
    }
    else
    {
      // hide the window
      [[last_view window] setFrameOrigin:[last_window_screen frame].origin];
      // expand the mouse bounds in SDL view to fullscreen
      [ last_view setFrameOrigin:NSMakePoint(0.0, 0.0)];
      [ last_view setFrameSize:NSMakeSize(m_nWidth, m_nHeight) ];

      // This is OpenGL FullScreen Mode
      // create our new context (sharing with the current one)
      newContext = (NSOpenGLContext*)CreateFullScreenContext(res.iScreen, (void*)cur_context);
      if (!newContext)
        return false;
      
      // clear the current context
      [NSOpenGLContext clearCurrentContext];
              
      // set fullscreen
      [newContext setFullScreen];
      
      // Capture the display before going fullscreen.
      if (blankOtherDisplays == true)
        CGCaptureAllDisplays();
      else
        CGDisplayCapture(GetDisplayID(res.iScreen));

      // If we don't hide menu bar, it will get events and interrupt the program.
      if (GetDisplayID(res.iScreen) == kCGDirectMainDisplay)
        HideMenuBar();
    }

    // Hide the mouse.
    [NSCursor hide];
    
    // Release old context if we created it.
    if (m_lastOwnedContext == cur_context)
    {
      [ NSOpenGLContext clearCurrentContext ];
      [ cur_context clearDrawable ];
      [ cur_context release ];
    }
    
    // activate context
    [newContext makeCurrentContext];
    m_lastOwnedContext = newContext;
    
    DisplayFadeFromBlack(fade_token);
  }
  else
  {
    // Windowed Mode
  	// Fade to black to hide resolution-switching flicker and garbage.
    CGDisplayFadeReservationToken fade_token = DisplayFadeToBlack();
    
    // exit fullscreen
    [cur_context clearDrawable];
    
    [NSCursor unhide];
    
    // Show menubar.
    if (GetDisplayID(res.iScreen) == kCGDirectMainDisplay)
      ShowMenuBar();

    if (g_guiSettings.GetBool("videoscreen.fakefullscreen"))
    {
      // restore the windowed window level
      [[last_view window] setLevel:NSNormalWindowLevel];
      
      // Get rid of the new window we created.
      [windowedFullScreenwindow close];
      [windowedFullScreenwindow release];
      windowedFullScreenwindow = NULL;
      
      // Unblank.
      // Force the unblank when returning from fullscreen, we get called with blankOtherDisplays set false.
      //if (blankOtherDisplays)
        UnblankDisplays();
    }
    else
    {
      // release displays
      CGReleaseAllDisplays();
    }
    
    // create our new context (sharing with the current one)
    NSOpenGLContext* newContext = (NSOpenGLContext*)CreateWindowedContext((void* )cur_context);
    if (!newContext)
      return false;
    
    // Assign view from old context, move back to original screen.
    [newContext setView:last_view];
    [[last_view window] setFrameOrigin:last_window_origin];
    // return the mouse bounds in SDL view to prevous size
    [ last_view setFrameSize:last_view_size ];
    [ last_view setFrameOrigin:last_view_origin ];
    // done with restoring windowed window, don't set last_view to NULL as we can lose it under dual displays.
    //last_window_screen = NULL;
    
    // Release the fullscreen context.
    if (m_lastOwnedContext == cur_context)
    {
      [ NSOpenGLContext clearCurrentContext ];
      [ cur_context clearDrawable ];
      [ cur_context release ];
    }
    
    // Activate context.
    [newContext makeCurrentContext];
    m_lastOwnedContext = newContext;
    
    DisplayFadeFromBlack(fade_token);
  }

  // need to make sure SDL tracks any window size changes
  ResizeWindow(m_nWidth, m_nHeight, -1, -1);

  return true;
}

void CWinSystemOSX::UpdateResolutions()
{
  CWinSystemBase::UpdateResolutions();

  // Add desktop resolution
  int w, h;
  double fps;
  GetScreenResolution(&w, &h, &fps);
  UpdateDesktopResolution(g_settings.m_ResInfo[RES_DESKTOP], 0, w, h, fps);

  // Add full screen settings for additional monitors
  int numDisplays = [[NSScreen screens] count];
  for (int i = 1; i < numDisplays; i++)
  {
    CFDictionaryRef mode = CGDisplayCurrentMode( GetDisplayID(i) );
    w = GetDictionaryInt(mode, kCGDisplayWidth);
    h = GetDictionaryInt(mode, kCGDisplayHeight);
    fps = GetDictionaryDouble(mode, kCGDisplayRefreshRate);
    if ((int)fps == 0)
    {
      // NOTE: The refresh rate will be REPORTED AS 0 for many DVI and notebook displays.
      fps = 60.0;
    }
    CLog::Log(LOGINFO, "Extra display %d is %dx%d\n", i, w, h);

    RESOLUTION_INFO res;

    UpdateDesktopResolution(res, i, w, h, fps);
    g_graphicsContext.ResetOverscan(res);
    g_settings.m_ResInfo.push_back(res);
  }
  
  //GetVideoModes();
}

/*
void* Cocoa_GL_CreateContext(void* pixFmt, void* shareCtx)
{
  if (!pixFmt)
    return nil;
    
  NSOpenGLContext* newContext = [[NSOpenGLContext alloc] initWithFormat:(NSOpenGLPixelFormat*)pixFmt
    shareContext:(NSOpenGLContext*)shareCtx];

  // snipit from SDL_cocoaopengl.m
  //
  // Wisdom from Apple engineer in reference to UT2003's OpenGL performance:
  //  "You are blowing a couple of the internal OpenGL function caches. This
  //  appears to be happening in the VAO case.  You can tell OpenGL to up
  //  the cache size by issuing the following calls right after you create
  //  the OpenGL context.  The default cache size is 16."    --ryan.
  //
  
  #ifndef GLI_ARRAY_FUNC_CACHE_MAX
  #define GLI_ARRAY_FUNC_CACHE_MAX 284
  #endif

  #ifndef GLI_SUBMIT_FUNC_CACHE_MAX
  #define GLI_SUBMIT_FUNC_CACHE_MAX 280
  #endif

  {
      long cache_max = 64;
      CGLContextObj ctx = (CGLContextObj)[newContext CGLContextObj];
      CGLSetParameter(ctx, (CGLContextParameter)GLI_SUBMIT_FUNC_CACHE_MAX, &cache_max);
      CGLSetParameter(ctx, (CGLContextParameter)GLI_ARRAY_FUNC_CACHE_MAX, &cache_max);
  }
  
  // End Wisdom from Apple Engineer section. --ryan.
  return newContext;
}
*/

void* CWinSystemOSX::CreateWindowedContext(void* shareCtx)
{
  NSOpenGLContext* newContext = NULL;

  NSOpenGLPixelFormatAttribute wattrs[] =
  {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAWindow,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)8,
    (NSOpenGLPixelFormatAttribute)0
  };

  NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:wattrs];
  
  newContext = [[NSOpenGLContext alloc] initWithFormat:(NSOpenGLPixelFormat*)pixFmt
    shareContext:(NSOpenGLContext*)shareCtx];
  [pixFmt release];

  if (!newContext)
  {
    // bah, try again for non-accelerated renderer
    NSOpenGLPixelFormatAttribute wattrs2[] =
    {
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAWindow,
      NSOpenGLPFANoRecovery,
      NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)8,
      (NSOpenGLPixelFormatAttribute)0
    };
    NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:wattrs2];

    newContext = [[NSOpenGLContext alloc] initWithFormat:(NSOpenGLPixelFormat*)pixFmt
      shareContext:(NSOpenGLContext*)shareCtx];
    [pixFmt release];
  }

  return newContext;
}

void* CWinSystemOSX::CreateFullScreenContext(int screen_index, void* shareCtx)
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;
  CGDirectDisplayID displayID;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  displayID = displayArray[screen_index];

  NSOpenGLPixelFormatAttribute fsattrs[] =
  {
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAFullScreen,
    NSOpenGLPFANoRecovery,
    NSOpenGLPFAAccelerated,
    NSOpenGLPFADepthSize,  (NSOpenGLPixelFormatAttribute)8,
    NSOpenGLPFAScreenMask, (NSOpenGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(displayID),
    (NSOpenGLPixelFormatAttribute)0
  };
  
  NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:fsattrs];
  if (!pixFmt)
    return nil;
    
  NSOpenGLContext* newContext = [[NSOpenGLContext alloc] initWithFormat:(NSOpenGLPixelFormat*)pixFmt
    shareContext:(NSOpenGLContext*)shareCtx];
  [pixFmt release];

  return newContext;
}

void CWinSystemOSX::GetScreenResolution(int* w, int* h, double* fps)
{
  // Figure out the screen size. (default to main screen)
  CGDirectDisplayID display_id = kCGDirectMainDisplay;
  CFDictionaryRef mode  = CGDisplayCurrentMode(display_id);
  
  NSOpenGLContext* context = [NSOpenGLContext currentContext];
  if (context)
  {
    NSView* view;
  
    view = [context view];
    if (view)
    {
      NSWindow* window;
      window = [view window];
      if (window)
      {
        display_id = GetDisplayIDFromScreen( [window screen] );      
        mode  = CGDisplayCurrentMode(display_id);
      }
    }
  }
  
  *w = GetDictionaryInt(mode, kCGDisplayWidth);
  *h = GetDictionaryInt(mode, kCGDisplayHeight);
  *fps = GetDictionaryDouble(mode, kCGDisplayRefreshRate);
  if ((int)*fps == 0)
  {
    // NOTE: The refresh rate will be REPORTED AS 0 for many DVI and notebook displays.
    *fps = 60.0;
  }
}

void CWinSystemOSX::EnableVSync(bool enable)
{
  // OpenGL Flush synchronised with vertical retrace                       
  GLint swapInterval;
  
  swapInterval = enable ? 1 : 0;
  [[NSOpenGLContext currentContext] setValues:(const long*)&swapInterval forParameter:NSOpenGLCPSwapInterval];
}

bool CWinSystemOSX::SwitchToVideoMode(int width, int height, double refreshrate)
{
  int match = 0;
  CFDictionaryRef dispMode = NULL;
  // Figure out the screen size. (default to main screen)
  CGDirectDisplayID display_id = kCGDirectMainDisplay;
  
  NSOpenGLContext* context = [NSOpenGLContext currentContext];
  if (context)
  {
    NSView* view;
  
    view = [context view];
    if (view)
    {
      NSWindow* window;
      window = [view window];
      if (window)
      {
        display_id = GetDisplayIDFromScreen( [window screen] );      
      }
    }
  }

  // find mode that matches the desired size
  dispMode = CGDisplayBestModeForParametersAndRefreshRate(
    display_id, 32, width, height, (CGRefreshRate)(refreshrate), &match);

  if (!match)
    dispMode = CGDisplayBestModeForParameters(display_id, 32, width, height, &match);

  if (!match)
    dispMode = CGDisplayBestModeForParameters(display_id, 16, width, height, &match);

  if (!match)
    return false;

  // switch mode and return success
  CGDisplayCapture(display_id);
  CGDisplayConfigRef cfg;
  CGBeginDisplayConfiguration(&cfg);
  CGConfigureDisplayFadeEffect(cfg, 0.3f, 0.5f, 0, 0, 0);
  CGConfigureDisplayMode(cfg, display_id, dispMode);
  CGError err = CGCompleteDisplayConfiguration(cfg, kCGConfigureForAppOnly);
  CGDisplayRelease(display_id);
  
  return (err == kCGErrorSuccess);
}

//BOOL interlaced = (CGDisplayModeGetIOFlags((CGDisplayModeRef)displayMode) & kDisplayModeInterlacedFlag);

void CWinSystemOSX::GetVideoModes(void)
{
  CGDirectDisplayID displayID = kCGDirectMainDisplay;
  CFArrayRef displayModes = CGDisplayAvailableModes(displayID);
  if (NULL == displayModes)
    return;

  Boolean stretched;
  Boolean interlaced;
  Boolean safeForHardware;
  Boolean televisionoutput;
  int width, height, bitsperpixel;
  double refreshrate;

  for (int i=0; i<CFArrayGetCount(displayModes); ++i)
  {
    CFDictionaryRef displayMode = (CFDictionaryRef)CFArrayGetValueAtIndex(displayModes, i);

    stretched = GetDictionaryBoolean(displayMode, kCGDisplayModeIsStretched);
    interlaced = GetDictionaryBoolean(displayMode, kCGDisplayModeIsInterlaced);
    bitsperpixel = GetDictionaryInt(displayMode, kCGDisplayBitsPerPixel);
    safeForHardware = GetDictionaryBoolean(displayMode, kCGDisplayModeIsSafeForHardware);
    televisionoutput = GetDictionaryBoolean(displayMode, kCGDisplayModeIsTelevisionOutput);

    if((bitsperpixel == 32) && (safeForHardware == YES) && (stretched == NO) && (interlaced == NO))
    {
      width = GetDictionaryInt(displayMode, kCGDisplayWidth);
      height = GetDictionaryInt(displayMode, kCGDisplayHeight);
      refreshrate = GetDictionaryDouble(displayMode, kCGDisplayRefreshRate);
      if ((int)refreshrate == 0)  // LCD display?
        refreshrate = 150.0;      // Divisible by 25Hz and 30Hz to minimise AV sync waiting
    }
  }
}

bool CWinSystemOSX::FlushBuffer(void)
{
  [ (NSOpenGLContext*)m_glContext flushBuffer ];

  return true;
}

void CWinSystemOSX::NotifyAppFocusChange(bool bGaining)
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  
  if (m_bFullScreen && bGaining)
  {
    // find the window
    NSOpenGLContext* context = [NSOpenGLContext currentContext];
    if (context)
    {
      NSView* view;
    
      view = [context view];
      if (view)
      {
        NSWindow* window;
        window = [view window];
        if (window)
        {
          // find the screenID
          NSDictionary* screenInfo = [[window screen] deviceDescription];
          NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"];
          if ((CGDirectDisplayID)[screenID longValue] == kCGDirectMainDisplay)
          {
            HideMenuBar();
          }
          [window orderFront:nil];
        }
      }
    }
  }
  [pool release];
}

void CWinSystemOSX::ShowOSMouse(bool show)
{
  SDL_ShowCursor(show ? 1 : 0);
}

bool CWinSystemOSX::Minimize()
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  
  [[NSApplication sharedApplication] miniaturizeAll:nil];

  [pool release];
  return true;
}

bool CWinSystemOSX::Restore()
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  
  [[NSApplication sharedApplication] unhide:nil];

  [pool release];
  return true;
}

bool CWinSystemOSX::Hide()
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  
  [[NSApplication sharedApplication] hide:nil];
  
  [pool release];
  return true;
}

bool CWinSystemOSX::Show(bool raise)
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  if (raise)
  {
    [[NSApplication sharedApplication] unhide:nil];
    [[NSApplication sharedApplication] activateIgnoringOtherApps: YES];
    [[NSApplication sharedApplication] arrangeInFront:nil];
  }
  else
  {
    [[NSApplication sharedApplication] unhideWithoutActivation];
  }
      
  [pool release];
  return true;
}

void CWinSystemOSX::EnableSystemScreenSaver(bool bEnable)
{
/* not working any more, problems on 10.6 and atv)
  if (!g_sysinfo.IsAppleTV() )
  {
    NSDictionary* errorDict;
    NSAppleScript* scriptObject;
    NSAppleEventDescriptor* returnDescriptor;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // If we don't call this, the screen saver will just stop and then start up again.
    UpdateSystemActivity(UsrActivity);
    
    if (bEnable)
    {
      // tell application id "com.apple.ScreenSaver.Engine" to launch
      scriptObject = [[NSAppleScript alloc] initWithSource:
        @"launch application \"ScreenSaverEngine\""];
    }
    else
    {
      // tell application id "com.apple.ScreenSaver.Engine" to quit
      scriptObject = [[NSAppleScript alloc] initWithSource:
        @"tell application \"ScreenSaverEngine\" to quit"];
    }
    returnDescriptor = [scriptObject executeAndReturnError: &errorDict];
    [scriptObject release];

    [pool release];
  }
*/
}

bool CWinSystemOSX::IsSystemScreenSaverEnabled()
{
  bool sss_enabled = false;
/*
  if (g_sysinfo.IsAppleTV() )
  {
    sss_enabled = false;
  }
  else
  {
    sss_enabled = g_xbmcHelper.GetProcessPid("ScreenSaverEngine") != -1;
  }
*/
  return(sss_enabled);
}

int CWinSystemOSX::GetNumScreens()
{
  int numDisplays = [[NSScreen screens] count];
  return(numDisplays);
}

#endif
