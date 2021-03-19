#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tv-data.h"

FILE *read_channel_file(void) {
    char *directory = getenv("HOME");
    strcat(directory, "/.local/share/m3u-tv/channels.m3u");

    FILE *channel_file;
    channel_file = fopen(directory, "r");

    return channel_file;
}

struct tv_data get_tv_data() {
    struct channel *channels = malloc(sizeof(struct channel));
    int channel_amount = 0;

    int buffer_length = 1024;
    char char_buffer[buffer_length];
    FILE *channel_file = read_channel_file();

    int uri_line = 0;

    while (fgets(char_buffer, buffer_length, channel_file)) {
        if (strncmp(char_buffer, "#EXTINF:", strlen("#EXTINF:")) == 0) {
            char channel_number[1024];
            char channel_name[1024];

            sscanf(char_buffer, "#EXTINF:-1 tvg-chno=\"%[^\"]\" tvg-name=\"%[^\"]\"", channel_number, channel_name);

            channels = realloc(channels, (channel_amount + 1) * sizeof(struct channel));
            strcpy(channels[channel_amount].number, channel_number);
            strcpy(channels[channel_amount].name, channel_name);

            uri_line = 1;
        } else if (uri_line) {
            char_buffer[strcspn(char_buffer, "\n")] = 0;

            strcpy(channels[channel_amount].uri, char_buffer);

            channel_amount++;
            uri_line = 0;
        }
    }

    fclose(channel_file);

    return (struct tv_data) {channel_amount, channels};
}
