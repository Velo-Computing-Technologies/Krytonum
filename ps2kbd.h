#ifndef PS2KBD_H
#define PS2KBD_H

void ps2kbd_init(void);

/*
 * Returns the next ASCII character if a key has been pressed, or -1 if
 * the keyboard buffer is empty.  Call this in your main loop.
 */
int ps2kbd_poll(void);

#endif /* PS2KBD_H */
