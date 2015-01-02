#ifndef isicon_h
#define isicon_h

/* For linked list containing window ID, icon ID, and icon_name;
 * own indicates whether winman created the icon window (True)
 * or was passed it through the WMHints (False) */
typedef struct _windowList {
    struct _windowList *next;
    Window window;
    Window icon;
    Bool own;
    char *icon_name;
} WindowListRec, *WindowList;

Bool isIcon(Window win, int x, int y, Window * assoc, char *icon_name,
	    Bool makeicon);

#endif
