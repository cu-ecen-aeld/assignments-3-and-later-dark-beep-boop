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
#endif

#include "aesd-circular-buffer.h"

static size_t aesd_circular_buffer_capacity(
  const struct aesd_circular_buffer *buffer);
static size_t aesd_circular_buffer_relative_position(
  const struct aesd_circular_buffer *buffer,
  size_t position,
  bool first);
static size_t aesd_circular_buffer_next_entry_offset(
  const struct aesd_circular_buffer *buffer,
  size_t entry_offset);
static ssize_t aesd_circular_buffer_substract_byte_offset_from_fpos(
  struct aesd_buffer_entry *entryptr,
  size_t char_offset);
static struct aesd_buffer_entry *aesd_circular_buffer_check_entry_and_iterate(
  struct aesd_circular_buffer *buffer,
  size_t *entry_offset,
  size_t *char_offset);

static size_t
aesd_circular_buffer_capacity(const struct aesd_circular_buffer *buffer)
{
  return sizeof(buffer->entry) / sizeof(struct aesd_buffer_entry);
}

static size_t
aesd_circular_buffer_relative_position(
  const struct aesd_circular_buffer *buffer,
  size_t position,
  bool first)
{
  size_t result;

  if (buffer->full && !first && position == buffer->in_offs)
    result = aesd_circular_buffer_capacity(buffer);
  else
    result =
      (position - buffer->out_offs) % aesd_circular_buffer_capacity(buffer);

  return result;
}

size_t
aesd_circular_buffer_next_entry_offset(
  const struct aesd_circular_buffer *buffer,
  size_t entry_offset)
{
  return (entry_offset + 1) % aesd_circular_buffer_capacity(buffer);
}

ssize_t
aesd_circular_buffer_substract_byte_offset_from_fpos(
  struct aesd_buffer_entry *entryptr,
  size_t char_offset)
{
  ssize_t result = -1;

  if (char_offset >= entryptr->size)
    result = char_offset - entryptr->size;

  return result;
}

struct aesd_buffer_entry *
aesd_circular_buffer_check_entry_and_iterate(
  struct aesd_circular_buffer *buffer,
  size_t *entry_position,
  size_t *char_offset)
{
  struct aesd_buffer_entry *result = NULL;
  ssize_t substraction = 0;

  substraction = aesd_circular_buffer_substract_byte_offset_from_fpos(
    buffer->entry + *entry_position,
    *char_offset);
  if (substraction < 0) {
    result = buffer->entry + *entry_position;
  } else {
    *char_offset = substraction;
    *entry_position =
      aesd_circular_buffer_next_entry_offset(buffer, *entry_position);
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
  size_t entry_position = buffer->out_offs;
  size_t last_position;
  struct aesd_buffer_entry *result = NULL;

  last_position =
    aesd_circular_buffer_relative_position(buffer, buffer->in_offs, false);

  if (
    aesd_circular_buffer_relative_position(buffer, entry_position, true) <
    last_position)
    result = aesd_circular_buffer_check_entry_and_iterate(
      buffer,
      &entry_position,
      &remaining);

  while (!result &&
         aesd_circular_buffer_relative_position(buffer, entry_position, false) <
           last_position)
    result = aesd_circular_buffer_check_entry_and_iterate(
      buffer,
      &entry_position,
      &remaining);

  if (result)
    *entry_offset_byte_rtn = remaining;

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
void
aesd_circular_buffer_add_entry(
  struct aesd_circular_buffer *buffer,
  const struct aesd_buffer_entry *add_entry)
{
  buffer->entry[buffer->in_offs] = *add_entry;
  buffer->in_offs =
    aesd_circular_buffer_next_entry_offset(buffer, buffer->in_offs);

  if (buffer->full)
    buffer->out_offs = buffer->in_offs;
  else if (buffer->in_offs == buffer->out_offs)
    buffer->full = true;
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
