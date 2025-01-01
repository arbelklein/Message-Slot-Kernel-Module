#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h> // included for all kernel modules
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h> // included for __init and __exit macros

#include "message_slot.h"

#define err_msg(msg) \
    printk(KERN_ERR "message_slot: %s, line: %d\n", msg, __LINE__);

MODULE_LICENSE("GPL");

static message_slot devices[MAX_SLOTS]; // This will holds all the slots s.t devices[minor] = slot

struct chardev_info
{
    spinlock_t lock;
};

static struct chardev_info device_info;

static int device_open(struct inode *inode, struct file *file)
{
    int minor;
    minor = iminor(inode); // Finding the minor of the inode

    // Will save the address of the slot of the opened file in the private_data of the file
    // If there is no slot for the opened file, then devices[minor] will hold an empty slot (slot that wasn't initialize)
    file->private_data = &devices[minor];
    return 0;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
    message_slot *slot = file->private_data;
    channel *ch = (*slot).channels;
    unsigned int channel_id = (unsigned int)ioctl_param;

    if (ioctl_num != MSG_SLOT_CHANNEL)
    {
        err_msg("Invalid ioctl control command");
        return -EINVAL;
    }

    // channel id must be a non-zero (and since it's unsigned it must be > 0)
    if(channel_id == 0)
    {
        err_msg("Invalid ioctl command param (invalid channel id)");
        return -EINVAL;
    }

    // Reach the maximum number of channels in the slot
    if (slot->num_channels >= MAX_CHANNELS)
    {
        err_msg("Reached maximum channels");
        return -1;
    }

    // Going through the channels of the slot and searching the desired channel
    while (ch != NULL && ch->id != channel_id)
    {
        ch = ch->next;
    }

    if (ch == NULL)
    {
        ch = (channel *)kmalloc(sizeof(channel), GFP_KERNEL);
        if (ch == NULL) // The kalloc failed
        {
            err_msg("kmalloc failed");
            return -ENOMEM;
        }

        ch->id = channel_id;
        ch->message_len = 0;
        ch->next = slot->channels;
        slot->channels = ch;
        slot->num_channels++;
    }

    // Setting the current channel
    slot->current_channel = ch;

    // get_channel did the work of changing the current channel to given channel_id
    return 0;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset)
{
    message_slot *slot;
    channel *ch;
    char write_buffer[BUF_LEN];
    int i;

    // There isn't a slot set to the fd
    if (file->private_data == NULL)
    {
        err_msg("Uninitialized slot for the fd");
        return -EINVAL;
    }

    slot = file->private_data;

    // There isn't a channel set to the fd
    if (slot->current_channel == NULL)
    {
        err_msg("Uninitialized channel for the slot");
        return -EINVAL;
    }

    ch = slot->current_channel; // slot->current_channel is not NULL

    // buffer length is inavlid
    if (length == 0 || length > BUF_LEN)
    {
        err_msg("Invalid buffer length for write");
        return -EMSGSIZE;
    }

    // buffer is inavlid
    if (buffer == NULL)
    {
        err_msg("Invalid buffer for write");
        return -EINVAL;
    }

    // Reading the message from user into write_buffer
    // Writing to write_buffer so the writing will be atomic
    for (i = 0; i < length; i++)
    {
        if (get_user(write_buffer[i], &buffer[i]) != 0) // get_user falied
        {
            err_msg("get_user failed");
            return -EFAULT;
        }
    }

    // Failed writing entire message
    if (i != length)
    {
        err_msg("Failed writing whole message");
        return -EAGAIN; // Try again error code
    }

    // Updating the channel
    memcpy(ch->message, write_buffer, sizeof(ch->message));
    ch->message_len = i;

    return i;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    message_slot *slot;
    channel *ch;
    char __user read_buffer[BUF_LEN];
    int i;

    // There isn't a slot set to the fd
    if (file->private_data == NULL)
    {
        err_msg("Uninitialized slot for the fd");
        return -EINVAL;
    }

    slot = file->private_data;

    // There isn't a channel set to the fd
    if ((slot->current_channel) == NULL)
    {
        err_msg("Uninitialized channel for the slot");
        return -EINVAL;
    }

    ch = slot->current_channel; // slot->current_channel is not NULL

    // There is no message in the channel
    if (ch->message_len == 0)
    {
        err_msg("Trying to read an empty message");
        return -EWOULDBLOCK;
    }

    // The buffer length is too small to hold the message
    if (ch->message_len > length)
    {
        err_msg("Trying to read a long message");
        return -ENOSPC;
    }

    //  Copying the buffer to the read_buffer, for supporting atomic read
    memcpy(read_buffer, buffer, length);

    // Reading the message from channel into read_buffer
    // Writing to read_buffer so the reading will be atomic
    for (i = 0; i < ch->message_len; i++)
    {
        if (put_user((ch->message)[i], &buffer[i]) != 0) // put_user falied
        {
            // The read failed, so returning buffer to it's initialize state by copying read_buffer to buffer
            memcpy(buffer, read_buffer, length);
            err_msg("put_user failed");
            return -EFAULT;
        }
    }

    // Failed reading entire message
    if (i != ch->message_len)
    {
        // The read failed, so returning buffer to it's initialize state by copying read_buffer to buffer
        memcpy(buffer, read_buffer, length);
        err_msg("Failed readig a whole message");
        return -EAGAIN; // Try again error code
    }

    return i;
}

static int device_release(struct inode *inode, struct file *file)
{
    message_slot *slot = file->private_data;

    slot->current_channel = NULL; // Reset the current channel

    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .write = device_write,
    .read = device_read,
    .release = device_release,
};

static int __init message_slot_init(void)
{
    int i, result;
    message_slot slot;

    memset(&device_info, 0, sizeof(struct chardev_info));
    spin_lock_init(&device_info.lock);

    result = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops);
    if (result < 0)
    {
        err_msg("Cannot register device");
        return result;
    }

    // Initializing the slots
    for (i = 0; i < MAX_SLOTS; i++)
    {
        slot = devices[i];
        slot.channels = NULL;
        slot.num_channels = 0;
        slot.current_channel = NULL;
    }

    return 0;
}

static void __exit message_slot_cleanup(void)
{
    int i;

    // Going through the slots to free the channels
    for (i = 0; i < MAX_SLOTS; i++)
    {
        channel *tmp;
        channel *head = devices[i].channels;

        // Freeing all the channels
        while (head != NULL)
        {
            tmp = head;
            head = head->next;
            kfree(tmp);
        }
    }

    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(message_slot_init);
module_exit(message_slot_cleanup);