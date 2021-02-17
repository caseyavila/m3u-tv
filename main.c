#include <stdio.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>

#include <epoxy/glx.h>
#include <gdk/gdkx.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

struct m3u_tv_player {
    mpv_handle *handle;
    mpv_render_context *render_context;

    GtkWidget *gl_area;
    GtkWidget *scale;

    int width;
    int height;

    int redraw;
    int has_events;

    /*
     * Ideally would have a way to poll mpv whether it is paused instead of
     * storing state in memory, but haven't figured that out...
     */
    int pause;
};

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

static void on_mpv_events(void *ctx) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) ctx;

    player->has_events = 1;
}

static void on_mpv_render_update(void *ctx) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) ctx;

    player->redraw = 1;
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

static gboolean render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    if (player->has_events) {
        process_events(player);
        player->has_events = 0;
    }

    // Render frame
    if ((mpv_render_context_update(player->render_context) & MPV_RENDER_UPDATE_FRAME)) {
        gint fbo = -1;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

        mpv_opengl_fbo opengl_fbo = {
            fbo, player->width, player->height, 0
        };
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &opengl_fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &(int){1}},
            {MPV_RENDER_PARAM_INVALID, NULL}
        };

        mpv_render_context_render(player->render_context, params);

        player->redraw = 0;
    }

    gtk_gl_area_queue_render(area);


    return TRUE;
}

static void realize(GtkGLArea *area) {
    printf("GTK: realize\n");
    gtk_gl_area_make_current(area);
}

static void resize(GtkGLArea *area, int width, int height, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    player->width = width;
    player->height = height;
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

void seek_absolute(GtkRange *range, GtkScrollType scroll, double value, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr(buf, G_ASCII_DTOSTR_BUF_SIZE, value);

    const char *cmd[] = {"seek", buf, "absolute", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

static gchar* format_value(GtkScale *scale, gdouble value) {
    int seconds = floor(value);

    if (seconds > 3600) {
        return g_strdup_printf("%d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    } else {
        return g_strdup_printf("%d:%02d", seconds / 60, seconds % 60);
    }
}

void button_play_pause_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    if (player->pause) {
        const char *cmd[] = {"set", "pause", "no", NULL};
        mpv_command_async(player->handle, 0, cmd);
        gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON));

        player->pause = 0;
    } else {
        const char *cmd[] = {"set", "pause", "yes", NULL};
        gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON));
        mpv_command_async(player->handle, 0, cmd);
        player->pause = 1;
    }
}

void button_skip_backward_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    const char *cmd[] = {"seek", "-10", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

void button_seek_backward_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    const char *cmd[] = {"seek", "-30", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please supply a video file name...\n");
        exit(1);
    }

    gtk_init(&argc, &argv);

    struct m3u_tv_player *player = malloc(sizeof *player);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *grid_player = gtk_grid_new();

    player->gl_area = gtk_gl_area_new();

    GtkWidget *button_skip_backward = gtk_button_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_BUTTON);
    g_object_set(button_skip_backward, "relief", GTK_RELIEF_NONE, NULL);

    GtkWidget *button_seek_backward = gtk_button_new_from_icon_name("media-seek-backward", GTK_ICON_SIZE_BUTTON);
    g_object_set(button_seek_backward, "relief", GTK_RELIEF_NONE, NULL);

    GtkWidget *button_play_pause = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    g_object_set(button_play_pause, "relief", GTK_RELIEF_NONE, NULL);

    GtkWidget *button_seek_forward = gtk_button_new_from_icon_name("media-seek-forward", GTK_ICON_SIZE_BUTTON);
    g_object_set(button_seek_forward, "relief", GTK_RELIEF_NONE, NULL);
    
    GtkWidget *button_skip_forward = gtk_button_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_BUTTON);
    g_object_set(button_skip_forward , "relief", GTK_RELIEF_NONE, NULL);

    player->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE (player->scale), FALSE);

    GtkWidget *label_player = gtk_label_new("Player");
    GtkWidget *label_guide = gtk_label_new("Guide");
    GtkWidget *label_guide2 = gtk_label_new("Guide");

    gtk_grid_attach(GTK_GRID (grid_player), player->gl_area, 0, 0, 6, 1);
    gtk_widget_set_hexpand(player->gl_area, TRUE);
    gtk_widget_set_vexpand(player->gl_area, TRUE);

    gtk_grid_attach(GTK_GRID (grid_player), button_skip_backward, 0, 1, 1, 1);
    g_signal_connect(button_seek_backward, "clicked", G_CALLBACK (button_seek_backward_clicked), player);

    gtk_grid_attach(GTK_GRID (grid_player), button_seek_backward, 1, 1, 1, 1);
    g_signal_connect(button_skip_backward, "clicked", G_CALLBACK (button_skip_backward_clicked), player);

    gtk_grid_attach(GTK_GRID (grid_player), button_play_pause, 2, 1, 1, 1);
    g_signal_connect(button_play_pause, "clicked", G_CALLBACK (button_play_pause_clicked), player);

    gtk_grid_attach(GTK_GRID (grid_player), button_seek_forward, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID (grid_player), button_skip_forward, 4, 1, 1, 1);

    gtk_grid_attach(GTK_GRID (grid_player), player->scale, 5, 1, 1, 1);
    gtk_widget_set_hexpand(player->scale, TRUE);

    gtk_notebook_set_tab_pos(GTK_NOTEBOOK (notebook), GTK_POS_TOP);
    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), grid_player, label_player);
    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), label_guide2, label_guide);

    g_signal_connect(window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect(player->gl_area, "render", G_CALLBACK (render), player);
    g_signal_connect(player->gl_area, "realize", G_CALLBACK (realize), NULL);
    g_signal_connect(player->gl_area, "resize", G_CALLBACK (resize), player);

    g_signal_connect(GTK_RANGE (player->scale), "change-value", G_CALLBACK(seek_absolute), player);
    
    gtk_container_add(GTK_CONTAINER (window), notebook);
    gtk_widget_show_all(window);

    mpv_init(player);

    const char *cmd[] = {"loadfile", argv[1], NULL};
    mpv_command_async(player->handle, 0, cmd);

    gtk_main();

    mpv_render_context_free(player->render_context);
    mpv_detach_destroy(player->handle);

    return 0;
}
