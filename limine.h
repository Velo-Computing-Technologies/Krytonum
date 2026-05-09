/*
 * Limine protocol header — compatible with Limine v5/v6/v12
 * Provides the minimal set of features required by Krytonium:
 *   - Base revision declaration
 *   - Framebuffer request
 *   - Higher-half direct-map (HHDM) request
 */
#ifndef LIMINE_H
#define LIMINE_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Magic IDs shared by all requests
 * ----------------------------------------------------------------------- */
#define LIMINE_COMMON_MAGIC \
    0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL

/* -----------------------------------------------------------------------
 * Base revision — kernel must declare this so Limine knows the maximum
 * protocol revision it understands.  We support revision 2.
 * ----------------------------------------------------------------------- */
#define LIMINE_BASE_REVISION(N) \
    static volatile uint64_t limine_base_revision[3] = { \
        0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bbcULL, (N) \
    }

/* -----------------------------------------------------------------------
 * Framebuffer
 * ----------------------------------------------------------------------- */
struct limine_framebuffer {
    void    *address;       /* virtual address (HHDM) usable by kernel */
    uint64_t width;
    uint64_t height;
    uint64_t pitch;         /* bytes per row */
    uint16_t bpp;           /* bits per pixel (typically 32) */
    uint8_t  memory_model;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  unused[7];
    uint64_t edid_size;
    void    *edid;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

#define LIMINE_FRAMEBUFFER_REQUEST \
    { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL }

/* -----------------------------------------------------------------------
 * Higher-Half Direct Map offset
 * ----------------------------------------------------------------------- */
struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

#define LIMINE_HHDM_REQUEST \
    { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL }

#endif /* LIMINE_H */
