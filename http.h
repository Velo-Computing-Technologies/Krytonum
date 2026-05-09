#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

/*
 * Perform an HTTP POST to the ESP32 gateway and check whether the
 * response body contains "access_token" (indicating a successful login).
 *
 * ip       : destination IPv4 address in host byte order
 * port     : destination TCP port
 * path     : request-URI (e.g. "/login")
 * api_key  : value for the "X-API-Key" header
 * email    : login email
 * password : login password
 *
 * Returns 1 if "access_token" was found in the response, 0 otherwise.
 */
int http_post_login(uint32_t ip, uint16_t port,
                    const char *path,
                    const char *api_key,
                    const char *email,
                    const char *password);

#endif /* HTTP_H */
