#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <signal.h>
#include <bsd/string.h> /* strlcpy */

#include <sys/types.h> /* wait */
#include <sys/wait.h>
#include <stdlib.h> /* malloc */

#include "bitmaps/focus_frame_bi.h"	
/* Include file for printing event types */
#include "eventnames.h"

#include "winman.h"
#include "isIcon.h"

typedef void (*sighandler_t) (int);

Window focus_window;
Window inverted_pane = NONE;

static char *menu_label[] = {
    "Raise",
    "Lower",
    "Move",
    "Resize",
    "CirculateDn",
    "CirculateUp",
    "(De)Iconify",
    "Kybrd Focus",
    "New Xterm",
    "Exit",
};

Display *display;

int screen_num;
int screen;

XFontStruct *font_info;

extern WindowList Icons;

void paint_pane(Window window, Window panes[], GC ngc, GC rgc, int mode)
{
    int win;
    int x = 2, y;
    GC gc;
    if (mode == BLACK) {
	XSetWindowBackground(display, window, BlackPixel(display,
							 screen_num));
	gc = rgc;
    } else {
	XSetWindowBackground(display, window, WhitePixel(display,
							 screen_num));
	gc = ngc;
    }
    /* Clearing repaints the background */
    XClearWindow(display, window);
    /* Find out index of window for label text */
    for (win = 0; window != panes[win]; win++);
    y = font_info->max_bounds.ascent;
    /* The string length is necessary because strings
     * for XDrawString may not be null terminated */
    XDrawString(display, window, gc, x, y, menu_label[win],
		strlen(menu_label[win]));
}

void circup(Window menuwin)
{
    XCirculateSubwindowsUp(display, RootWindow(display, screen_num));
    XRaiseWindow(display, menuwin);
}

void circdn(Window menuwin)
{
    XCirculateSubwindowsDown(display, RootWindow(display, screen_num));
    XRaiseWindow(display, menuwin);
}

void raise_lower(Window menuwin, Bool raise_or_lower)
{
    XEvent report;
    int root_x, root_y;
    Window child, root;
    int win_x, win_y;
    unsigned int mask;
    unsigned int button;

    /* Wait for ButtonPress, find out which subwindow of root */
    XMaskEvent(display, ButtonPressMask, &report);
    button = report.xbutton.button;
    XQueryPointer(display, RootWindow(display, screen_num), &root,
		  &child, &root_x, &root_y, &win_x, &win_y, &mask);

    /* If not RootWindow, raise */
    if (child != 0) {
	if (raise_or_lower == RAISE)
	    XRaiseWindow(display, child);
	else
	    XLowerWindow(display, child);
	/* Make sure window manager can never be obscured */
	XRaiseWindow(display, menuwin);
    }

    /* Get the matching ButtonRelease on same button */
    while (1) {
	XMaskEvent(display, ButtonReleaseMask, &report);
	if (report.xbutton.button == button)
	    break;
    }

    /* Throw out any remaining events so we start fresh */
    while (XCheckMaskEvent(display, ButtonReleaseMask |
			   ButtonPressMask, &report));
}

