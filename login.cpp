#include "orion.h"
#include "http.h"
#include "net.h"

/* -----------------------------------------------------------------------
 * ESP32 gateway configuration
 * ----------------------------------------------------------------------- */
#define ESP32_IP    NET_ESP32_IP
#define ESP32_PORT  NET_ESP32_PORT
#define ESP32_API_KEY \
    "7Y4RHBUhdhueuhruhHAHhHhdhbfyhee8372ujskakmc"

/* -----------------------------------------------------------------------
 * Login state
 * ----------------------------------------------------------------------- */
static bool logged_in_status = false;
static int  login_result = -1;  /* -1=pending, 0=fail, 1=ok */

/* Input fields */
static char email_buf[64];
static char pass_buf[64];
static int  email_len = 0;
static int  pass_len  = 0;
static int  field_focus = 0;  /* 0=email, 1=password */

/* -----------------------------------------------------------------------
 * Keyboard input handler for the login screen
 * ----------------------------------------------------------------------- */
void login_key(int ch)
{
    if (ch == -1) return;

    if (ch == '\t') {
        field_focus ^= 1;
        return;
    }

    if (ch == '\n') {
        attempt_esp32_login();
        return;
    }

    if (ch == '\b') {
        if (field_focus == 0) {
            if (email_len > 0) email_buf[--email_len] = 0;
        } else {
            if (pass_len > 0)  pass_buf[--pass_len]   = 0;
        }
        return;
    }

    if (ch >= 0x20 && ch <= 0x7E) {
        if (field_focus == 0 && email_len < 63) {
            email_buf[email_len++] = (char)ch;
            email_buf[email_len]   = 0;
        } else if (field_focus == 1 && pass_len < 63) {
            pass_buf[pass_len++]  = (char)ch;
            pass_buf[pass_len]    = 0;
        }
    }
}

/* -----------------------------------------------------------------------
 * Fire an HTTP POST to the ESP32 gateway
 * ----------------------------------------------------------------------- */
void attempt_esp32_login(void)
{
    login_result = -1;  /* show "connecting..." */
    int ok = http_post_login(ESP32_IP, ESP32_PORT,
                             "/login",
                             ESP32_API_KEY,
                             email_buf,
                             pass_buf);
    logged_in_status = (ok == 1);
    login_result     = ok ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * Render the login screen onto the 320×200 virtual canvas
 * ----------------------------------------------------------------------- */
void render_login_screen(void)
{
    /* Background */
    draw_rect(0, 0, 320, 200, 0);

    /* Login panel */
    draw_rect(50, 30, 220, 140, 8);   /* dark grey panel */
    draw_rect(52, 32, 216, 136, 0);   /* black inner */

    draw_text(110, 38, "KRYTONIUM", 15);
    draw_text(112, 48, "Secure Login", 7);

    /* Email field */
    draw_text(60, 68, "Email:", 7);
    int email_focused = (field_focus == 0);
    draw_rect(60, 78, 200, 14, email_focused ? 1 : 8);
    draw_rect(61, 79, 198, 12, 0);
    draw_text(63, 81, email_buf, 15);

    /* Password field (masked) */
    draw_text(60, 100, "Password:", 7);
    int pass_focused = (field_focus == 1);
    draw_rect(60, 110, 200, 14, pass_focused ? 1 : 8);
    draw_rect(61, 111, 198, 12, 0);

    /* Show asterisks for the password */
    static char mask[64];
    for (int i = 0; i < pass_len; i++) mask[i] = '*';
    mask[pass_len] = 0;
    draw_text(63, 113, mask, 15);

    /* Login button */
    draw_rect(110, 132, 100, 16, 2);
    draw_text(130, 136, "LOGIN", 0);

    /* Status line */
    if (login_result == 1)
        draw_text(85, 158, "AUTH: OK", 10);
    else if (login_result == 0)
        draw_text(70, 158, "AUTH: FAILED", 12);
    else if (login_result == -1 && email_len == 0)
        draw_text(65, 158, "Tab=switch Enter=login", 7);

    /* Hint */
    draw_text(60, 170, "Tab: switch field", 8);
}

bool is_authenticated(void)
{
    return logged_in_status;
}
