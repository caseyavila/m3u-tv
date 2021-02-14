#include <stdio.h>
#include <locale.h>

#include <gtk/gtk.h>

#include <epoxy/glx.h>
#include <gdk/gdkx.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

struct m3u_tv_player {
    mpv_handle *handle;
    mpv_render_context *render_context;

    int width;
    int height;

    int redraw;
};

static void on_mpv_events(void *ctx) {
    printf("MPV: event\n");
}

static void on_mpv_render_update(void *ctx) {
    struct m3u_tv_player *player = (struct m3u_tv_player*) ctx;

    printf("MPV: render event, %d\n", player->redraw);

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
    printf("GTK: render (%d, %d); %d\n", player->width, player->height, player->redraw);

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
    }

    if (player->redraw) {
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

    player->redraw = 1;

    mpv_set_wakeup_callback(player->handle, on_mpv_events, NULL);
    mpv_render_context_set_update_callback(player->render_context, on_mpv_render_update, player);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please supply a video file name...\n");
        exit(1);
    }

    gtk_init(&argc, &argv);

    struct m3u_tv_player *player = malloc(sizeof *player);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *gl_area = gtk_gl_area_new();

    g_signal_connect(window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect(gl_area, "render", G_CALLBACK (render), player);
    g_signal_connect(gl_area, "realize", G_CALLBACK (realize), NULL);
    g_signal_connect(gl_area, "resize", G_CALLBACK (resize), player);
    
    gtk_container_add(GTK_CONTAINER (window), gl_area);

    gtk_widget_show_all(window);

    mpv_init(player);
    const char *cmd[] = {"loadfile", argv[1], NULL};
    mpv_command(player->handle, cmd);

    gtk_main();

    mpv_render_context_free(player->render_context);
    mpv_detach_destroy(player->handle);

    return 0;
}
