#include <stdio.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>

#include <epoxy/glx.h>
#include <gdk/gdkx.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "m3u-tv-player.h"

static void on_mpv_events(void *ctx) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) ctx;

    player->has_events = 1;
}

static void on_mpv_render_update(void *ctx) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) ctx;

    player->redraw = 1;
}

void process_events(struct m3u_tv_player* player) {
    int done = 0;
    while (!done) {
        mpv_event *event = mpv_wait_event(player->handle, 0);

        switch (event->event_id) {
            case MPV_EVENT_NONE:
                done = 1;
                break;

            case MPV_EVENT_LOG_MESSAGE:
                printf("log: %s", ((mpv_event_log_message *) event->data)->text);
                break;

            case MPV_EVENT_PROPERTY_CHANGE: {
                mpv_event_property *prop = (mpv_event_property *)event->data;
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    if (strcmp(prop->name, "duration") == 0) {
                        gtk_range_set_range(GTK_RANGE (player->scale), 0, *(double *) prop->data);
                    } else if (strcmp(prop->name, "time-pos") == 0) {
                        gtk_range_set_value(GTK_RANGE (player->scale), *(double *) prop->data);
                    }
                }
            }
            break;

            default: ;
        }
    }
}

static void *get_proc_address(void *fn_ctx, const gchar *name) {
    GdkDisplay *display = gdk_display_get_default();

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(display)) {
        return eglGetProcAddress(name);
    }
#endif
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(display)) {
        return (void *)(intptr_t) glXGetProcAddressARB((const GLubyte *)name);
    }
#endif
#ifdef GDK_WINDOWING_WIN32
    if (GDK_IS_WIN32_DISPLAY(display)) {
        return wglGetProcAddress(name);
    }
#endif
    g_assert_not_reached();
    return NULL;
}

void mpv_init(struct m3u_tv_player *player) {
    setlocale(LC_NUMERIC, "C");
    player->handle = mpv_create();
    player->redraw = 1;
    player->has_events = 0;
    player->pause = 0;

    mpv_initialize(player->handle);
    mpv_request_log_messages(player->handle, "debug");

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &(mpv_opengl_init_params){
            .get_proc_address = get_proc_address,
        }},

        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &(int){1}},
        {0}
    };

    mpv_render_context_create(&player->render_context, player->handle, params);

    mpv_observe_property(player->handle, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(player->handle, 0, "time-pos", MPV_FORMAT_DOUBLE);

    mpv_set_wakeup_callback(player->handle, on_mpv_events, player);
    mpv_render_context_set_update_callback(player->render_context, on_mpv_render_update, player);
}


