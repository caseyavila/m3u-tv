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

    int width;
    int height;

    int wakeup;
};

static void on_mpv_events(void *ctx) {
    printf("MPV: event\n");
}

static void on_mpv_render_update(void *ctx) {
    struct mpv_kc *mpv_kc = (struct mpv_kc*) ctx;

    printf("MPV: render event, %d\n", mpv_kc->wakeup);

    mpv_kc->wakeup = 1;
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

void render_frame(struct mpv_kc *mpv_kc) {
	if ((mpv_render_context_update(mpv_kc->render_context) & MPV_RENDER_UPDATE_FRAME)) {
		gint fbo = -1;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

		mpv_opengl_fbo opengl_fbo =
			{fbo, mpv_kc->width, mpv_kc->height, 0};
		mpv_render_param params[] =
			{	{MPV_RENDER_PARAM_OPENGL_FBO, &opengl_fbo},
				{MPV_RENDER_PARAM_FLIP_Y, &(int){1}},
				{MPV_RENDER_PARAM_INVALID, NULL} };

		mpv_render_context_render(mpv_kc->render_context, params);
	}

    if (mpv_kc->wakeup) {
        mpv_render_context_report_swap(mpv_kc->render_context);
        mpv_kc->wakeup = 0;
    }
}

static gboolean render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    struct mpv_kc *mpv_kc = user_data;
    printf("GTK: render (%d, %d); %d\n", mpv_kc->width, mpv_kc->height, mpv_kc->wakeup);
    // inside this function it's safe to use GL; the given
    // #GdkGLContext has been made current to the drawable
    // surface used by the #GtkGLArea and the viewport has
    // already been set to be the size of the allocation

    render_frame(mpv_kc);

    gtk_gl_area_queue_render(area);

    return TRUE;
}

static void realize(GtkGLArea *area) {
    printf("GTK: realize\n");
    gtk_gl_area_make_current(area);
}

static void resize(GtkGLArea *area, int width, int height, gpointer user_data) {
    struct mpv_kc *mpv_kc = user_data;

    mpv_kc->width = width;
    mpv_kc->height = height;
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

    mpv_kc->wakeup = 1;

    mpv_set_wakeup_callback(mpv_kc->handle, on_mpv_events, NULL);
    mpv_render_context_set_update_callback(mpv_kc->render_context, on_mpv_render_update, mpv_kc);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Please supply file name...\n");
        exit(1);
    }

    gtk_init(&argc, &argv);

    struct mpv_kc *mpv_kc = malloc(sizeof *mpv_kc);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *gl_area = gtk_gl_area_new();

    g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (gl_area, "render", G_CALLBACK (render), mpv_kc);
    g_signal_connect (gl_area, "realize", G_CALLBACK (realize), NULL);
    g_signal_connect (gl_area, "resize", G_CALLBACK (resize), mpv_kc);
    
    gtk_container_add(GTK_CONTAINER (window), gl_area);

    gtk_widget_show_all(window);

    mpv_init(mpv_kc);
    const char *cmd[] = {"loadfile", argv[1], NULL};
    mpv_command(mpv_kc->handle, cmd);

    gtk_main();

    mpv_render_context_free(mpv_kc->render_context);
    mpv_detach_destroy(mpv_kc->handle);

    return 0;
}
