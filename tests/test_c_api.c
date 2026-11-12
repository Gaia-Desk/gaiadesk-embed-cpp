/* The C API from C (C99), against the real library: the header compiles as
 * C, and the calls a C app makes on bad input are refused without a session.
 * SPDX-License-Identifier: MIT */
#include <gaiadesk_embed.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #c); \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static int events = 0;
static void on_event(void *user_data, const char *json) {
    (void)user_data;
    (void)json;
    ++events;
}

#define TOKEN "gdemb_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

static int refused_with(const char *config, const char *code) {
    GdEmbedCallbacks cb;
    GdEmbed *h;
    char want[64];
    cb.on_event = on_event;
    cb.user_data = NULL;
    h = gd_embed_start(config, cb);
    if (h != NULL) {
        gd_embed_stop(h);
        fprintf(stderr, "started: %s\n", config);
        return 0;
    }
    snprintf(want, sizeof want, "\"code\":\"%s\"", code);
    if (strstr(gd_embed_last_error(), want) == NULL) {
        fprintf(stderr, "for %s\n  want %s\n  got  %s\n", config, code, gd_embed_last_error());
        return 0;
    }
    return 1;
}

int main(void) {
    GdEmbedCallbacks none;
    GdRect r;
    none.on_event = NULL;
    none.user_data = NULL;

    CHECK(strcmp(gd_embed_version(), "0.3.0") == 0);
    CHECK(strstr(gd_embed_build_info(), "\"test_source\":false") != NULL);

    CHECK(refused_with("{\"embed_token\":\"" TOKEN "\",\"consent_granted\":false}", "consent_required"));
    CHECK(refused_with("{\"embed_token\":\"" TOKEN "\",\"consent_granted\":\"true\"}", "consent_required"));
    CHECK(refused_with("{\"embed_token\":\"gdemb_nope\",\"consent_granted\":true}", "bad_token"));
    CHECK(refused_with("{\"embed_token\":\"" TOKEN "\",\"consent_granted\":true,\"masks\":[{\"x\":0,\"y\":0,\"w\":-1,\"h\":1}]}",
                       "bad_config"));
    CHECK(refused_with("{\"embed_token\":\"" TOKEN "\",\"consent_granted\":true,\"masks\":[{\"x\":0,\"y\":0,\"w\":1}]}",
                       "bad_config"));
    /* No switch turns the indicator off: unknown keys are refused. */
    CHECK(refused_with("{\"embed_token\":\"" TOKEN "\",\"consent_granted\":true,\"indicator\":false}", "bad_config"));
    CHECK(refused_with("not json", "bad_config"));

    CHECK(gd_embed_start(NULL, none) == NULL);
    CHECK(strstr(gd_embed_last_error(), "bad_config") != NULL);

    r.x = 0; r.y = 0; r.w = 1; r.h = 1;
    CHECK(gd_embed_set_mask_rects(NULL, &r, 1) == -1);
    CHECK(strstr(gd_embed_last_error(), "bad_argument") != NULL);
    CHECK(gd_embed_set_paused(NULL, 1) == -1);
    gd_embed_stop(NULL); /* ignored */

    CHECK(events == 0);
    printf("%s (%d failures)\n", failures ? "FAIL" : "ok", failures);
    return failures ? 1 : 0;
}
