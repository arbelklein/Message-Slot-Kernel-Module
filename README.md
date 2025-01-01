# Message-Slot-Kernel-Module

## Description
This repository contains a kernel module that provides a new inter-process communication (IPC) mechanism called a message slot. A message slot is a character device file through which processes can communicate. Each message slot device can have multiple message channels active concurrently, allowing multiple processes to use it. The module supports setting a channel ID using `ioctl()`, and sending/receiving messages using `write()` and `read()` system calls.

## Usage
1. Compile the kernel module:
    ```sh
    make
    ```
2.  Load the kernel module:
    ```sh
    sudo insmod message_slot.ko
    ```
3. Create a message slot device file:
    ```sh
    sudo mknod /dev/slot0 c 235 0
    sudo chmod 666 /dev/slot0
    ```

### Example Session
1. Load the kernel module:
    ```sh
    sudo insmod message_slot.ko
    ```
2. Create a message slot file:
    ```sh
    sudo mknod /dev/slot0 c 235 0
    sudo chmod 666 /dev/slot0
    ```
3. Send a message:
    ```sh
    ./message_sender /dev/slot0 1 "Hello, World!"
    ```
4. Read the message:
    ```sh
    ./message_reader /dev/slot0 1
    Hello, World!
    ```
