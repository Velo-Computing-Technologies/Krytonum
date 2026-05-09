#ifndef ORION_H
#define ORION_H

#include <stdint.h>
#include <stdbool.h>

/*
 * -----------------------------------------------------------------------
 * Framebuffer initialisation — called once from kernel_main with the
 * address, dimensions, pitch and bpp provided by Limine.
 * -----------------------------------------------------------------------
 */
void fb_init(void *addr, uint32_t width, uint32_t height,
             uint32_t pitch, uint16_t bpp);

/*
 * -----------------------------------------------------------------------
 * Core graphics (implemented in system.cpp)
 *
 * Colour values are VGA palette indices (0–15) so that the existing
 * screen-drawing code works without modification.  The palette is
 * converted to 32-bit RGB when the double buffer is flushed.
 * -----------------------------------------------------------------------
 */

/* Virtual canvas — kept at 320×200 for compatibility with existing code */
#define SCREEN_W  320
#define SCREEN_H  200

extern uint8_t double_buffer[SCREEN_W * SCREEN_H];

void draw_pixel(int x, int y, uint8_t color);
void draw_rect(int x, int y, int width, int height, uint8_t color);
void draw_text(int x, int y, const char *str, uint8_t color);
void draw_logo(int start_x, int start_y);
void flush_buffer_to_screen(void);

/*
 * -----------------------------------------------------------------------
 * KAN-1: Login (implemented in login.cpp)
 * -----------------------------------------------------------------------
 */
void render_login_screen(void);
bool is_authenticated(void);
void login_key(int ch);          /* feed a keystroke to the login UI */
void attempt_esp32_login(void);  /* fire the HTTP request to the ESP32 */

/*
 * -----------------------------------------------------------------------
 * KAN-4: Home screen
 * -----------------------------------------------------------------------
 */
void render_home_screen(void);

/*
 * -----------------------------------------------------------------------
 * KAN-5 / KAN-6 / KAN-8: App view / Terminal / Settings / Taskbar
 * -----------------------------------------------------------------------
 */
void render_app_view(void);
void render_terminal(void);
void render_settings(void);
void render_taskbar(void);

/*
 * -----------------------------------------------------------------------
 * KAN-7: Defaults (implemented in default.cpp)
 * -----------------------------------------------------------------------
 */
extern uint8_t theme_bg_color;
extern uint8_t theme_taskbar_color;
extern uint8_t theme_text_color;
void load_defaults(void);

/*
 * -----------------------------------------------------------------------
 * KAN-8: System initialisation
 * -----------------------------------------------------------------------
 */
void init_system(void);

/*
 * -----------------------------------------------------------------------
 * PS/2 keyboard (implemented in ps2kbd.cpp)
 * -----------------------------------------------------------------------
 */
void ps2kbd_init(void);
int  ps2kbd_poll(void);  /* returns ASCII char, or -1 if none ready */

/*
 * -----------------------------------------------------------------------
 * Networking (implemented in e1000.cpp / net.cpp / http.cpp)
 * -----------------------------------------------------------------------
 */
void net_init(void);

/* Plain HTTP POST to the ESP32 gateway.
 * Returns 1 on success (access_token found in response), 0 on failure. */
int http_login_esp32(const char *email, const char *password);

#endif /* ORION_H */
