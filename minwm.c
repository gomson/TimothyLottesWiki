#define WINDOW_MAX 256

/*============================================================================


                             /\/\in \/\/ /\/\


             Minimal Window Manager [MinWM] by Timothy Lottes


------------------------------------------------------------------------------
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

------------------------------------------------------------------------------
ABOUT
-----
 Simple yet very useful single screen X Window Manager.
 Designed to minimize wasted user time interacting with windows.
 No configuration files.
 Tiny x86-64 binary.

------------------------------------------------------------------------------
KEYS
----
 ALT+ESC .......... Close window.
 ALT+TAB .......... Cycle through window list on virtual screen (like Windows).
 ALT+` ............ Cycle window shape between full, and tiled positions.
 ALT+1 ............ Switch virtual screen left.
 ALT+2 ............ Switch virtual screen right.
 ALT+3 ............ Move focus window to virtual screen left.
 ALT+4 ............ Move focus window to virtual screen right.

------------------------------------------------------------------------------
WINDOW LIST
-----------
 The windows list is ordered as follows,

  { most recently used, 2nd most recently used, ..., last used }

 While ALT is held down pressing TAB will cycle through list,
 going to the last reciently used window from the current window.
 It will wrap around at the end.
 After ALT is released, the list is updated.
 The new current window is moved to the front of the list.

------------------------------------------------------------------------------
COMPILE
-------
 Only requires a C compiler and the X11 library.
 Try something like,

  gcc minwm.c -Os -o minwm -I/usr/X11/include -L/usr/X11/lib -lX11
  strip minwm

 Then setup your .xinitrc file like,

  xrdb -merge $HOME/.Xresources
  xterm -rv -ls +sb -sl 4096 &
  exec $HOME/minwm

 Then run xinit and then start programs from the terminal.

============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
/*--------------------------------------------------------------------------*/
enum { INDEX_ROOT, INDEX_TOP, INDEX_2ND };
enum { KEY_TAB, KEY_ESC, KEY_TILDE, KEY_ALT, KEY_1, KEY_2, KEY_3, KEY_4, KEYS };
enum { SHAPE_FULL, SHAPE_LEFT, SHAPE_RIGHT, SHAPES };
/*--------------------------------------------------------------------------*/
static void GrabKeys(Display* display, Window root, int* __restrict keycode) {
  uint32_t i;
  const static unsigned int modifiers[KEYS*2] = {
    Mod1Mask, // KEY_TAB
    Mod1Mask, // KEY_ESC
    Mod1Mask, // KEY_TILDE
    0,        // KEY_ALT
    Mod1Mask, // KEY_1
    Mod1Mask, // KEY_2
    Mod1Mask, // KEY_3
    Mod1Mask, // KEY_4
    Mod1Mask | Mod2Mask,
    Mod1Mask | Mod2Mask,
    Mod1Mask | Mod2Mask,
    Mod2Mask,
    Mod1Mask | Mod2Mask,
    Mod1Mask | Mod2Mask,
    Mod1Mask | Mod2Mask,
    Mod1Mask | Mod2Mask };
  for(i = 0; i < KEYS*2; i++) { XGrabKey(display, keycode[i >= KEYS ? i-KEYS : i], modifiers[i], root, True, GrabModeAsync, GrabModeAsync); } }
/*--------------------------------------------------------------------------*/
static Window Parent(Display* display, Window window) {
  Window root; Window parent; Window* children; unsigned int num;
  if(XQueryTree(display, window, &root, &parent, &children, &num) == 0) return 0;
  if(num && children) XFree(children);
  return parent; }
/*--------------------------------------------------------------------------*/
static void InstallColormap(Display* display, Colormap colormap) { if(colormap != None) XInstallColormap(display, colormap); }
/*--------------------------------------------------------------------------*/
// Lookup() returns count on window not found.
static uint32_t Lookup(Window* __restrict windows, Window window, uint32_t count) {
  windows[count] = window;
  Window* __restrict table = windows;
  uint32_t index;
  while(table[index] != window) index++;
  return index; }
