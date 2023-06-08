BUILD_DIR := build
CC 			:= g++
LINK_FLAGS 	:= -pthread
CC_FLAGS 	:= -std=c++20 -Wall -Werror

ifneq ($(BUILD_DIR), $(wildcard $(BUILD_DIR)))
    $(shell mkdir $(BUILD_DIR))
endif

# cpp utils binary
OBJS := LogImpl.o
SRC_FILES := LogImpl.cpp

all: $(OBJS)
	$(CC) $(CC_FLAGS) $(LINK_FLAGS) $(SRC_FILES) -c -o $(BUILD_DIR)/$(OBJS)

# For test
TEST_OBJS := test
TSET_SRC_FILES := test.cpp

test: $(BUILD_DIR)/$(OBJS)

$(TEST_OBJS): $(TSET_SRC_FILES)
	$(CC) $(CC_FLAGS) $(LINK_FLAGS) $(TSET_SRC_FILES) $(BUILD_DIR)/$(OBJS) -o $(BUILD_DIR)/$(TEST_OBJS)
