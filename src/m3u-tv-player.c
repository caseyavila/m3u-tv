#include <stdio.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>

#include <epoxy/egl.h>
#include <epoxy/glx.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "m3u-tv-player.h"

char *format_time(double seconds, gboolean show_hour) {
    int seconds_int = floor(seconds);
	char *result = NULL;

	if (show_hour) {
		result = g_strdup_printf("%02d:%02d:%02d", seconds_int / 3600, (seconds_int % 3600) / 60, seconds_int % 60);
	} else {
		result = g_strdup_printf("%02d:%02d", seconds_int / 60, seconds_int % 60);
	}

	return result;
}

static void update_label(struct time_label label, double time) {
	int sec = floor(time);
	int duration = floor(label.duration);
	char *text;

	if (duration > 0) {
		gchar *sec_str = format_time(sec, duration >= 3600);
		gchar *duration_str = format_time(duration, duration >= 3600);

		text = g_strdup_printf("%s/%s", sec_str, duration_str);

		g_free(sec_str);
		g_free(duration_str);
	} else {
		text = g_strdup_printf("%02d:%02d", (sec%3600)/60, sec%60);
	}

	gtk_label_set_text(GTK_LABEL (label.label), text);
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

gboolean process_events(gpointer data) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) data;
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
                        player->label.duration = *(double *) prop->data;
                    } else if (strcmp(prop->name, "time-pos") == 0) {
                        gtk_range_set_value(GTK_RANGE (player->scale), *(double *) prop->data);
                        update_label(player->label, *(double *) prop->data);
                    }
                }
            }
            break;

            default: ;
        }
    }

    return FALSE;
}

static void on_mpv_events(void *ctx) {
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, process_events, ctx, NULL);
}

void mpv_init(struct m3u_tv_player *player) {
    setlocale(LC_NUMERIC, "C");
    player->handle = mpv_create();
    player->pause = 0;

    mpv_initialize(player->handle);
    //mpv_request_log_messages(player->handle, "debug");

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
}
