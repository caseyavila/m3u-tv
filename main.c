#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <gtk/gtk.h>

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <glib.h>
#include <gdk/gdkx.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

struct mpv_kc {
    mpv_handle *handle;
    mpv_render_context *render_context;
};

static void on_mpv_events(void *ctx) {
    printf("mpv event\n");
}

static void on_mpv_render_update(void *ctx) {
    printf("mpv render event\n");
}

static void *get_proc_address(void *fn_ctx, const gchar *name) {
	GdkDisplay *display = gdk_display_get_default();

#ifdef GDK_WINDOWING_WAYLAND
	if (GDK_IS_WAYLAND_DISPLAY(display))
		return eglGetProcAddress(name);
#endif
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY(display))
		return	(void *)(intptr_t)
			glXGetProcAddressARB((const GLubyte *)name);
#endif
#ifdef GDK_WINDOWING_WIN32
	if (GDK_IS_WIN32_DISPLAY(display))
		return wglGetProcAddress(name);
#endif
	g_assert_not_reached();
	return NULL;
}


static void draw_an_object() {
    // Nothing seems to work here...
}

static gboolean render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    // inside this function it's safe to use GL; the given
    // #GdkGLContext has been made current to the drawable
    // surface used by the #GtkGLArea and the viewport has
    // already been set to be the size of the allocation

    struct mpv_kc *mpv_kc = user_data;

    // we can start by clearing the buffer
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    printf("RENDER\n");

    //while (1) {
    //    mpv_event *event = mpv_wait_event(mpv_kc->handle, 10000);
    //    printf("event: %s\n", mpv_event_name(event->event_id));
    //    if (event->event_id == MPV_EVENT_SHUTDOWN) {
    //        break;
    //    }
    //}

    // draw your object
    draw_an_object();

    // we completed our drawing; the draw commands will be
    // flushed at the end of the signal emission chain, and
    // the buffers will be drawn on the window

    return TRUE;
}

static void realize(GtkGLArea *area) {
    gtk_gl_area_make_current(area);
}

void mpv_init(struct mpv_kc *mpv_kc) {
    setlocale(LC_NUMERIC, "C");
    mpv_kc->handle = mpv_create();

    mpv_initialize(mpv_kc->handle);
    mpv_request_log_messages(mpv_kc->handle, "debug");

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &(mpv_opengl_init_params){
            .get_proc_address = get_proc_address,
        }},

        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &(int){1}},
        {0}
    };

    mpv_render_context_create(&mpv_kc->render_context, mpv_kc->handle, params);

    mpv_set_wakeup_callback(mpv_kc->handle, on_mpv_events, NULL);
    mpv_render_context_set_update_callback(mpv_kc->render_context, on_mpv_render_update, NULL);


    // Play the file
    const char *cmd[] = {"loadfile", "/home/casey/hi.mp4", NULL};
    mpv_command_async(mpv_kc->handle, 0, cmd);

}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    struct mpv_kc *mpv_kc = malloc(sizeof *mpv_kc);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *gl_area = gtk_gl_area_new();

    g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (gl_area, "render", G_CALLBACK (render), NULL);
    g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
    
    gtk_container_add(GTK_CONTAINER (window), gl_area);

    gtk_widget_show_all(window);

    mpv_init(mpv_kc);

    gtk_main();

    mpv_terminate_destroy(mpv_kc->handle);

    return 0;
}