void move_resize(Window menuwin, Cursor hand_cursor, Bool move_or_resize)
{
    XEvent report;
    XWindowAttributes win_attr;
    int press_x, press_y, release_x, release_y, move_x, move_y;
    static int box_drawn = False;
    int left, right, top, bottom;
    Window root, child;
    Window win_to_configure;
    int win_x, win_y;
    unsigned int mask;
    unsigned int pressed_button;
    XSizeHints size_hints;
    /*Bool min_size, increment;*/
    unsigned int width, height;
    int temp_size;
    static GC gc;
    static int first_time = True;
    long user_supplied_mask;

    if (first_time) {
	gc = XCreateGC(display, RootWindow(display, screen_num), 0, NULL);
	XSetSubwindowMode(display, gc, IncludeInferiors);
	XSetForeground(display, gc, WhitePixel(display, screen_num));
	XSetFunction(display, gc, GXxor);
	first_time = False;
    }

    /* Wait for ButtonPress choosing window to configure */
    XMaskEvent(display, ButtonPressMask, &report);
    pressed_button = report.xbutton.button;

    /* Which child of root was press in? */
    XQueryPointer(display, RootWindow(display, screen_num), &root,
		  &child, &press_x, &press_y, &win_x, &win_y, &mask);

    win_to_configure = child;

    if ((win_to_configure == 0) ||
	((win_to_configure == menuwin) && (move_or_resize == RESIZE))) {

	/* If in RootWindow or resizing menuwin,
	 * get release event and get out */
	while (XCheckMaskEvent(display, ButtonReleaseMask |
			       ButtonPressMask, &report));
	return;
    }

    /* Button press was in a valid subwindow of root */
    /* Get original position and size of window */
    XGetWindowAttributes(display, win_to_configure, &win_attr);

    /* Get size hints for the window */
    XGetWMNormalHints(display, win_to_configure, &size_hints,
		      &user_supplied_mask);
    /*if (size_hints.flags && PMinSize)
	min_size = True;
    if (size_hints.flags && PResizeInc)
	increment = True;*/

    /* Now we need pointer motion events */
    XChangeActivePointerGrab(display, PointerMotionHintMask |
			     ButtonMotionMask | ButtonReleaseMask |
			     OwnerGrabButtonMask, hand_cursor,
			     CurrentTime);

    /* Don't allow other display operations during move
     * because the moving outline drawn with Xor won't
     * work properly otherwise */
    XGrabServer(display);

    /* Move outline of window until button release */
    while (1) {
	XNextEvent(display, &report);
	switch (report.type) {
	case ButtonRelease:
	    if (report.xbutton.button == pressed_button) {
		if (box_drawn)
		    /* Undraw box */
		    draw_box(gc, left, top, right-left, bottom-top);

		/* This may seem premature but actually
		 * ButtonRelease indicates that the
		 * rubber-banding is done */
		XUngrabServer(display);

		/* Get final window position */
		XQueryPointer(display, RootWindow(display,
						  screen_num), &root,
			      &child, &release_x, &release_y, &win_x,
			      &win_y, &mask);

		/* Move or resize window */
		if (move_or_resize == MOVE)
		    XMoveWindow(display, win_to_configure,
				win_attr.x + (release_x -
					      press_x), win_attr.y +
				(release_y - press_y));
		else
		    XResizeWindow(display, win_to_configure,
				  win_attr.width + (release_x - press_x),
				  win_attr.height + (release_y - press_y));

		XRaiseWindow(display, win_to_configure);
		XFlush(display);
		box_drawn = False;
		while (XCheckMaskEvent(display,
				       ButtonReleaseMask
				       | ButtonPressMask, &report));
		return;
	    }
	    break;
	case MotionNotify:

	    if (box_drawn == True)
		/* Undraw box */
		draw_box(gc, left, top, right-left, bottom-top);

	    /* Can get rid of all MotionNotify events in
	     * queue, since otherwise the round-trip delays
	     * caused by XQueryPointer may cause a backlog
	     * of MotionNotify events, which will cause
	     * additional wasted XQueryPointer calls */
	    while (XCheckTypedEvent(display, MotionNotify, &report));

	    /* Get current mouse position */
	    XQueryPointer(display, RootWindow(display,
					      screen_num), &root, &child,
			  &move_x, &move_y, &win_x, &win_y, &mask);

	    if (move_or_resize == MOVE) {
		left = move_x - press_x + win_attr.x;
		top = move_y - press_y + win_attr.y;
		right = left + win_attr.width ;
		bottom = top + win_attr.height ;
	    } else {
		if (move_x < win_attr.x)
		    move_x = 0;
		if (move_y < win_attr.y)
		    move_y = 0;
		left = win_attr.x;
		top = win_attr.y;
		right = left + win_attr.width + move_x - press_x;
		bottom = top + win_attr.height + move_y - press_y;

		/* Must adjust size according to size hints */
		/* Enforce minimum dimensions */
		width = right - left;
		height = bottom - top;

		/* Make sure dimension are increment of
		 * width_inc and height_inc and at least
		 * min_width and min_height */
		for (temp_size = size_hints.min_width;
		     temp_size < width; temp_size += size_hints.width_inc);

		right = left + temp_size + 2;

		for (temp_size = size_hints.min_height;
		     temp_size < height;
		     temp_size += size_hints.height_inc);

		/* Most applications (xterm
		 * included) pad their right
		 * and bottom dimensions by
		 * 2 pixels */
		bottom = top + temp_size + 2;
		
	    }

	    draw_box(gc, left, top, right-left,bottom-top);
	    box_drawn = True;
	    break;
	default:
	    /* StructureNotify events should not appear
	     * here because of the ChangeActivePointerGrab
	     * call, but they do for some reason; anyway,
	     * it doesn't matter */
	    /* fprintf(stderr, "unexpected event type %s\n",
	     * report.type); */
	    ;
	}			/* End switch */
    }				/* End outer while */
}				/* End move */



