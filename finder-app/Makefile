TARGET_EXEC := writer

BUILD_DIR := .
SRC_DIR := .
DST_DIR := .

CC := $(CROSS_COMPILE)gcc

.PHONY: all install clean

all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): %: %.o
	mkdir -p $(dir $@)
	$(CC) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET_EXEC).o: $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install:
	mkdir -p $(DST_DIR)
	install $(BUILD_DIR)/$(TARGET_EXEC) $(DST_DIR)/$(TARGET_EXEC)

clean:
		rm -f $(BUILD_DIR)/$(TARGET_EXEC).o
		rm -f $(BUILD_DIR)/$(TARGET_EXEC)

