#include "orion.h"
#include "logo.h"
#include "font8x8.h"

/*
 * -----------------------------------------------------------------------
 * VGA-to-RGB24 colour palette (indices 0-15)
 * -----------------------------------------------------------------------
 */
static const uint32_t vga_palette[16] = {
    0x000000, /* 0  black        */
    0x0000AA, /* 1  blue         */
    0x00AA00, /* 2  green        */
    0x00AAAA, /* 3  cyan         */
    0xAA0000, /* 4  red          */
    0xAA00AA, /* 5  magenta      */
    0xAA5500, /* 6  brown        */
    0xAAAAAA, /* 7  light grey   */
    0x555555, /* 8  dark grey    */
    0x5555FF, /* 9  bright blue  */
    0x55FF55, /* 10 bright green */
    0x55FFFF, /* 11 bright cyan  */
    0xFF5555, /* 12 bright red   */
    0xFF55FF, /* 13 bright magenta */
    0xFFFF55, /* 14 yellow       */
    0xFFFFFF, /* 15 white        */
};

/*
 * -----------------------------------------------------------------------
 * UEFI framebuffer state (filled by fb_init)
 * -----------------------------------------------------------------------
 */
static uint8_t  *fb_addr  = 0;
static uint32_t  fb_width  = 0;
static uint32_t  fb_height = 0;
static uint32_t  fb_pitch  = 0;  /* bytes per row */
static uint16_t  fb_bpp    = 32;

void fb_init(void *addr, uint32_t width, uint32_t height,
             uint32_t pitch, uint16_t bpp)
{
    fb_addr   = (uint8_t *)addr;
    fb_width  = width;
    fb_height = height;
    fb_pitch  = pitch;
    fb_bpp    = bpp;
}

/*
 * -----------------------------------------------------------------------
 * 320×200 virtual canvas (one byte = VGA colour index)
 * -----------------------------------------------------------------------
 */
uint8_t double_buffer[SCREEN_W * SCREEN_H];

void draw_pixel(int x, int y, uint8_t color)
{
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        double_buffer[y * SCREEN_W + x] = color;
}

void draw_rect(int x, int y, int width, int height, uint8_t color)
{
    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++)
            draw_pixel(x + col, y + row, color);
}

void draw_text(int x, int y, const char *str, uint8_t color)
{
    int cx = x;
    while (*str) {
        char c = *str++;
        if (c == '\n') { y += 9; cx = x; continue; }
        if (c < 0x20 || c > 0x7E) { cx += 8; continue; }
        const uint8_t *glyph = font8x8[(uint8_t)c - 0x20];
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80u >> col))
                    draw_pixel(cx + col, y + row, color);
            }
        }
        cx += 8;
    }
}

void draw_logo(int start_x, int start_y)
{
    for (int row = 0; row < 120; row++)
        for (int col = 0; col < 120; col++)
            draw_pixel(start_x + col, start_y + row,
                       logo_pixels[row * 120 + col]);
}

/*
 * Flush the 320×200 virtual canvas to the actual UEFI framebuffer.
 * Each virtual pixel is scaled to fill (scale_x × scale_y) real pixels.
 */
void flush_buffer_to_screen(void)
{
    if (!fb_addr) return;

    /* Integer scale factors — fill as much of the screen as possible */
    uint32_t sx = fb_width  / SCREEN_W;
    uint32_t sy = fb_height / SCREEN_H;
    if (sx < 1) sx = 1;
    if (sy < 1) sy = 1;

    uint32_t bytes_per_pixel = fb_bpp / 8;

    for (int vy = 0; vy < SCREEN_H; vy++) {
        uint8_t *vrow = double_buffer + vy * SCREEN_W;
        for (int vx = 0; vx < SCREEN_W; vx++) {
            uint32_t rgb = vga_palette[vrow[vx] & 0xF];
            /* Write scaled block of real pixels */
            for (uint32_t dy = 0; dy < sy; dy++) {
                uint32_t phy = vy * sy + dy;
                if (phy >= fb_height) break;
                uint8_t *row_base = fb_addr + phy * fb_pitch;
                for (uint32_t dx = 0; dx < sx; dx++) {
                    uint32_t phx = vx * sx + dx;
                    if (phx >= fb_width) break;
                    uint32_t *px = (uint32_t *)(row_base +
                                                phx * bytes_per_pixel);
                    *px = rgb;
                }
            }
        }
    }
}

void render_taskbar(void)
{
    draw_rect(0, 180, SCREEN_W, 20, theme_taskbar_color);
    draw_rect(2, 182, 30, 16, 15);  /* "Start" button */
    draw_text(5, 187, "START", 0);
}

void init_system(void)
{
    load_defaults();
}