void draw_box(GC gc, int x, int y, unsigned int width, unsigned int height)
{
    XDrawRectangle(display, RootWindow(display, screen), gc, x, y,
		   width, height);
}

void iconify(Window menuwin)
{
    XEvent report;
    /*extern Window focus_window;*/
    Window assoc_win;
    int press_x, press_y;
    Window child;
    Window root;
    int win_x, win_y;
    unsigned int mask;
    unsigned int button;
	char icon_name[50];

    /* Wait for ButtonPress, any win */
    XMaskEvent(display, ButtonPressMask, &report);
    button = report.xbutton.button;

    /* Find out which subwindow the mouse was in */
    XQueryPointer(display, RootWindow(display, screen_num), &root,
		  &child, &press_x, &press_y, &win_x, &win_y, &mask);

    /* Can't iconify rootwindow or menu window */
    if ((child == 0) || (child == menuwin)) {
	/* Wait for ButtonRelease before exiting */
	while (1) {
	    XMaskEvent(display, ButtonReleaseMask, &report);
	    if (report.xbutton.button == button)
		break;
	}
	return;
    }

    /* Returned value of isIcon not used here, but
     * it is elsewhere in the code */
    isIcon(child, press_x, press_y, &assoc_win, icon_name, True);

    /* Window selected is unmapped, whether it is icon
     * or main window; the other is then mapped */
    XUnmapWindow(display, child);
    XMapWindow(display, assoc_win);

    /* Wait for ButtonRelease before exiting */
    /* Get the matching ButtonRelease on same button */
    while (1) {
	XMaskEvent(display, ButtonReleaseMask, &report);
	if (report.xbutton.button == button)
	    break;
    }

    /* Throw out any remaining events so we start fresh
     * for next op */
    while (XCheckMaskEvent(display, ButtonReleaseMask |
			   ButtonPressMask, &report));
}

char *getDefaultIconSize(Window window,unsigned int *icon_w,unsigned int *icon_h)
{
    /* Determine the size of the icon window */
    char *icon_name;

    icon_name = getIconName(window);
    *icon_h = font_info->ascent + font_info->descent + 4;
    *icon_w = XTextWidth(font_info, icon_name, strlen(icon_name));
    return (icon_name);
}

char *getIconName(Window window)
{
    char *name;

    if (XGetIconName(display, window, &name))
	return (name);
    /* Get program name if set */
    if (XFetchName(display, window, &name))
	return (name);
    return ("Icon");
}


