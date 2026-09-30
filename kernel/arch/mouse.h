#ifndef MOUSE_H
#define MOUSE_H

void mouse_init(int screen_w, int screen_h);
int  mouse_get_x(void);
int  mouse_get_y(void);
int  mouse_is_left_down(void);
int  mouse_is_right_down(void);
int  mouse_is_middle_down(void);
int  mouse_clicked(int btn); // returns 1 once on left click release/press
void mouse_set_speed(int level);
int  mouse_get_speed(void);
void mouse_set_bounds(int screen_w, int screen_h);
void mouse_move_relative(int dx, int dy);
void mouse_inject_click(int left, int right);
void mouse_center(void);
int  mouse_is_detected(void);
void mouse_handle_byte(unsigned char b);

#endif
