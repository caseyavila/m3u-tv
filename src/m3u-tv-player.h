#include <gtk/gtk.h>
#include <mpv/client.h>

struct m3u_tv_player {
    mpv_handle *handle;
    mpv_render_context *render_context;

    GtkWidget *gl_area;
    GtkWidget *scale;

    int width;
    int height;

    int pause;
};

void mpv_init(struct m3u_tv_player *player);
