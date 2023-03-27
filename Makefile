TARGET_EXEC := writer

BUILD_DIR := ./finder-app
SRC_DIR := ./finder-app

CC := $(CROSS_COMPILE)gcc

$(BUILD_DIR)/$(TARGET_EXEC): %: %.o
	mkdir -p $(dir $@)
	$(CC) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET_EXEC).o: $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm $(BUILD_DIR)/$(TARGET_EXEC).o $(BUILD_DIR)/$(TARGET_EXEC)
