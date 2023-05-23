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
#include "aesdchar.h"

#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define TERMINATOR_CHARACTER '\n'

int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Jesús María Gómez Moreno");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static ssize_t aesd_find_char(const char *buffer, size_t count, char character);

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

int
aesd_open(struct inode *inode, struct file *filp)
{
  struct aesd_dev *dev = NULL;

  PDEBUG("open");

  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = dev;

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
  struct aesd_dev *dev = filp->private_data;
  struct aesd_buffer_entry *current_entry = NULL;
  size_t current_entry_offset;
  size_t final_count = 0;

  PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

  /**
   * TODO: handle read
   */
  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  current_entry = aesd_circular_buffer_find_entry_offset_for_fpos(
    &dev->buffer,
    *f_pos,
    &current_entry_offset);

  if (current_entry) {
    final_count = current_entry->size - current_entry_offset;
    final_count = count < final_count ? count : final_count;

    TRYZ(
      error = copy_to_user(
        buf,
        current_entry->buffptr + current_entry_offset,
        final_count),
      "error while writing in the user buffer");
  }

  *f_pos += final_count;
  retval = final_count;

done:
  if (retval < 0 && error != 0)
    retval = -EFAULT;

  if (mutex_is_locked(&dev->lock))
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
  size_t final_count = count;
  char *buffptr = NULL;
  char *oldptr = NULL;
  struct aesd_dev *dev = filp->private_data;
  struct aesd_buffer_entry entry;

  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

  if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;

  if (count) {
    TRY(
      dev->unterminated = (char *)krealloc(
        dev->unterminated,
        (dev->unterminated_size + count) * sizeof(char),
        GFP_KERNEL),
      "buffer pointer allocation failed");
    buffptr = dev->unterminated + dev->unterminated_size;

    TRYZ(
      error = copy_from_user(buffptr, buf, count),
      "error while copying from user");

    terminator_position = aesd_find_char(buffptr, count, TERMINATOR_CHARACTER);
    if (terminator_position < 0) {
      final_count = count;
      dev->unterminated_size += count;
    } else {
      final_count = terminator_position + 1;
      entry.buffptr = dev->unterminated;
      entry.size = dev->unterminated_size + final_count;
      oldptr = aesd_circular_buffer_add_entry(&dev->buffer, &entry);
      if (oldptr)
        kfree(oldptr);
      dev->unterminated = NULL;
      dev->unterminated_size = 0;
    }

    retval = final_count;
  }

done:
  if (retval < 0) {
    if (dev->unterminated && dev->unterminated_size == 0) {
      kfree(dev->unterminated);
      dev->unterminated = NULL;
    }

    if (error != 0) {
      retval = -EFAULT;
    }
  }

  if (mutex_is_locked(&dev->lock))
    mutex_unlock(&dev->lock);

  return retval;
}

struct file_operations aesd_fops = {
  .owner = THIS_MODULE,
  .read = aesd_read,
  .write = aesd_write,
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

  if (aesd_device.unterminated)
    kfree(aesd_device.unterminated);
  aesd_device.unterminated = NULL;
  aesd_device.unterminated_size = 0;

  AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.buffer, index)
  {
    if (entryptr->buffptr)
      kfree(entryptr->buffptr);
    entryptr->buffptr = NULL;
    entryptr->size = 0;
  };

  unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