Window makeIcon(Window window, int x, int y, char *icon_name_return)
{
    int icon_x, icon_y;		/* Icon U. L. X and Y
				 * coordinates */
    unsigned int icon_w, icon_h;		/* Icon width and height */
    unsigned int icon_bdr;		/* Icon border width */
    unsigned int depth;			/* For XGetGeometry */
    Window root;		/* For XGetGeometry */
    XSetWindowAttributes icon_attrib;	/* For icon creation */
    unsigned long icon_attrib_mask;
    XWMHints *wmhints;		/* See if icon position
				 * provided */
    XWMHints *XGetWMHints();
    Window FinishIcon();
    char *icon_name = NULL;

    /* Process window manager hints.  If icon window hint
     * exists, use it directly.  If icon pixmap hint exists,
     * get its size.  Otherwise, get default size.  If icon
     * position hint exists, use it; otherwise, use the
     * position passed (current mouse position). */
	wmhints = XGetWMHints(display, window);
    if (wmhints) {
	if (wmhints->flags & IconWindowHint)
	    /* Icon window was passed; use it as is */
	    return (finishIcon
		    (window, wmhints->icon_window, False, icon_name));
	else if (wmhints->flags & IconPixmapHint) {
	    /* Pixmap was passed.  Determine size of icon
	     * window from pixmap.  Only icon_w and icon_h
	     * are significant. */
	    if (!XGetGeometry(display, wmhints->icon_pixmap,
			      &root, &icon_x, &icon_y,
			      &icon_w, &icon_h, &icon_bdr, &depth)) {
		fprintf(stderr, "winman: client passed invalid \
                        icon pixmap.");
		return (0);
	    } else {
		icon_attrib.background_pixmap = wmhints->icon_pixmap;
		icon_attrib_mask = CWBorderPixel | CWBackPixmap;
	    }
	}
	/* Else no window or pixmap passed */
	else {
	    icon_name = getDefaultIconSize(window, &icon_w, &icon_h);
	    icon_attrib_mask = CWBorderPixel | CWBackPixel;
	    icon_attrib.background_pixel = (unsigned long)
		WhitePixel(display, screen_num);
	}
    }
    /* Else no hints at all exist */
    else {
	icon_name = getDefaultIconSize(window, &icon_w, &icon_h);
	icon_attrib_mask = CWBorderPixel | CWBackPixel;
    }

    /* Pad sizes */
    icon_w += 2;
    icon_h += 2;
	
	if (icon_name)
    strlcpy(icon_name_return, icon_name,50);

    /* Set the icon border attributes */
    icon_bdr = 2;
    icon_attrib.border_pixel =
	(unsigned long) BlackPixel(display, screen_num);

    /* If icon position hint exists, get it; this also checks
     * to see if wmhints is NULL, which it will be if WMHints
     * were never set at all */
    if (wmhints && (wmhints->flags & IconPositionHint)) {
	icon_x = wmhints->icon_x;
	icon_y = wmhints->icon_y;
    } else {

	/* Put it where the mouse was */
	icon_x = x;
	icon_y = y;
    }

    /* Create the icon window */
    return (finishIcon(window, XCreateWindow(display,
					     RootWindow(display,
							screen_num),
					     icon_x, icon_y, icon_w,
					     icon_h, icon_bdr, 0,
					     CopyFromParent,
					     CopyFromParent,
					     icon_attrib_mask,
					     &icon_attrib), True,
		       icon_name));
}





Window finishIcon(Window window, Window icon, Bool own, char *icon_name)
{
    WindowList win_list;
    Cursor manCursor;

    /* If icon window didn't get created, return failure */
    if (icon == 0)
	return (0);

    /* Use the man cursor whenever the mouse is in the
     * icon window */
    manCursor = XCreateFontCursor(display, XC_man);
    XDefineCursor(display, icon, manCursor);

    /* Select events for the icon window */
    XSelectInput(display, icon, ExposureMask);

    /* Set the event window's icon window to be the new
     * icon window */
    win_list = (WindowList) malloc(sizeof(WindowListRec));
    win_list->window = window;
    win_list->icon = icon;
    win_list->own = own;
    win_list->icon_name = icon_name;
    win_list->next = Icons;
    Icons = win_list;
    return (icon);
}

void removeIcon(Window window)
{
    WindowList win_list, win_list1;

    for (win_list = Icons; win_list; win_list = win_list->next)
	if (win_list->window == window) {
	    if (win_list->own)
		XDestroyWindow(display, win_list->icon);
	    break;
	}
    if (win_list) {
	if (win_list == Icons)
	    Icons = Icons->next;
	else
	    for (win_list1 = Icons; win_list1->next;
		 win_list1 = win_list1->next)
		if (win_list1->next == win_list) {
		    win_list1->next = win_list->next;
		    break;
		};
    }
}

