TARGET_EXEC := aesdsocket

BUILD_DIR := build
DST_DIR := .
INCLUDE_DIRS := include ../aesd-char-driver
SRC_DIR := src
FILES := main aesdsocket node doubly_linked_list queue monitor

OBJ_FILES := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(FILES)))
INCLUDES := $(addprefix -I,$(INCLUDE_DIRS))

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror -O2
LDFLAGS ?= -pthread -lrt

.PHONY: all install clean

all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJ_FILES)
	mkdir -p $(dir $@)
	$(CC) $^ $(LDFLAGS) -o $@

$(OBJ_FILES): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

install:
	mkdir -p $(DST_DIR)
	install $(BUILD_DIR)/$(TARGET_EXEC) $(DST_DIR)/$(TARGET_EXEC)

clean:
		rm -f $(OBJ_FILES)
		rm -f $(BUILD_DIR)/$(TARGET_EXEC)

