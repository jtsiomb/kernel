#ifndef VID_H_
#define VID_H_

#define WIDTH			80
#define HEIGHT			25

/* the standard CGA color palette */
enum {
	BLACK,
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	LTGRAY,
	GRAY,
	LTBLUE,
	LTGREEN,
	LTCYAN,
	LTRED,
	LTMAGENTA,
	YELLOW,
	WHITE
};

void clear_scr(void);
void set_char(char c, int x, int y, int fg, int bg);
void set_cursor(int x, int y);
void scroll_scr(void);

#endif	/* VID_H_ */
