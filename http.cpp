/*
 * Plain HTTP/1.1 POST client for the ESP32 gateway.
 *
 * Builds a JSON body {"email":"...","password":"..."}, sends the
 * request to the ESP32 at the configured IP / port, reads the full
 * response, and searches the body for the substring "access_token".
 *
 * No TLS.  No dynamic memory.  No libc.
 */

#include "http.h"
#include "net.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Minimal string helpers (no libc)
 * ----------------------------------------------------------------------- */
static int my_strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void my_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++)) {}
}

static void my_strcat(char *dst, const char *src)
{
    my_strcpy(dst + my_strlen(dst), src);
}

/* Convert an int to its decimal string representation in buf */
static void int_to_str(int v, char *buf)
{
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char tmp[16];
    int i = 0;
    while (v > 0) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}

/* Search for needle in haystack (both NUL-terminated strings).
 * Returns 1 if found, 0 otherwise. */
static int str_contains(const char *haystack, const char *needle)
{
    int hlen = my_strlen(haystack);
    int nlen = my_strlen(needle);
    for (int i = 0; i <= hlen - nlen; i++) {
        int ok = 1;
        for (int j = 0; j < nlen; j++) {
            if (haystack[i + j] != needle[j]) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Convert IPv4 host-order address to dotted-decimal string
 * ----------------------------------------------------------------------- */
static void ip_to_str(uint32_t ip, char *buf)
{
    buf[0] = 0;
    char tmp[8];
    int_to_str((ip >> 24) & 0xFF, tmp); my_strcat(buf, tmp); my_strcat(buf, ".");
    int_to_str((ip >> 16) & 0xFF, tmp); my_strcat(buf, tmp); my_strcat(buf, ".");
    int_to_str((ip >>  8) & 0xFF, tmp); my_strcat(buf, tmp); my_strcat(buf, ".");
    int_to_str( ip        & 0xFF, tmp); my_strcat(buf, tmp);
}

/* -----------------------------------------------------------------------
 * Public: HTTP POST login
 * ----------------------------------------------------------------------- */
int http_post_login(uint32_t ip, uint16_t port,
                    const char *path,
                    const char *api_key,
                    const char *email,
                    const char *password)
{
    /* Build JSON body: {"email":"<e>","password":"<p>"} */
    char body[256];
    body[0] = 0;
    my_strcat(body, "{\"email\":\"");
    my_strcat(body, email);
    my_strcat(body, "\",\"password\":\"");
    my_strcat(body, password);
    my_strcat(body, "\"}");
    int body_len = my_strlen(body);

    /* Build Host header value */
    char host_str[32];
    ip_to_str(ip, host_str);

    /* Build the full HTTP request */
    char req[1024];
    req[0] = 0;

    char len_str[16];
    int_to_str(body_len, len_str);

    my_strcat(req, "POST ");
    my_strcat(req, path);
    my_strcat(req, " HTTP/1.1\r\n");
    my_strcat(req, "Host: ");
    my_strcat(req, host_str);
    my_strcat(req, "\r\n");
    my_strcat(req, "X-API-Key: ");
    my_strcat(req, api_key);
    my_strcat(req, "\r\n");
    my_strcat(req, "Content-Type: application/json\r\n");
    my_strcat(req, "Content-Length: ");
    my_strcat(req, len_str);
    my_strcat(req, "\r\n");
    my_strcat(req, "Connection: close\r\n");
    my_strcat(req, "\r\n");
    my_strcat(req, body);

    int req_len = my_strlen(req);

    /* Open TCP connection */
    int sock = tcp_connect(ip, port);
    if (sock < 0) return 0;

    /* Send request */
    if (tcp_send(sock, req, req_len) < 0) {
        tcp_close(sock);
        return 0;
    }

    /* Receive response — collect until connection closes */
    static char resp_buf[4096];
    int total = 0;
    for (;;) {
        int n = tcp_recv(sock, resp_buf + total,
                         (int)sizeof(resp_buf) - total - 1);
        if (n <= 0) break;
        total += n;
        if (total >= (int)sizeof(resp_buf) - 1) break;
    }
    resp_buf[total] = 0;

    tcp_close(sock);

    /* Search the entire response buffer (handles chunked encoding too) */
    return str_contains(resp_buf, "access_token");
}
