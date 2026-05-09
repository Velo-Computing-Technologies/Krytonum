#include "orion.h"

void render_app_view(void)
{
    draw_rect(50, 40, 220, 120, 15); /* main white window */
    draw_rect(50, 40, 220, 15,   1); /* blue title bar */
    draw_rect(255, 42, 11, 11,   4); /* red close button */
    draw_text(55, 43, "Application", 15);
}

void render_home_screen(void)
{
    /* Clear to background colour */
    draw_rect(0, 0, SCREEN_W, SCREEN_H, theme_bg_color);

    /* Logo in the top-centre area */
    draw_logo(100, 30);

    /* Taskbar at the bottom */
    render_taskbar();

    /* Welcome text */
    draw_text(10, 10, "Krytonium OS - x86_64 UEFI", 15);
}
