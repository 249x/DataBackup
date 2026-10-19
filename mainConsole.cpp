// 控制台版入口
//
//   app.exe                  → 交互式命令循环
//   app.exe <命令文件>        → 等价于敲一条 run "<命令文件>"
//
#include "Core/System.h"

int main(int argc, char* argv[]) {
    System system;
    return system.Run(argc, argv);
}