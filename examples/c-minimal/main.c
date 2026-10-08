/*
 * c-minimal: libgaiadesk_embed from plain C, in the order an app calls it.
 *
 *   GAIADESK_EMBED_TOKEN=gdemb_… ./c-minimal
 *
 * 1. Ask the user (the library's dialog; your own where it has none).
 * 2. Start with their answer and the masks you already know.
 * 3. Print events until the session ends (the customer pressed Stop on the
 *    indicator, the session expired, …) or Ctrl+C, which stops it.
 *
 * A console program has no window of its own, so with the default capture
 * ("window", the app's own window) the library reports an error and ends:
 * this example shows the calls; examples/qt-helpdesk is a real app. In your
 * app, pass its window as "window_id".
 *
 * Environment: GAIADESK_SERVER (default https://gaiadesk.net),
 * GAIADESK_DEMO_CAPTURE (window | app | screen), GAIADESK_DEMO_GUIDED=1, and,
 * for automated tests only, GAIADESK_DEMO_TEST_CONSENT=1 (skips the dialog:
 * never do that in an app).
 *
 * On macOS the library's indicator lives on the main run loop, so the main
 * thread runs it while the session is up.
 *
 * SPDX-License-Identifier: MIT
 */
#include <gaiadesk_embed.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

/* ---- "the session ended" / "Ctrl+C", across threads ---------------------- */
#if defined(__APPLE__)
#include <pthread.h>
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_woken = 0;
static void wake_main(void) {
    pthread_mutex_lock(&g_mu);
    g_woken = 1;
    pthread_mutex_unlock(&g_mu);
    CFRunLoopStop(CFRunLoopGetMain());
}
static int woken(void) {
    int w;
    pthread_mutex_lock(&g_mu);
    w = g_woken;
    pthread_mutex_unlock(&g_mu);
    return w;
}
static void on_sigint(void *ctx) { (void)ctx; wake_main(); }
#elif defined(_WIN32)
static HANDLE g_wake;
static void wake_main(void) { SetEvent(g_wake); }
static BOOL WINAPI on_ctrl(DWORD type) { (void)type; wake_main(); return TRUE; }
#else
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static int g_woken = 0;
static volatile sig_atomic_t g_interrupted = 0;
static void wake_main(void) {
    pthread_mutex_lock(&g_mu);
    g_woken = 1;
    pthread_cond_signal(&g_cv);
    pthread_mutex_unlock(&g_mu);
}
static void on_sigint(int sig) { (void)sig; g_interrupted = 1; }
#endif

static void wait_for_end_or_ctrl_c(void) {
#if defined(__APPLE__)
    /* Run the main run loop (the indicator needs it) until woken. */
    while (!woken()) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
#elif defined(_WIN32)
    WaitForSingleObject(g_wake, INFINITE);
#else
    pthread_mutex_lock(&g_mu);
    while (!g_woken && !g_interrupted) {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        t.tv_nsec += 200 * 1000 * 1000;
        if (t.tv_nsec >= 1000000000L) { t.tv_sec += 1; t.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&g_cv, &g_mu, &t);
    }
    pthread_mutex_unlock(&g_mu);
#endif
}

/* ---- events ---------------------------------------------------------------- */
/* Called on the library's threads: print and return quickly. "ended" is
 * always the last event, exactly once. */
static void on_event(void *user_data, const char *event_json) {
    (void)user_data;
    printf("event %s\n", event_json);
    fflush(stdout);
    if (strstr(event_json, "\"type\":\"ended\"") != NULL) wake_main();
}

/* ---- consent --------------------------------------------------------------- */
static int ask_on_stdin(const char *company) {
    char line[16];
    printf("Share this window with %s support? They will see this app's window, not the rest of your screen. [y/N] ",
           company);
    fflush(stdout);
    if (fgets(line, sizeof line, stdin) == NULL) return 0;
    return line[0] == 'y' || line[0] == 'Y';
}

static void json_string(char *out, size_t size, const char *s) {
    /* A minimal JSON string writer for values from the environment. */
    size_t n = 0;
    if (size < 3) { if (size) out[0] = '\0'; return; }
    out[n++] = '"';
    for (; *s && n + 8 < size; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c < 0x20) { n += (size_t)snprintf(out + n, size - n, "\\u%04x", c); }
        else out[n++] = (char)c;
    }
    out[n++] = '"';
    out[n] = '\0';
}

int main(void) {
    const char *company = "Acme";
    const char *token = getenv("GAIADESK_EMBED_TOKEN");
    const char *server = getenv("GAIADESK_SERVER");
    const char *capture = getenv("GAIADESK_DEMO_CAPTURE");
    int guided = getenv("GAIADESK_DEMO_GUIDED") && strcmp(getenv("GAIADESK_DEMO_GUIDED"), "1") == 0;
    int test_consent = getenv("GAIADESK_DEMO_TEST_CONSENT") && strcmp(getenv("GAIADESK_DEMO_TEST_CONSENT"), "1") == 0;
    int agreed;
    char token_json[256], server_json[512], config[1024];
    GdEmbedCallbacks callbacks;
    GdEmbed *help;

    printf("libgaiadesk_embed %s %s\n", gd_embed_version(), gd_embed_build_info());
    if (token == NULL || *token == '\0') {
        fprintf(stderr, "set GAIADESK_EMBED_TOKEN to a support session's embed_token (gdemb_…)\n");
        return 2;
    }
    if (capture == NULL || *capture == '\0') capture = "window";

    /* 1. Consent. 1 agreed, 0 declined, -1 no native dialog (Linux), -2
     * wrong thread (macOS: main thread only). */
    if (test_consent) {
        agreed = 1;
    } else {
        int answer = gd_embed_show_consent(company, capture, guided);
        agreed = answer < 0 ? ask_on_stdin(company) : answer;
    }
    if (!agreed) {
        printf("declined: nothing is shared\n");
        return 0;
    }

    /* 2. Start with the user's answer. Masks you know go in the config, so
     * they cover the first frame (here: a 300x40 region at 20,80 of the
     * window; points on macOS, GetWindowRect units on Windows). */
    json_string(token_json, sizeof token_json, token);
    json_string(server_json, sizeof server_json, server ? server : "https://gaiadesk.net");
    snprintf(config, sizeof config,
             "{\"embed_token\":%s,\"consent_granted\":true,\"server\":%s,\"company\":\"%s\","
             "\"capture\":\"%s\",\"guided\":%s,\"masks\":[{\"x\":20,\"y\":80,\"w\":300,\"h\":40}]}",
             token_json, server_json, company, capture, guided ? "true" : "false");

#if defined(_WIN32)
    g_wake = CreateEventW(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(on_ctrl, TRUE);
#elif defined(__APPLE__)
    {
        dispatch_source_t sig = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGINT, 0, dispatch_get_main_queue());
        signal(SIGINT, SIG_IGN); /* delivered through the dispatch source instead */
        dispatch_source_set_event_handler_f(sig, on_sigint);
        dispatch_resume(sig);
    }
#else
    signal(SIGINT, on_sigint);
#endif

    callbacks.on_event = on_event;
    callbacks.user_data = NULL;
    help = gd_embed_start(config, callbacks);
    if (help == NULL) {
        /* Read it now: it is per thread. {"code":"bad_token","message":"…"} */
        fprintf(stderr, "refused: %s\n", gd_embed_last_error());
        return 1;
    }

    /* 3. Until it ends, or Ctrl+C. */
    wait_for_end_or_ctrl_c();
    gd_embed_stop(help); /* ends it if still up ("ended":"app_stopped"), frees the handle */
    printf("done\n");
    return 0;
}
