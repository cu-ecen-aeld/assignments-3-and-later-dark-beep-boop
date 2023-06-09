/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include "aesd-circular-buffer.h"
#include <linux/cdev.h>
#include <linux/mutex.h>

#define AESD_DEBUG 1 // Remove comment on this line to enable debug

#undef PDEBUG /* undef it, just in case */
#ifdef AESD_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PWARN /* undef it, just in case */
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PWARN(fmt, args...) printk(KERN_WARNING "aesdchar: " fmt, ##args)
#else
/* This one for user space */
#define PWARN(fmt, args...) fprintf(stderr, fmt, ##args)
#endif

#define PRINT_ERROR(message)                                                   \
  PWARN(                                                                       \
    "ERROR (file=%s, line=%d, function=%s): %s\n",                             \
    __FILE__,                                                                  \
    __LINE__,                                                                  \
    __func__,                                                                  \
    message)

#define TRYCATCH(condition, action, message)                                   \
  if (condition) {                                                             \
    PRINT_ERROR(message);                                                      \
    action;                                                                    \
  }                                                                            \
  NULL

#define TRY(expr, message) TRYCATCH(!(expr), goto done, message)

#define TRYC(expr, message) TRYCATCH((expr) < 0, goto done, message)

#define TRYZ(expr, message) TRYCATCH((expr), goto done, message)

struct aesd_dev
{
  struct mutex lock;
  char *unterminated_buffptr;
  size_t unterminated_size;
  struct aesd_circular_buffer buffer;
  struct cdev cdev; /* Char device structure      */
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
