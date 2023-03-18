BUILD := build
CC := g++-10
CC_FLAGS := -std=c++20 -Wall -Werror

OBJS := test
SRC_FILES := test.cpp

ifneq ($(BUILD), $(wildcard $(BUILD)))
    $(shell mkdir $(BUILD))
endif

all: $(OBJS)

$(OBJS): $(SRC_FILES)
	$(CC) $(CC_FLAGS) $(SRC_FILES) -o $(BUILD)/$(OBJS)
