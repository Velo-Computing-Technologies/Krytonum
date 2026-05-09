#include "orion.h"

void render_settings(void)
{
    draw_rect(60, 20, 200, 150, 7);   /* light grey background */
    draw_rect(60, 20, 200, 15, 1);    /* blue title bar */
    draw_text(65, 23, "Settings", 15);
    draw_rect(70, 50, 180, 40, 15);   /* system info box */
    draw_text(75, 55, "Krytonium x86_64 UEFI", 1);
    draw_rect(70, 100, 180, 40, 15);  /* dependencies box */
    draw_text(75, 105, "Limine  e1000  TCP/IP", 1);
}
