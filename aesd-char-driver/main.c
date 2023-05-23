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

#define TERMINATING_CHARACTER '\n'

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
  /**
   * TODO: handle open
   */

  dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
  filp->private_data = dev;

  return 0;
}

int
aesd_release(struct inode *inode, struct file *filp)
{
  struct aesd_dev *dev = filp->private_data;

  PDEBUG("release");
  /**
   * TODO: handle release
   */

  if (dev->entry_buffptr)
    kfree(dev->entry_buffptr);

  dev->entry_buffptr = NULL;
  dev->entry_size = 0;

  return 0;
}

ssize_t
aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  ssize_t retval = 0;
  PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
  /**
   * TODO: handle read
   */
  return retval;
}

ssize_t
aesd_write(
  struct file *filp,
  const char __user *buf,
  size_t count,
  loff_t *f_pos)
{
  ssize_t ok = -ENOMEM;
  ssize_t result = 0;
  size_t terminating_pos = 0;
  size_t write_count = 0;
  size_t new_entry_size = 0;
  char *buffptr = NULL;
  char *buffcopy = NULL;
  struct aesd_dev *dev = filp->private_data;
  struct aesd_buffer_entry entry;

  PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
  /**
   * TODO: handle write
   */

  TRY(
    buffcopy = (char *)kmalloc(count * sizeof(char), GFP_KERNEL),
    "buffer copy allocation failed");

  TRYZ(
    result = copy_from_user(buffcopy, buf, count),
    "error while copying from user");

  terminating_pos = aesd_find_char(buf, count, TERMINATING_CHARACTER);
  write_count = terminating_pos < 0 ? count : terminating_pos + 1;

  if (write_count > 0) {
    if (mutex_lock_interruptible(&dev->lock))
      return -ERESTARTSYS;

    new_entry_size = dev->entry_size + write_count;

    TRY(
      buffptr = (char *)
        krealloc(dev->entry_buffptr, new_entry_size * sizeof(char), GFP_KERNEL),
      "buffer pointer allocation failed");

    buffptr += dev->entry_size;

    memcpy(buffptr, buffcopy, write_count);

    buffptr -= dev->entry_size;

    if (terminating_pos < 0) {
      dev->entry_buffptr = buffptr;
      dev->entry_size = new_entry_size;
    } else {
      entry.buffptr = buffptr;
      entry.size = new_entry_size;
      aesd_circular_buffer_add_entry(&dev->buffer, &entry);
      dev->entry_buffptr = NULL;
      dev->entry_size = 0;
    }

    f_pos += write_count;
  }

  ok = 0;

done:
  if (ok < 0) {
    if (buffptr && dev->entry_size == 0) {
      kfree(buffptr);
    }

    if (result < 0) {
      ok = result;
    }
  }

  if (mutex_is_locked(&dev->lock))
    mutex_unlock(&dev->lock);

  if (buffcopy)
    kfree(buffcopy);

  return ok;
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

  /**
   * TODO: initialize the AESD specific portion of the device
   */
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

  /**
   * TODO: cleanup AESD specific poritions here as necessary
   */
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
