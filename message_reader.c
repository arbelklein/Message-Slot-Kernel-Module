#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <channel>\n", argv[0]);
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

    // Reading from the channel in the current message slot
    char buffer[BUF_LEN];
    ssize_t message_len = read(fd, buffer, BUF_LEN);
    if (message_len < 0) {
        perror("Error reading message");
        close(fd);
        return 1;
    }

    // Writing the message to the stdout
    if (write(STDOUT_FILENO, buffer, message_len) < 0) {
        perror("Error printing message");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}