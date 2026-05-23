#pragma once

void keyboard_init(void);
int  kbd_haschar(void);     /* non-blocking: 1 if character waiting */
char kbd_getchar(void);     /* blocking: waits for next character */