/*--------------------------------------------------------------------------*/
static void Use(uint32_t index, Display* display, const Window* __restrict windows, const Colormap* __restrict colormaps) {
  InstallColormap(display, colormaps[index]);
  #if 0
    // Sorry cannot remember why this is here.
    XSetInputFocus(display, windows[index], RevertToPointerRoot, CurrentTime);
    if(index != INDEX_ROOT) XRaiseWindow(display, windows[index]); }
  #else
    if(index != INDEX_ROOT) {
      XRaiseWindow(display, windows[index]);
      XSetInputFocus(display, windows[index], RevertToPointerRoot, CurrentTime); } }
  #endif
/*--------------------------------------------------------------------------*/
static void Move(uint32_t dst, uint32_t src, uint32_t count, Window* windows, Colormap* colormaps, uint8_t* shapes, int32_t* offs) {
  if(count == 0) return;
  memmove(windows + dst, windows + src, count * sizeof(Window));
  memmove(colormaps + dst, colormaps + src, count * sizeof(Colormap));
  memmove(shapes + dst, shapes + src, count * sizeof(uint8_t));
  memmove(offs + dst, offs + src, count * sizeof(int32_t)); }
/*--------------------------------------------------------------------------*/
// New() returns new count value.
static uint32_t New(Display* display, Window window, uint32_t count, Window* windows, Colormap* colormaps, uint8_t* shapes, int32_t* offs) {
  XWindowAttributes att;
  XGetWindowAttributes(display, window, &att);
  if((att.override_redirect == True) || (Parent(display, window) != windows[INDEX_ROOT])) return count;
  Move(INDEX_2ND, INDEX_TOP, count - 1, windows, colormaps, shapes, offs);
  count++;
  colormaps[INDEX_TOP] = att.colormap;
  windows[INDEX_TOP] = window;
  shapes[INDEX_TOP] = SHAPE_LEFT;
  offs[INDEX_TOP] = 0;
  XMapWindow(display, window);
  Use(INDEX_TOP, display, windows, colormaps);
  return count; }
/*--------------------------------------------------------------------------*/
static int Handler(Display* display, XErrorEvent* event) { return(0); }
/*--------------------------------------------------------------------------*/
// Returns new Use() window.
static uint32_t Shift(Display* display, uint32_t top, uint32_t except, int32_t off, uint32_t count, int32_t* __restrict offs, const Window* __restrict windows) {
  uint32_t limit = count - 1; uint32_t i; uint32_t ret = INDEX_ROOT;
  for(i=0;i<limit;i++) {
    uint32_t next = top + i;
    next = (next >= count) ? 1 + next - count : next;
    if(next != except) {
      XWindowAttributes att;
      XGetWindowAttributes(display, windows[next], &att);
      XWindowChanges change;
      change.x = att.x + off;
      XConfigureWindow(display, windows[next], CWX, &change);
      offs[next] += off;
      if((offs[next] == 0) && (ret == INDEX_ROOT)) ret = next; } }
  return ret; }
/*--------------------------------------------------------------------------*/
static uint32_t FindNext(uint32_t top, uint32_t count, const int32_t* __restrict offs) {
  uint32_t limit = count - 2; uint32_t i;
  for(i=0;i<limit;i++) {
    uint32_t next = top + 1 + i;
    next = (next >= count) ? 1 + next - count : next;
    if(offs[next] == 0) return next; } return top; }
/*--------------------------------------------------------------------------*/
static const char stupidUser[] =
  "\n"
  "--/\\/\\in \\/\\/ /\\/\\-------------------------------------------------\n"
  "No windows found!\n"
  "Before starting xinit, set `.xinitrc` to run a term before minwm,\n"
  "\n"
  " xterm -rv -ls +sb -sl 4096 &\n"
  " $HOME/minwm\n"
  "\n";
