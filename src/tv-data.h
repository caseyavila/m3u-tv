struct tv_data {
    int channel_amount;
    struct channel *channels;
};

struct tv_data get_tv_data();

struct channel {
    char uri[1024];
    char number[1024];
    char name[1024];
};
