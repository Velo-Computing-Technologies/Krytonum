#include "orion.h"

void render_terminal(void)
{
    draw_rect(40, 30, 240, 140, 0);  /* black terminal background */
    draw_rect(40, 30, 240, 15, 7);   /* grey title bar */
    draw_text(45, 33, "Terminal", 0);

    /* Fake cursor prompt */
    draw_text(45, 52, "> _", 2);
}
