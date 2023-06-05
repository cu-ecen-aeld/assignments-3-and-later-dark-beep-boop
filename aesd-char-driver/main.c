/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"
#include "aesdchar.h"

#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define TERMINATOR_CHARACTER '\n'
#define MSG_MAX_LEN 100

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Jesús María Gómez Moreno");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static ssize_t aesd_find_char(const char *buffer, size_t count, char character);
static long aesd_iocseekto(
  struct file *filp,
  uint32_t write_cmd,
  uint32_t wirte_cmd_offset);

ssize_t
aesd_find_char(const char *buffer, size_t count, char character)
{
  ssize_t result = -1;
  size_t i = 0;

  while (i < count && buffer[i] != character)
    ++i;

  if (i < count)
    result = i;

  return result;
}

static long
aesd_iocseekto(struct file *filp, uint32_t write_cmd, uint32_t write_cmd_offset)
{
  long result = -EINVAL;
  struct aesd_dev *dev = filp->private_data;
  ssize_t f_pos;

  PDEBUG(
    "seeking write command %u with offset %u",
    write_cmd,
    write_cmd_offset);

  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  f_pos = aesd_circular_buffer_find_fpos_for_entry_offset(
    &dev->buffer,
    write_cmd,
    write_cmd_offset);

  if (f_pos >= 0) {
    result = fixed_size_llseek(
      filp,
      f_pos,
      SEEK_SET,
      aesd_circular_buffer_size(&dev->buffer));
  }

  mutex_unlock(&dev->lock);

  return result;
}

int
aesd_open(struct inode *inode, struct file *filp)
{
  PDEBUG("open");

  filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);

  return 0;
}

int
aesd_release(struct inode *inode, struct file *filp)
{
  PDEBUG("release");

  return 0;
}

ssize_t
aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  ssize_t retval = -1;
  ssize_t error = 0;
  ssize_t remaining = 0;
  struct aesd_dev *dev = filp->private_data;
  struct aesd_buffer_entry *current_entry = NULL;
  struct aesd_buffer_entry tmp_entry;
  size_t current_entry_byte;
  size_t final_count = 0;
  char msg_buffer[MSG_MAX_LEN] = {};

  PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  current_entry = aesd_circular_buffer_find_entry_offset_for_fpos(
    &dev->buffer,
    *f_pos,
    &current_entry_byte);

  if (
    !current_entry &&
    ((remaining = *f_pos - aesd_circular_buffer_size(&dev->buffer)) <
     dev->unterminated_size)) {
    current_entry_byte = remaining;
    tmp_entry.buffptr = dev->unterminated_buffptr;
    tmp_entry.size = dev->unterminated_size;
    current_entry = &tmp_entry;
  }

  if (current_entry) {
    final_count = current_entry->size - current_entry_byte;
    final_count = count < final_count ? count : final_count;

    strncpy(
      msg_buffer,
      current_entry->buffptr + current_entry_byte,
      final_count);
    PDEBUG("writing %s", msg_buffer);

    TRYZ(
      error = copy_to_user(
        buf,
        current_entry->buffptr + current_entry_byte,
        final_count),
      "error while writing in the user buffer");
  }

  *f_pos += final_count;
  retval = final_count;

done:
  if (retval < 0 && error != 0)
    retval = -EFAULT;

  mutex_unlock(&dev->lock);

  return retval;
}

