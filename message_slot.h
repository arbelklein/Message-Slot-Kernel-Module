#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#define MAJOR_NUM 235
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int) // Creates a unique ioctl command code for the kernel to recognize
#define MAX_SLOTS 256
#define MAX_CHANNELS 0x100000 // 0 < channels in device <= 2^20 = 0x100000

typedef struct channel{
    unsigned int id;
    char message[BUF_LEN];
    int message_len;
    struct channel *next;
}channel;

typedef struct message_slot{
    channel *channels; // Will hold the open channel in a slot
    unsigned long num_channels; // Number of open channel in a slot
    channel *current_channel; // The channel the slot points to currently
}message_slot;

#endif