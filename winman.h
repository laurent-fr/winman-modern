#ifndef winman_h
#define winman_h

void paint_pane(Window window, Window panes[], GC ngc, GC rgc, int mode);
void circup(Window menuwin);
void circdn(Window menuwin);
void raise_lower(Window menuwin, Bool raise_or_lower);
void move_resize(Window menuwin, Cursor hand_cursor, Bool move_or_resize);
void draw_box(GC gc, int x, int y, unsigned int width,
	      unsigned int height);
void iconify(Window menuwin);
char *getDefaultIconSize(Window window, unsigned int *icon_w,unsigned int *icon_h);
char *getIconName(Window window);
Window makeIcon(Window window, int x, int y, char *icon_name_return);
Window finishIcon(Window window, Window icon, Bool own, char *icon_name);
void removeIcon(Window window);
Window focus(Window menuwin);
void draw_focus_frame();
int execute(char *s);

#define MAX_CHOICE 10
#define DRAW 1
#define ERASE 0
#define RAISE 1
#define LOWER 0
#define MOVE 1
#define RESIZE 0
#define NONE 100
#define NOTDEFINED 0
#define BLACK  1
#define WHITE  0

#endif