ssize_t
aesd_write(
  struct file *filp,
  const char __user *buf,
  size_t count,
  loff_t *f_pos)
{
  ssize_t retval = -ENOMEM;
  ssize_t error = 0;
  ssize_t terminator_position = 0;
  size_t final_count = 0;
  char *buffptr = NULL;
  const char *oldptr = NULL;
  struct aesd_dev *dev = filp->private_data;
  struct aesd_buffer_entry entry;
  char msg_buffer[MSG_MAX_LEN] = {};

  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  if (count) {
    TRY(
      dev->unterminated_buffptr = (char *)krealloc(
        dev->unterminated_buffptr,
        (dev->unterminated_size + count) * sizeof(char),
        GFP_KERNEL),
      "buffer pointer allocation failed");
    buffptr = dev->unterminated_buffptr + dev->unterminated_size;

    TRYZ(
      error = copy_from_user(buffptr, buf, count),
      "error while copying from user");

    strncpy(msg_buffer, buffptr, count);
    PDEBUG("writing %s", msg_buffer);

    terminator_position = aesd_find_char(buffptr, count, TERMINATOR_CHARACTER);
    if (terminator_position < 0) {
      final_count = count;
      dev->unterminated_size += final_count;
    } else {
      final_count = terminator_position + 1;
      entry.buffptr = dev->unterminated_buffptr;
      entry.size = dev->unterminated_size + final_count;
      oldptr = aesd_circular_buffer_add_entry(&dev->buffer, &entry);
      if (oldptr)
        kfree(oldptr);
      dev->unterminated_buffptr = NULL;
      dev->unterminated_size = 0;
    }

    *f_pos += final_count;
    retval = final_count;
  }

done:

  if (retval < 0) {
    if (dev->unterminated_buffptr && dev->unterminated_size == 0) {
      kfree(dev->unterminated_buffptr);
      dev->unterminated_buffptr = NULL;
    }

    if (error != 0) {
      retval = -EFAULT;
    }
  }

  mutex_unlock(&dev->lock);

  return retval;
}

loff_t
aesd_llseek(struct file *filp, loff_t offset, int whence)
{
  struct aesd_dev *dev = filp->private_data;
  loff_t retval = 0;

  PDEBUG("ENTERING LLSEEK");
  PDEBUG("seeking offset %llu with whence %d\n", offset, whence);

  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  retval = fixed_size_llseek(
    filp,
    offset,
    whence,
    aesd_circular_buffer_size(&dev->buffer));

  mutex_unlock(&dev->lock);
  PDEBUG("new file offset: %llu\n", retval);
  PDEBUG("EXITING LLSEEK");

  return retval;
}

long
aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
  uint64_t local_64_arg;
  long retval;

  switch (cmd) {
    case AESDCHAR_IOCSEEKTO:
      PDEBUG("Executing ioctl AESDCHAR_IOCSEEKTO");
      if (!get_user(local_64_arg, (uint64_t *)arg)) {
        retval = aesd_iocseekto(
          filp,
          ((struct aesd_seekto *)&local_64_arg)->write_cmd,
          ((struct aesd_seekto *)&local_64_arg)->write_cmd_offset);
      } else {
        retval = -EFAULT;
      }
      break;
    default:
      retval = -ENOTTY;
  }

  return retval;
}

struct file_operations aesd_fops = {
  .owner = THIS_MODULE,
  .llseek = aesd_llseek,
  .read = aesd_read,
  .write = aesd_write,
  .unlocked_ioctl = aesd_ioctl,
  .open = aesd_open,
  .release = aesd_release,
};

static int
aesd_setup_cdev(struct aesd_dev *dev)
{
  int err, devno = MKDEV(aesd_major, aesd_minor);

  cdev_init(&dev->cdev, &aesd_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &aesd_fops;
  TRYC(err = cdev_add(&dev->cdev, devno, 1), "couldn't add character device");

done:
  return err;
}

int
aesd_init_module(void)
{
  dev_t dev = 0;
  int result;
  int ok = -1;

  TRYC(
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar"),
    "character device major number allocation failed");
  aesd_major = MAJOR(dev);

  memset(&aesd_device, 0, sizeof(struct aesd_dev));

  mutex_init(&aesd_device.lock);
  aesd_circular_buffer_init(&aesd_device.buffer);

  TRYC(result = aesd_setup_cdev(&aesd_device), "character device setup failed");

  ok = result;

done:
  if (ok < 0) {
    if (dev) {
      unregister_chrdev_region(dev, 1);
    }

    if (result < 0) {
      ok = result;
    }
  }

  return ok;
}

void
aesd_cleanup_module(void)
{
  dev_t devno = MKDEV(aesd_major, aesd_minor);
  struct aesd_buffer_entry *entryptr = NULL;
  uint8_t index = 0;

  cdev_del(&aesd_device.cdev);

  if (aesd_device.unterminated_buffptr)
    kfree(aesd_device.unterminated_buffptr);

  AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.buffer, index)
  {
    if (entryptr->buffptr) {
      kfree(entryptr->buffptr);
    }
  };

  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