/*==========================================================================*/
int main(int unused, char** alsoUnused) {
/*--------------------------------------------------------------------------*/
  XEvent event;
/*--------------------------------------------------------------------------*/
  Colormap colormaps[WINDOW_MAX];
  Window windows[WINDOW_MAX];
  uint8_t shapes[WINDOW_MAX];
  int32_t offs[WINDOW_MAX];
/*--------------------------------------------------------------------------*/
  Display* display = XOpenDisplay(0);
  if(display == 0) exit(0);
  XSetErrorHandler(Handler);
  windows[INDEX_ROOT] = RootWindow(display, 0);
  XSelectInput(display, windows[INDEX_ROOT], PropertyChangeMask | SubstructureRedirectMask | SubstructureNotifyMask);
  colormaps[INDEX_ROOT] = DefaultColormap(display, 0);
  uint32_t count = 1;
  uint32_t top = INDEX_TOP;
/*--------------------------------------------------------------------------*/
  XClientMessageEvent close;
  close.type = ClientMessage;
  close.send_event = True;
  close.display = display;
  close.format = 32;
  close.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", 0);
  close.message_type = XInternAtom(display, "WM_PROTOCOLS", 0);
/*--------------------------------------------------------------------------*/
  XWindowChanges changes[SHAPES];
  memset(changes, 0, sizeof(changes));
  uint32_t width = DisplayWidth(display, 0);
  changes[SHAPE_FULL].width = width;
  changes[SHAPE_LEFT].width = width>>1;
  changes[SHAPE_RIGHT].width = width - changes[SHAPE_LEFT].width;
  changes[SHAPE_FULL].height =
    changes[SHAPE_LEFT].height =
    changes[SHAPE_RIGHT].height = DisplayHeight(display, 0);
  changes[SHAPE_RIGHT].x = changes[SHAPE_LEFT].width;
/*--------------------------------------------------------------------------*/
  int keycode[KEYS] = {
    XKeysymToKeycode(display, XK_Tab),
    XKeysymToKeycode(display, XK_Escape),
    XKeysymToKeycode(display, XK_asciitilde),
    XKeysymToKeycode(display, XK_Alt_L),
    XKeysymToKeycode(display, XK_1),
    XKeysymToKeycode(display, XK_2),
    XKeysymToKeycode(display, XK_3),
    XKeysymToKeycode(display, XK_4) };
/*--------------------------------------------------------------------------*/
  GrabKeys(display, windows[INDEX_ROOT], keycode);
/*--------------------------------------------------------------------------*/
  { uint32_t tries = 0;
    while(1) {
      Window root; Window parent; Window* children; unsigned int num;
      XQueryTree(display, windows[INDEX_ROOT], &root, &parent, &children, &num);
      if(num) {
        const Window* __restrict list = children;
        while(num--) { count = New(display, list[0], count, windows, colormaps, shapes, offs); list++; }
        if(children) XFree(children); }
      if(count > 1) break;
      usleep(100*1000);
      tries++;
      if(tries > 20) { printf(stupidUser); exit(0); } } }
/*--------------------------------------------------------------------------*/
  XSync(display, False);
  while(1) {
    XNextEvent(display, &event);
    switch(event.type) {
/*--------------------------------------------------------------------------*/
      case FocusIn:
      case FocusOut:
        if(event.xfocus.mode == NotifyGrab) GrabKeys(display, windows[INDEX_ROOT], keycode); break;
/*--------------------------------------------------------------------------*/
      case ConfigureRequest:
        { XWindowChanges changes;
          changes.x = event.xconfigurerequest.x;
          changes.y = event.xconfigurerequest.y;
          changes.width = event.xconfigurerequest.width;
          changes.height = event.xconfigurerequest.height;
          changes.border_width = event.xconfigurerequest.border_width;
          XConfigureWindow(display, event.xconfigurerequest.window, event.xconfigurerequest.value_mask & (~(CWSibling | CWStackMode)), &changes); }
        break;
/*--------------------------------------------------------------------------*/
      case ColormapNotify:
        { XWindowAttributes att;
          uint32_t index = Lookup(windows, event.xcolormap.window, count);
          if(index != count) {
            XGetWindowAttributes(display, event.xcolormap.window, &att);
            colormaps[index] = att.colormap;
            if(index == top) InstallColormap(display, att.colormap); } }
        break;
/*--------------------------------------------------------------------------*/
      case MapRequest:
        { XWindowAttributes att;
          XGetWindowAttributes(display, event.xmaprequest.window, &att);
          if((Lookup(windows, event.xmaprequest.window, count) == count) && (Parent(display, event.xmaprequest.window) == windows[INDEX_ROOT]) && (att.override_redirect == False)) {
            count = New(display, event.xmaprequest.window, count, windows, colormaps, shapes, offs);
            top = INDEX_TOP; }
          else XMapRaised(display, event.xmaprequest.window); }
        break;
/*--------------------------------------------------------------------------*/
      case KeyRelease:
        { uint32_t retop=0;
               if(event.xkey.keycode == keycode[KEY_1]) { top = Shift(display, (top==INDEX_ROOT)?INDEX_TOP:top, INDEX_ROOT,  width, count, offs, windows); Use(top, display, windows, colormaps); retop=1; }
          else if(event.xkey.keycode == keycode[KEY_2]) { top = Shift(display, (top==INDEX_ROOT)?INDEX_TOP:top, INDEX_ROOT, -width, count, offs, windows); Use(top, display, windows, colormaps); retop=1; }
          else if(event.xkey.keycode == keycode[KEY_3]) { Shift(display, (top==INDEX_ROOT)?INDEX_TOP:top, top,  width, count, offs, windows); Use(top, display, windows, colormaps); retop=1; }
          else if(event.xkey.keycode == keycode[KEY_4]) { Shift(display, (top==INDEX_ROOT)?INDEX_TOP:top, top, -width, count, offs, windows); Use(top, display, windows, colormaps); retop=1; }
          else if(((event.xkey.keycode == keycode[KEY_ALT]) || (retop==1)) && (top > INDEX_TOP)) {
            Colormap colormap = colormaps[top];
            Window window = windows[top];
            uint8_t shape = shapes[top];
            uint32_t off = offs[top];
            Move(INDEX_2ND, INDEX_TOP, top - 1, windows, colormaps, shapes, offs);
            colormaps[INDEX_TOP] = colormap;
            windows[INDEX_TOP] = window;
            shapes[INDEX_TOP] = shape;
            offs[INDEX_TOP] = off;
            top = INDEX_TOP; }
          else XSendEvent(display, windows[top], True, 0, &event); }
        break;
/*--------------------------------------------------------------------------*/
      case KeyPress:
        if(event.xkey.keycode == keycode[KEY_TAB]) { top = FindNext(top, count, offs); Use(top, display, windows, colormaps); }
        else if(top != INDEX_ROOT) {
          if(event.xkey.keycode == keycode[KEY_TILDE]) {
            uint32_t shape = shapes[top] + 1;
            shape = (shape == SHAPES) ? 0 : shape;
            shapes[top] = shape;
            XConfigureWindow(display, windows[top], CWX | CWY | CWWidth | CWHeight, changes + shape); }
          else if(event.xkey.keycode == keycode[KEY_ESC]) {
            close.window = windows[top];
            XSendEvent(display, close.window, False, 0, (XEvent*)&close); } }
        else XSendEvent(display, windows[top], True, 0, &event);
        break;
/*--------------------------------------------------------------------------*/
      case UnmapNotify:
        { uint32_t index = Lookup(windows, event.xunmap.window, count);
          if(index != count) {
            count--;
            if(count <= 1) exit(0);
            Move(index, index + 1, count - 1, windows, colormaps, shapes, offs);
            Use(INDEX_TOP, display, windows, colormaps);
            top = INDEX_TOP; } }
        break; } } }
