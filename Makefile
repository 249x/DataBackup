# ============================================================
#  一份构建脚本，两个目标：
#
#    make            控制台版 app.exe     （老习惯不变）
#    make gui        图形版   app-gui.exe （需要 Qt6）
#    make both       两个一起
#    make run        跑控制台版
#    make run-gui    跑图形版
#    make clean      清掉两个产物与全部中间文件
#
#  两个前端入口在根目录（mainConsole.cpp / mainGUI.cpp），界面在 UI/，其余全部
#  内核源码都在 Core/ 下。内核只编一遍，两个前端共用同一批 .o。
#
#  build/ 下的目标文件与源码目录完全一一对应，比如：
#      Core/Managers/Filter/NameFilter.cpp  →  build/Core/Managers/Filter/NameFilter.o
#
#  Qt6 没装时 make / make run / make clean 照常可用，只有 make gui / make both
#  会打印安装命令并以非零退出。
#
#  需要 sh 与 find（MSYS2 的 usr/bin 在 PATH 里），与 MSYS2 自带的 mingw32-make 配套。
#  提示信息都用 make 自己的 $(info) 打印：走 sh 的 echo 在管道/重定向下会被 MSYS2
#  按控制台代码页改写，中文会变成乱码。
# ============================================================

CXX   ?= g++
SHELL := /bin/sh
FIND  := /usr/bin/find

# 内核与前端共用这一套开关
# 要用 gdb 时在命令行临时补上即可：make FLAGS="-std=c++17 -Wall -O2 -g -I."
FLAGS = -std=c++17 -Wall -O2 -I.

# ===== Qt6：探测不到就不提供图形版目标 =====
QT_CFLAGS := $(shell pkg-config --cflags Qt6Widgets 2>/dev/null)
QT_LIBS   := $(shell pkg-config --libs Qt6Widgets 2>/dev/null)
HAVE_QT   := $(if $(strip $(QT_CFLAGS)),1,)

# ===== 内核源码：只此一份清单 =====
# 内核源码全在 Core/ 下，目录层级保持原样；加源文件只维护这一份清单
CORE_SRCS = \
    $(wildcard Core/*.cpp) \
    $(wildcard Core/FileStruct/*.cpp) \
    $(wildcard Core/General/*.cpp) \
    $(wildcard Core/Managers/*.cpp) \
    $(wildcard Core/Managers/Archive/*.cpp) \
    $(wildcard Core/Managers/Command/*.cpp) \
    $(wildcard Core/Managers/Compression/*.cpp) \
    $(wildcard Core/Managers/Compression/Handler/*.cpp) \
    $(wildcard Core/Managers/Encryption/*.cpp) \
    $(wildcard Core/Managers/Encryption/Handler/*.cpp) \
    $(wildcard Core/Managers/Filter/*.cpp) \
    $(wildcard Core/Managers/Filter/Filters/*.cpp) \
    $(wildcard Core/Managers/FileIO/*.cpp) \
    $(wildcard Core/Managers/FileIO/IO/*.cpp) \
    $(wildcard Core/Managers/FileIO/IO/Content/*.cpp) \
    $(wildcard Core/Managers/FileIO/IO/Meta/*.cpp) \
    $(wildcard Core/Managers/Pipeline/*.cpp) \
    $(wildcard Core/Managers/Save/*.cpp) \
    $(wildcard Core/Managers/TestCase/*.cpp)

CONSOLE_SRCS = mainConsole.cpp
GUI_SRCS     = mainGUI.cpp $(wildcard UI/*.cpp)

# 目标文件路径 = 源码路径换个后缀
CORE_OBJS    = $(patsubst %.cpp,build/%.o,$(CORE_SRCS))
CONSOLE_OBJS = $(patsubst %.cpp,build/%.o,$(CONSOLE_SRCS))
GUI_OBJS     = $(patsubst %.cpp,build/%.o,$(GUI_SRCS))
ALL_OBJS     = $(CORE_OBJS) $(CONSOLE_OBJS) $(GUI_OBJS)

# Qt 的开关只加在图形版的目标文件上（入口 mainGUI.o 与 UI/*.o）
$(GUI_OBJS): QT_FLAGS = $(QT_CFLAGS)

CONSOLE_BIN = app.exe
GUI_BIN     = app-gui.exe

LIBS = -lbcrypt

# ===== 目标 =====
all: $(CONSOLE_BIN)

$(CONSOLE_BIN): $(CORE_OBJS) $(CONSOLE_OBJS)
	$(CXX) $^ -o $@ $(LIBS)

ifeq ($(HAVE_QT),)
gui:
	$(info 找不到 Qt6：pkg-config --cflags Qt6Widgets 没有输出。)
	$(info MSYS2 里装：pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-pkgconf)
	@exit 1
else
gui: $(GUI_BIN)

$(GUI_BIN): $(CORE_OBJS) $(GUI_OBJS)
	$(CXX) $^ -o $@ $(QT_LIBS) $(LIBS)
endif

both: all gui

run: $(CONSOLE_BIN)
	./$(CONSOLE_BIN)

run-gui: gui
	./$(GUI_BIN)

# ===== 编译规则：目标路径跟源码路径一一对应 =====
build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(FLAGS) $(QT_FLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf build $(CONSOLE_BIN) $(GUI_BIN)
	$(FIND) . -name '*.o' -delete
	$(FIND) . -name '*.d' -delete

-include $(ALL_OBJS:.o=.d)

.PHONY: all gui both run run-gui clean
