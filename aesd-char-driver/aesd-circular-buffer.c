/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#endif

#include "aesd-circular-buffer.h"

static inline uint8_t aesd_circular_buffer_entry_array_capacity(
  const struct aesd_circular_buffer *buffer);
static inline uint8_t aesd_circular_buffer_relative_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset);
static inline uint8_t aesd_circular_buffer_absolute_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset);
static inline uint8_t aesd_circular_buffer_entry_array_size(
  const struct aesd_circular_buffer *buffer);
static inline uint8_t aesd_circular_buffer_next_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset);
static inline ssize_t aesd_circular_buffer_substract_entry_from_fpos(
  struct aesd_buffer_entry *entryptr,
  size_t char_offset);
static struct aesd_buffer_entry *
aesd_circular_buffer_iterate_find_entry_offset_for_fpos(
  struct aesd_circular_buffer *buffer,
  uint8_t *entry_offset,
  size_t *char_offset);

uint8_t
aesd_circular_buffer_entry_array_capacity(
  const struct aesd_circular_buffer *buffer)
{
  return sizeof(buffer->entry) / sizeof(struct aesd_buffer_entry);
}

uint8_t
aesd_circular_buffer_relative_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset)
{
  uint8_t result;
  uint8_t adjusted_entry_offset =
    entry_offset % aesd_circular_buffer_entry_array_capacity(buffer);

  if (buffer->full && adjusted_entry_offset == buffer->in_offs)
    result = aesd_circular_buffer_entry_array_capacity(buffer);
  else if (adjusted_entry_offset >= buffer->out_offs)
    result = adjusted_entry_offset - buffer->out_offs;
  else
    result = adjusted_entry_offset +
             aesd_circular_buffer_entry_array_capacity(buffer) -
             buffer->out_offs;

  return result;
}

uint8_t
aesd_circular_buffer_absolute_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset)
{
  return (buffer->out_offs + entry_offset) %
         aesd_circular_buffer_entry_array_capacity(buffer);
}

uint8_t
aesd_circular_buffer_entry_array_size(const struct aesd_circular_buffer *buffer)
{
  uint8_t result;

  if (buffer->full)
    result = aesd_circular_buffer_entry_array_capacity(buffer);
  else
    result = (buffer->in_offs - buffer->out_offs) %
             aesd_circular_buffer_entry_array_capacity(buffer);

  return result;
}

uint8_t
aesd_circular_buffer_next_entry_offset(
  const struct aesd_circular_buffer *buffer,
  uint8_t entry_offset)
{
  return (entry_offset + 1) % aesd_circular_buffer_entry_array_capacity(buffer);
}

ssize_t
aesd_circular_buffer_substract_entry_from_fpos(
  struct aesd_buffer_entry *entryptr,
  size_t char_offset)
{
  ssize_t result = -1;

  if (char_offset >= entryptr->size)
    result = char_offset - entryptr->size;

  return result;
}

