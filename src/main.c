#include <gtk/gtk.h>

#include <epoxy/glx.h>
#include <gdk/gdkx.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "m3u-tv-player.h"
#include "tv-data.h"

static gboolean render(GtkGLArea *area, GdkGLContext *context, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

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
    }

    gtk_gl_area_queue_render(area);

    return TRUE;
}

static void realize(GtkGLArea *area) {
    gtk_gl_area_make_current(area);
}

static void resize(GtkGLArea *area, int width, int height, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    player->width = width;
    player->height = height;
}

void seek_absolute(GtkRange *range, GtkScrollType scroll, double value, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr(buf, G_ASCII_DTOSTR_BUF_SIZE, value);

    const char *cmd[] = {"seek", buf, "absolute", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

void button_play_pause_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;
    gboolean value;

    if (player->pause) {
        value = FALSE;
        gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON));
        player->pause = 0;
    } else {
        value = TRUE;
        gtk_button_set_image(GTK_BUTTON (button), gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON));
        player->pause = 1;
    }

    mpv_set_property(player->handle, "pause", MPV_FORMAT_FLAG, &value);
}

void button_seek_backward_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    const char *cmd[] = {"seek", "-10", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

void button_seek_forward_clicked(GtkButton *button, gpointer user_data) {
    struct m3u_tv_player *player = user_data;

    const char *cmd[] = {"seek", "10", NULL};
    mpv_command_async(player->handle, 0, cmd);
}

void
channel_clicked(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    struct m3u_tv_player *player = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *uri;
        gtk_tree_model_get(model, &iter, 2, &uri, -1);
        const char *cmd[] = {"loadfile", uri, NULL};
        mpv_command_async(player->handle, 0, cmd);
        g_free(uri);
    }
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    struct m3u_tv_player *player = malloc(sizeof *player);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *notebook = gtk_notebook_new();
    GtkWidget *grid_player = gtk_grid_new();

    player->gl_area = gtk_gl_area_new();

    GtkWidget *button_seek_backward = gtk_button_new_from_icon_name("media-seek-backward", GTK_ICON_SIZE_BUTTON);
    GtkWidget *button_play_pause = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    GtkWidget *button_seek_forward = gtk_button_new_from_icon_name("media-seek-forward", GTK_ICON_SIZE_BUTTON);

    g_object_set(button_seek_forward, "relief", GTK_RELIEF_NONE, NULL);
    g_object_set(button_seek_backward, "relief", GTK_RELIEF_NONE, NULL);
    g_object_set(button_play_pause, "relief", GTK_RELIEF_NONE, NULL);

    player->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE (player->scale), FALSE);

    GtkWidget *label_player = gtk_label_new("Player");
    GtkWidget *label_guide = gtk_label_new("Guide");

    gtk_grid_attach(GTK_GRID (grid_player), player->gl_area, 0, 0, 4, 1);
    gtk_widget_set_hexpand(player->gl_area, TRUE);
    gtk_widget_set_vexpand(player->gl_area, TRUE);

    gtk_grid_attach(GTK_GRID (grid_player), button_seek_backward, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID (grid_player), button_play_pause, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID (grid_player), button_seek_forward, 2, 1, 1, 1);

    gtk_grid_attach(GTK_GRID (grid_player), player->scale, 3, 1, 1, 1);
    gtk_widget_set_hexpand(player->scale, TRUE);

    gtk_notebook_set_tab_pos(GTK_NOTEBOOK (notebook), GTK_POS_TOP);
    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), grid_player, label_player);

    g_signal_connect(window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect(player->gl_area, "render", G_CALLBACK (render), player);
    g_signal_connect(player->gl_area, "realize", G_CALLBACK (realize), NULL);
    g_signal_connect(player->gl_area, "resize", G_CALLBACK (resize), player);
    g_signal_connect(GTK_RANGE (player->scale), "change-value", G_CALLBACK(seek_absolute), player);
    g_signal_connect(button_seek_backward, "clicked", G_CALLBACK (button_seek_backward_clicked), player);
    g_signal_connect(button_play_pause, "clicked", G_CALLBACK (button_play_pause_clicked), player);
    g_signal_connect(button_seek_forward, "clicked", G_CALLBACK (button_seek_forward_clicked), player);
    
    struct tv_data data = get_tv_data();
    GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkTreeIter iter;
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL (store));
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree_view), -1, "Number", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW (tree_view), -1, "Name", gtk_cell_renderer_text_new(), "text", 1, NULL);

    for (int i = 0; i < data.channel_amount; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, data.channels[i].number, 1, data.channels[i].name, 2, data.channels[i].uri, -1);
    }

    g_signal_connect(tree_view, "row-activated", G_CALLBACK (channel_clicked), player);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(tree_view));

    gtk_notebook_append_page(GTK_NOTEBOOK (notebook), scroll, label_guide);

    gtk_container_add(GTK_CONTAINER (window), notebook);
    gtk_widget_show_all(window);

    mpv_init(player);
    if (argc == 2) {
        printf("Playing file path from stdin...\n");
        const char *cmd[] = {"loadfile", argv[1], NULL};
        mpv_command_async(player->handle, 0, cmd);
    }

    gtk_main();

    mpv_render_context_free(player->render_context);
    mpv_detach_destroy(player->handle);
    free(player);

    return 0;
}
