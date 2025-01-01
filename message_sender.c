#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "message_slot.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file> <channel> <message>\n", argv[0]);
        return 1;
    }

    // Opening the message slot file 
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("Error opening file");
        return 1;
    }

    // Setting the channel of the current message slot
    unsigned int channel = atoi(argv[2]);
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel) < 0) {
        perror("Error setting channel");
        close(fd);
        return 1;
    }

    // Writing to the channel in the current message slot
    if (write(fd, argv[3], strlen(argv[3])) < 0) {
        perror("Error writing message");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}