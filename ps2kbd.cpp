/*
 * PS/2 keyboard driver (polling, no IRQ required).
 *
 * Works with the PS/2 controller present in both QEMU and on real PC
 * hardware (including Surface 4 with a USB keyboard bridged by firmware
 * to the legacy PS/2 interface via the i8042 compatibility layer).
 *
 * Only US QWERTY layout is handled; Shift and Caps-Lock are supported.
 */

#include "ps2kbd.h"
#include "io.h"

#define KBD_DATA 0x60
#define KBD_STAT 0x64

/* Set-1 scan-code → ASCII, unshifted */
static const char sc_ascii[128] = {
/*00*/  0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
/*0F*/ '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
/*1D*/  0,  /* LCtrl */
/*1E*/ 'a','s','d','f','g','h','j','k','l',';','\'','`',
/*2A*/  0,  /* LShift */
/*2B*/ '\\',
/*2C*/ 'z','x','c','v','b','n','m',',','.','/',
/*36*/  0,  /* RShift */
/*37*/ '*',
/*38*/  0,  /* LAlt */
/*39*/ ' ',
/*3A*/  0,  /* Caps */
/* F1-F10, NumLock, Scroll, KP7-KP9, KP-, KP4-KP6, KP+, KP1-KP3, KP0, KP. */
/*3B*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*54*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* pad */
/*70*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* Shift map for the characters above that have an upper-case / shifted form */
static const char sc_shift[128] = {
/*00*/  0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
/*0F*/ '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
/*1D*/  0,
/*1E*/ 'A','S','D','F','G','H','J','K','L',':','"','~',
/*2A*/  0,
/*2B*/ '|',
/*2C*/ 'Z','X','C','V','B','N','M','<','>','?',
/*36*/  0,
/*37*/ '*',
/*38*/  0,
/*39*/ ' ',
/*3A*/  0,
/*3B*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*54*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/*70*/  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static int shift_held  = 0;
static int caps_locked = 0;

void ps2kbd_init(void)
{
    /* Flush any stale bytes in the keyboard buffer */
    while (inb(KBD_STAT) & 0x01)
        inb(KBD_DATA);
}

int ps2kbd_poll(void)
{
    if (!(inb(KBD_STAT) & 0x01))
        return -1;          /* nothing in buffer */

    uint8_t sc = inb(KBD_DATA);

    /* Key release: top bit set */
    if (sc & 0x80) {
        uint8_t base = sc & 0x7F;
        if (base == 0x2A || base == 0x36)   /* LShift / RShift */
            shift_held = 0;
        return -1;
    }

    /* Key press */
    if (sc == 0x2A || sc == 0x36) { shift_held  = 1; return -1; }
    if (sc == 0x3A)               { caps_locked ^= 1; return -1; }
    if (sc >= 128)                  return -1;

    int use_shift = shift_held;

    char c;
    if (use_shift)
        c = sc_shift[sc];
    else
        c = sc_ascii[sc];

    if (!c) return -1;

    /* Apply Caps-Lock for letters */
    if (caps_locked && c >= 'a' && c <= 'z') c -= 32;
    else if (caps_locked && c >= 'A' && c <= 'Z') c += 32;

    return (unsigned char)c;
}
