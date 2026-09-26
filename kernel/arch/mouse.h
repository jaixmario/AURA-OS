#ifndef MOUSE_H
#define MOUSE_H

void mouse_init(int screen_w, int screen_h);
int  mouse_get_x(void);
int  mouse_get_y(void);
int  mouse_is_left_down(void);
int  mouse_is_right_down(void);
int  mouse_is_middle_down(void);
int  mouse_clicked(int btn); // returns 1 once on left click release/press

#endif