struct aesd_buffer_entry *
aesd_circular_buffer_iterate_find_entry_offset_for_fpos(
  struct aesd_circular_buffer *buffer,
  uint8_t *entry_offset,
  size_t *char_offset)
{
  struct aesd_buffer_entry *result = NULL;
  ssize_t substraction = 0;

  substraction = aesd_circular_buffer_substract_entry_from_fpos(
    buffer->entry + *entry_offset,
    *char_offset);
  if (substraction < 0) {
    result = buffer->entry + *entry_offset;
  } else {
    *char_offset = substraction;
    *entry_offset =
      aesd_circular_buffer_next_entry_offset(buffer, *entry_offset);
  }

  return result;
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary
 * locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing
 * the zero referenced character index if all buffer strings were concatenated
 * end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the
 * byte of the returned aesd_buffer_entry buffptr member corresponding to
 * char_offset. This value is only set when a matching char_offset is found in
 * aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position
 * described by char_offset, or NULL if this position is not available in the
 * buffer (not enough data is written).
 */
struct aesd_buffer_entry *
aesd_circular_buffer_find_entry_offset_for_fpos(
  struct aesd_circular_buffer *buffer,
  size_t char_offset,
  size_t *entry_offset_byte_rtn)
{
  size_t remaining = char_offset;
  uint8_t entry_offset = buffer->out_offs;
  struct aesd_buffer_entry *result = NULL;

  if (0 < aesd_circular_buffer_entry_array_size(buffer)) {
    result = aesd_circular_buffer_iterate_find_entry_offset_for_fpos(
      buffer,
      &entry_offset,
      &remaining);
  }

  while (!result &&
         aesd_circular_buffer_relative_entry_offset(buffer, entry_offset) <
           aesd_circular_buffer_entry_array_size(buffer)) {
    result = aesd_circular_buffer_iterate_find_entry_offset_for_fpos(
      buffer,
      &entry_offset,
      &remaining);
  }

  if (result)
    *entry_offset_byte_rtn = remaining;

  return result;
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary
 * locking must be performed by caller.
 * @param entry_offset the entry position to search for in the buffer list,
 * describing the zero referenced entry index.
 * @param entry_offset_byte is the byte offset within the entry.
 * @return the zero referenced offset corresponding to the specified position as
 * if the strings stored in the buffer were concatenated end to end.
 */
ssize_t
aesd_circular_buffer_find_fpos_for_entry_offset(
  struct aesd_circular_buffer *buffer,
  uint8_t entry_offset,
  size_t entry_offset_byte)
{
  uint8_t iter;
  ssize_t result = -1;

  if (
    entry_offset < aesd_circular_buffer_entry_array_size(buffer) &&
    entry_offset_byte < buffer
                          ->entry[aesd_circular_buffer_absolute_entry_offset(
                            buffer,
                            entry_offset)]
                          .size) {
    iter = buffer->out_offs;
    result = 0;

    if (0 < entry_offset) {
      result += buffer->entry[iter].size;
      iter = aesd_circular_buffer_next_entry_offset(buffer, iter);
    }

    while (aesd_circular_buffer_relative_entry_offset(buffer, iter) <
           entry_offset) {
      result += buffer->entry[iter].size;
      iter = aesd_circular_buffer_next_entry_offset(buffer, iter);
    }

    result += entry_offset_byte;
  }

  return result;
}

/**
 * Adds entry @param add_entry to @param buffer in the location specified in
 * buffer->in_offs. If the buffer was already full, overwrites the oldest
 * entry and advances buffer->out_offs to the new start location. Any
 * necessary locking must be handled by the caller Any memory referenced in
 * @param add_entry must be allocated by and/or must have a lifetime managed
 * by the caller.
 */
const char *
aesd_circular_buffer_add_entry(
  struct aesd_circular_buffer *buffer,
  const struct aesd_buffer_entry *add_entry)
{
  const char *old = NULL;
  if (buffer->full)
    old = buffer->entry[buffer->in_offs].buffptr;

  buffer->entry[buffer->in_offs] = *add_entry;
  buffer->in_offs =
    aesd_circular_buffer_next_entry_offset(buffer, buffer->in_offs);

  if (buffer->full)
    buffer->out_offs = buffer->in_offs;
  else if (buffer->in_offs == buffer->out_offs)
    buffer->full = true;

  return old;
}

/**
 * @return the current total size of the stored data
 */
size_t
aesd_circular_buffer_size(const struct aesd_circular_buffer *buffer)
{
  const struct aesd_buffer_entry *entryptr = NULL;
  uint8_t index;
  size_t size = 0;

  AESD_CIRCULAR_BUFFER_FOREACH(entryptr, buffer, index)
  {
    if (entryptr->buffptr) {
      size += entryptr->size;
    }
  }

  return size;
}

/**
 * Initializes the circular buffer described by @param buffer to an empty
 * struct
 */
void
aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
  memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}
