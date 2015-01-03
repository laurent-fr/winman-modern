#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <bsd/string.h> /*strlcpy*/

#include "isIcon.h"
#include "winman.h"

extern Display *display;
extern int screen_num;

WindowList Icons = NULL;

Bool isIcon(Window win, int x, int y, Window * assoc, char *icon_name,
	    Bool makeicon)
{
    WindowList win_list;
    /*Window makeIcon();*/

    /* Go through linked list of window-icon structures */
    for (win_list = Icons; win_list; win_list = win_list->next) {
	if (win == win_list->icon) {	/* Win is icon */
	    *assoc = win_list->window;
		if (win_list->icon_name)
	    strlcpy(icon_name, win_list->icon_name,50);
	    return (True);
	}
	if (win == win_list->window) {	/* Win is main window */
	    *assoc = win_list->icon;
		if (win_list->icon_name)
	    strlcpy(icon_name, win_list->icon_name,50);
	    return (False);
	}
    }

    /* Window not in list means icon not created yet; create icon
     * and add main window to save-set in case window manager dies */
    if (makeicon) {
	*assoc = makeIcon(win, x, y, icon_name);
	XAddToSaveSet(display, win);
    }

    return (False);
}
