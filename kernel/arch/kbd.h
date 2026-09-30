#ifndef KBD_H
#define KBD_H

#define KEY_ENTER     '\n'
#define KEY_BACKSPACE '\b'
#define KEY_TAB       '\t'
#define KEY_ESC       27

#define KEY_UP        0x80
#define KEY_DOWN      0x81
#define KEY_LEFT      0x82
#define KEY_RIGHT     0x83
#define KEY_SUPER     0x84
#define KEY_HOME      0x85
#define KEY_F11       0x86
#define KEY_F12       0x87

void kbd_init(void);
int  kbd_has_char(void);
char kbd_get_char(void);
unsigned char kbd_last_scancode(void);
int  kbd_is_shift_down(void);
void kbd_handle_scancode(unsigned char scancode);

#endif
