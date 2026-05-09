#include "orion.h"

uint8_t theme_bg_color      = 1;   /* Dark Blue  */
uint8_t theme_taskbar_color = 8;   /* Dark Grey  */
uint8_t theme_text_color    = 15;  /* White      */

void load_defaults(void)
{
    theme_bg_color      = 1;
    theme_taskbar_color = 8;
    theme_text_color    = 15;
}
