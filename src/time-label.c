#include "time-label.h"

char *format_time(int seconds, gboolean show_hour) {
	char *result = NULL;

	if (show_hour) {
		result = g_strdup_printf(	"%02d:%02d:%02d",
						seconds/3600,
						(seconds%3600)/60,
						seconds%60 );
	} else {
		result = g_strdup_printf("%02d:%02d", seconds/60, seconds%60);
	}

	return result;
}

static void update_label(struct time_label label) {
	int sec = label.time;
	int duration = label.duration;
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
