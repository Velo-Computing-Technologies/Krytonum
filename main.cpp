/*
 * Krytonium OS — kernel_main
 *
 * Entry point called by the x86_64 assembly stub (_start) after Limine
 * has set up long mode, paging, and provided the UEFI framebuffer.
 */

#include "orion.h"
#include "limine.h"
#include "ps2kbd.h"
#include "net.h"

/* -----------------------------------------------------------------------
 * Limine protocol requests — must be in the ".requests" section so the
 * bootloader can find them by scanning the kernel image.
 * ----------------------------------------------------------------------- */

/* Declare maximum protocol revision we support (revision 2) */
LIMINE_BASE_REVISION(2);

__attribute__((section(".requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = 0,
};

__attribute__((section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = 0,
};

/* -----------------------------------------------------------------------
 * Simple busy-wait delay
 * ----------------------------------------------------------------------- */
static void delay_loop(unsigned long n)
{
    for (volatile unsigned long i = 0; i < n; i++) {}
}

/* -----------------------------------------------------------------------
 * Kernel entry — called from _start in kernel_entry.asm
 * ----------------------------------------------------------------------- */
extern "C" void kernel_main(void)
{
    /* ------------------------------------------------------------------
     * Initialise framebuffer from Limine response
     * ------------------------------------------------------------------ */
    if (fb_request.response && fb_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb =
            fb_request.response->framebuffers[0];
        fb_init(fb->address,
                (uint32_t)fb->width,
                (uint32_t)fb->height,
                (uint32_t)fb->pitch,
                fb->bpp);
    }

    /* ------------------------------------------------------------------
     * System and peripheral initialisation
     * ------------------------------------------------------------------ */
    init_system();
    ps2kbd_init();
    net_init();      /* locate e1000, ARP setup etc. — non-fatal if absent */

    /* ------------------------------------------------------------------
     * Main loop
     * ------------------------------------------------------------------ */
    while (true) {
        /* Feed keystrokes to the active screen */
        int ch = ps2kbd_poll();
        if (ch != -1) {
            if (!is_authenticated())
                login_key(ch);
        }

        /* Process incoming network packets */
        net_poll();

        /* Render */
        if (is_authenticated())
            render_home_screen();
        else
            render_login_screen();

        flush_buffer_to_screen();

        delay_loop(50000);   /* ~1 ms throttle to avoid busy-burning the CPU */
    }
}