Window focus(Window menuwin)
{
    XEvent report;
    int x, y;
    Window child;
    Window root;
    Window assoc_win;
    extern Window focus_window;
    int win_x, win_y;
    unsigned int mask;
    char icon_name[50];
    unsigned int button;
    XWindowAttributes win_attr;
    static int old_width;
    static Window old_focus;
    int status;

    /* Wait for ButtonPress, any win */
    XMaskEvent(display, ButtonPressMask, &report);
    button = report.xbutton.button;
    /* Find out which subwindow the mouse was in */
    XQueryPointer(display, RootWindow(display, screen_num), &root,
		  &child, &x, &y, &win_x, &win_y, &mask);

    if ((child == 0) || (isIcon(child, x, y, &assoc_win, icon_name,False)))
	focus_window = RootWindow(display, screen_num);
    else
	focus_window = child;
    if (focus_window != old_focus) {	/* If focus changed */

	/* If not first time set, set border back */
	if (old_focus != 0)
	    XSetWindowBorderWidth(display, old_focus, old_width);
	XSetInputFocus(display, focus_window, RevertToPointerRoot,
		       CurrentTime);
	if (focus_window != RootWindow(display, screen_num)) {
	    /* Get current border width and add one */
	    if (!(status = XGetWindowAttributes(display,
						focus_window, &win_attr)))
		fprintf(stderr, "winman: can't get attributes for \
                  focus window\n");
	    XSetWindowBorderWidth(display, focus_window,
				  win_attr.border_width + 1);
	    /* Keep record so we can change it back */
	    old_width = win_attr.border_width;
	}
    }

    /* Get the matching ButtonRelease on same button */
    while (1) {
	XMaskEvent(display, ButtonReleaseMask, &report);
	if (report.xbutton.button == button)
	    break;
    }
    old_focus = focus_window;

    return (focus_window);
}

void draw_focus_frame()
{
    XWindowAttributes win_attr;
    int frame_width = 4;
    static Pixmap focus_tile;
    static GC gc;
    int foreground = BlackPixel(display, screen_num);
    int background = WhitePixel(display, screen_num);
    extern Window focus_window;
    static Bool first_time = True;

    if (first_time) {
	/* Make Bitmap from bitmap data */
	focus_tile = XCreatePixmapFromBitmapData(display,
						 RootWindow(display,
							    screen_num),
						 focus_frame_bi_bits,
						 focus_frame_bi_width,
						 focus_frame_bi_height,
						 foreground, background,
						 DefaultDepth(display,
							      screen_num));

	/* Create graphics context */
	gc = XCreateGC(display, RootWindow(display, screen_num), 0, NULL);
	XSetFillStyle(display, gc, FillTiled);
	XSetTile(display, gc, focus_tile);
	first_time = False;
    }

    /* Get rid of old frames */
    XClearWindow(display, RootWindow(display, screen_num));

    /* If focus is RootWindow, no frame drawn */
    if (focus_window == RootWindow(display, screen_num))
	return;

    /* Get dimensions and position of focus_window */
    XGetWindowAttributes(display, focus_window, &win_attr);
    XFillRectangle(display, RootWindow(display, screen_num), gc,
		   win_attr.x - frame_width, win_attr.y - frame_width,
		   win_attr.width + 2 * (win_attr.border_width +
					 frame_width),
		   win_attr.height + 2 * (win_attr.border_width +
					  frame_width));
}

int execute(char *s)
{
    int status, pid, w;
    sighandler_t istat, qstat;

    if ((pid = vfork()) == 0) {
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	execl("/bin/sh", "sh", "-c", s, NULL);
	_exit(127);
    }
    istat = signal(SIGINT, SIG_IGN);
    qstat = signal(SIGQUIT, SIG_IGN);
    while ((w = wait(&status)) != pid && w != -1);
    if (w == -1)
	status = -1;
    signal(SIGINT, istat);
    signal(SIGQUIT, qstat);

    return (status);
}

