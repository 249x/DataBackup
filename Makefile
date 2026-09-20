# ===== 编译器 =====
CXX ?= g++

CXXFLAGS = -std=c++17 -Wall -g -I.

LDFLAGS = -lbcrypt

SHELL := /bin/sh
FIND  := /usr/bin/find

# ===== 源文件 =====
SRCS = \
    $(wildcard *.cpp) \
    $(wildcard FileStruct/*.cpp) \
    $(wildcard General/*.cpp) \
    $(wildcard Managers/*.cpp) \
    $(wildcard Managers/Archive/*.cpp) \
    $(wildcard Managers/Command/*.cpp) \
    $(wildcard Managers/Compression/*.cpp) \
    $(wildcard Managers/Compression/Handler/*.cpp) \
    $(wildcard Managers/Encryption/*.cpp) \
    $(wildcard Managers/Encryption/Handler/*.cpp) \
    $(wildcard Managers/FileIO/*.cpp) \
    $(wildcard Managers/FileIO/IO/*.cpp) \
    $(wildcard Managers/FileIO/IO/Content/*.cpp) \
    $(wildcard Managers/FileIO/IO/Meta/*.cpp) \
    $(wildcard Managers/Pipeline/*.cpp) \
    $(wildcard Managers/TestCase/*.cpp)

# ===== 构建目录 =====
BUILD_DIR = build

OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

TARGET = app.exe

# ===== 默认目标 =====
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf build app.exe
	$(FIND) . -name '*.o' -delete
	$(FIND) . -name '*.d' -delete

-include $(DEPS)

.PHONY: all clean