int main(int argc, char *argv[])
{
    Window menuwin;
    Window panes[MAX_CHOICE];
    int menu_width, menu_height, x = 0, y = 0, border_width = 4;
    int winindex;
    /*int cursor_shape;*/
    Cursor cursor, hand_cursor;
    char *font_name = "9x15";
    int direction, ascent, descent;
    int char_count;
    char *string;
    XCharStruct overall;
    Bool owner_events;
    int pointer_mode;
    int keyboard_mode;
    Window confine_to;
    GC gc, rgc;
    int pane_height;
    Window assoc_win;
    XEvent event;
    unsigned int button;
	char icon_name[50] = "";


    if ((display = XOpenDisplay(NULL)) == NULL) {
	(void) fprintf(stderr, "winman: cannot connect to \
                X server %s\n", XDisplayName(NULL));
	exit(-1);
    }
    screen_num = DefaultScreen(display);

    /* Access font */
    font_info = XLoadQueryFont(display, font_name);
    if (font_info == NULL) {
	(void) fprintf(stderr, "winman: Cannot open font %s\n", font_name);
	exit(-1);
    }
    string = menu_label[6];
    char_count = strlen(string);

    /* Determine the extent of each menu pane based on
     * the font size */
    XTextExtents(font_info, string, char_count, &direction, &ascent,
		 &descent, &overall);
    menu_width = overall.width + 4;
    pane_height = overall.ascent + overall.descent + 4;
    menu_height = pane_height * MAX_CHOICE;

    /* Place the window in upper-right corner */
    x = DisplayWidth(display,
		     screen_num) - menu_width - (2 * border_width);
    y = 0;			/* Appears at top */

    /* Create opaque window */
    menuwin = XCreateSimpleWindow(display, RootWindow(display,
						      screen_num), x, y,
				  menu_width, menu_height, border_width,
				  BlackPixel(display, screen_num),
				  WhitePixel(display, screen_num));

    /* Create the choice windows for the text */
    for (winindex = 0; winindex < MAX_CHOICE; winindex++) {
	panes[winindex] = XCreateSimpleWindow(display, menuwin, 0,
					      menu_height / MAX_CHOICE *
					      winindex, menu_width,
					      pane_height, border_width =
					      1, BlackPixel(display,
							    screen_num),
					      WhitePixel(display,
							 screen_num));
	XSelectInput(display, panes[winindex],
		     ButtonPressMask | ButtonReleaseMask | ExposureMask);
    }
    XSelectInput(display, RootWindow(display, screen_num),
		 SubstructureNotifyMask);

    /* These do not appear until parent (menuwin) is mapped */
    XMapSubwindows(display, menuwin);

    /* Create the cursor for the menu */
    cursor = XCreateFontCursor(display, XC_left_ptr);
    hand_cursor = XCreateFontCursor(display, XC_hand2);
    XDefineCursor(display, menuwin, cursor);
    focus_window = RootWindow(display, screen_num);

    /* Create two graphics contexts for inverting panes (white
     * and black).  We invert the panes by changing the background
     * pixel, clearing the window, and using the GC with the
     * contrasting color to redraw the text.  Another way is using
     * XCopyArea.  The default is to generate GraphicsExpose and
     * NoExpose events to indicate whether the source area was
     * obscured.  Since the logical function is GXinvert, the
     * destination is also the source.  Therefore, if other
     * windows are obscuring parts of the exposed pane, the
     * wrong area will be inverted.  Therefore, we would need
     * to handle GraphicsExpose and NoExpose events.  We'll do
     * it the easier way. */
    gc = XCreateGC(display, RootWindow(display, screen_num), 0, NULL);
    XSetForeground(display, gc, BlackPixel(display, screen_num));
    rgc = XCreateGC(display, RootWindow(display, screen_num), 0, NULL);
    XSetForeground(display, rgc, WhitePixel(display, screen_num));

    /* Map the menu window (and its subwindows) to the screen_num */
    XMapWindow(display, menuwin);

    /* Force child processes to disinherit the TCP file descriptor;
     * this helps the shell command (creating new xterm) forked and
     * executed from the menu to work properly */
    if ((fcntl(ConnectionNumber(display), F_SETFD, 1)) == -1)
	fprintf(stderr, "winman: child cannot disinherit TCP fd");
    /* Loop getting events on the menu window and icons */
    while (1) {
	/* Wait for an event */
	XNextEvent(display, &event);
	/* If expose, draw text in pane if it is pane */
	switch (event.type) {
	case Expose:
	    if (isIcon(event.xexpose.window, event.xexpose.x,
		       event.xexpose.y, &assoc_win, icon_name, False))
		XDrawString(display, event.xexpose.window, gc, 2,
			    ascent + 2, icon_name, strlen(icon_name));
	    else {		/* It's a pane, might be inverted */
		if (inverted_pane == event.xexpose.window)
		    paint_pane(event.xexpose.window, panes, gc, rgc,
			       BLACK);
		else
		    paint_pane(event.xexpose.window, panes, gc, rgc,
			       WHITE);
	    }
	    break;
	case ButtonPress:
	    paint_pane(event.xbutton.window, panes, gc, rgc, BLACK);
	    button = event.xbutton.button;
	    inverted_pane = event.xbutton.window;
	    /* Get the matching ButtonRelease on same button */
	    while (1) {
		/* Get rid of presses on other buttons */
		while (XCheckTypedEvent(display, ButtonPress, &event));
		/* Wait for release; if on correct button, exit */
		XMaskEvent(display, ButtonReleaseMask, &event);
		if (event.xbutton.button == button)
		    break;
	    }
	    /* All events are sent to the grabbing window
	     * regardless of whether this is True or False;
	     * owner_events only affects the distribution
	     * of events when the pointer is within this
	     * application's windows */
	    owner_events = True;
	    /* We don't want pointer or keyboard events
	     * frozen in the server */
	    pointer_mode = GrabModeAsync;
	    keyboard_mode = GrabModeAsync;
	    /* We don't want to confine the cursor */
	    confine_to = None;
	    XGrabPointer(display, menuwin, owner_events,
			 ButtonPressMask | ButtonReleaseMask,
			 pointer_mode, keyboard_mode,
			 confine_to, hand_cursor, CurrentTime);
	    /* If press and release occurred in same window,
	     * do command; if not, do nothing */
	    if (inverted_pane == event.xbutton.window) {
		/* Convert window ID to window array index  */
		for (winindex = 0; inverted_pane != panes[winindex];
		     winindex++);
		switch (winindex) {
		case 0:
		    raise_lower(menuwin, RAISE);
		    break;
		case 1:
		    raise_lower(menuwin, LOWER);
		    break;
		case 2:
		    move_resize(menuwin, hand_cursor, MOVE);
		    break;
		case 3:
		    move_resize(menuwin, hand_cursor, RESIZE);
		    break;
		case 4:
		    circup(menuwin);
		    break;
		case 5:
		    circdn(menuwin);
		    break;
		case 6:
		    iconify(menuwin);
		    break;
		case 7:
		    focus_window = focus(menuwin);
		    break;
		case 8:
		    execute("xterm&");
		    break;
		case 9:	/* Exit */
		    XSetInputFocus(display,
				   RootWindow(display, screen_num),
				   RevertToPointerRoot, CurrentTime);
		    /* Turn all icons back into windows */
		    /* Must clear focus highlights */
		    XClearWindow(display, RootWindow(display, screen_num));
		    /* Need to change focus border width back here */
		    XFlush(display);
		    XCloseDisplay(display);
		    exit(1);
		default:
		    (void) fprintf(stderr, "Something went wrong\n");
		    break;
		}		/* End switch */
	    }
	    /* End if */

	    /* Invert Back Here (logical function is invert) */
	    paint_pane(event.xexpose.window, panes, gc, rgc, WHITE);
	    inverted_pane = NONE;
	    draw_focus_frame();
	    XUngrabPointer(display, CurrentTime);
	    XFlush(display);
	    break;
	case DestroyNotify:
	    /* Window we have iconified has died, remove its
	     * icon; don't need to remove window from save-set
	     * because that is done automatically */
	    removeIcon(event.xdestroywindow.window);
	    break;
	case CirculateNotify:
	case ConfigureNotify:
	case UnmapNotify:
	    /* All these uncover areas of screen_num */
	    draw_focus_frame();
	    break;
	case CreateNotify:
	case GravityNotify:
	case MapNotify:
	case ReparentNotify:
	    /* Don't need these, but get them anyway since
	     * we need DestroyNotify and UnmapNotify */
	    break;
	case ButtonRelease:
	    /* Throw these way, they are spurious here */
	    break;
	case MotionNotify:
	    /* Throw these way, they are spurious here */
	    break;
	default:
	    fprintf(stderr, "winman: got unexpected %s event.\n",
		    event_names[event.type]);
	}			/* End switch */
    }				/* End menu loop (while) */
}				/* End main */

