# DataBackup —— 数据备份软件

《软件开发综合实验》小组项目。实现目录树的备份与还原，并在其之上提供打包、
压缩、加密、按条件筛选、图形界面等扩展功能。

提供两个前端，共用同一套内核：

| 前端 | 产物 | 说明 |
|---|---|---|
| 命令行 | `app.exe` | 交互式命令循环，适合脚本化和自动化测试 |
| 图形界面 | `app-gui.exe` | Qt6 编写的桌面窗口，四个页签覆盖全部功能 |

---

## 一、直接运行（不需要编译）

解压后进入 `可执行程序` 目录：

* **图形界面**：双击 `图形版\app-gui.exe`
* **命令行**：在 `控制台版` 目录里打开终端，执行 `.\app.exe`

两个目录里已经带齐了所需的运行库（MinGW 运行时、Qt6 及其依赖、Qt 平台插件），
拷到别的 Windows 机器上也能直接运行，**不需要安装 MSYS2 或 Qt**。

> 命令行版如果要用管道喂命令（例如 `run 脚本文件`），命令最后一定要写 `quit`，
> 否则读到输入结束（EOF）后程序会一直刷提示符。

---

## 二、功能一览

### 基础功能

* **数据备份**：把目录树中的文件保存到指定位置，保留相对目录结构
* **数据还原**：把备份内容按原结构写回指定目录

### 扩展功能

| 扩展项 | 实现情况 |
|---|---|
| 文件类型支持 | 普通文件、目录、硬链接、符号链接、目录联接（Junction）；硬链接在还原后仍指向同一份数据 |
| 元数据支持 | 属主、属组、创建/访问/修改时间、只读属性 |
| 打包解包 | 把整棵目录树拼接成单个归档文件 |
| 压缩解压 | Huffman 编码、RLE 游程编码 |
| 加密解密 | AES-256-CBC、DES-CBC，均由用户指定密钥或口令；解密后校验自校验标记，密钥错误会明确报错而不是产出乱码 |
| 自定义备份 | 六类筛选条件：路径 / 类型 / 名字 / 时间 / 尺寸 / 用户，另加 and / or / not 组合 |
| 图形界面 | Qt6 桌面程序，见下文 |

### 处理流水线

备份时顺序固定：

```text
读入目录树 → 应用筛选 → 检查密钥 → 打包 → 压缩 → 加密 → 写出
```

还原时按相反顺序逐层剥开。备份文件里记录了每一层做了什么、用了哪个算法，
所以还原时不需要把压缩/加密算法设回备份时的取值，**只有密钥必须一致**。

---

## 三、命令行命令一览

命令行版是一个交互式循环，提示符就是当前目录，形如 `D:\demo|>`。命令和参数之间
用空格分隔，**参数里带空格就用双引号包住**（如 `cd "D:\my backup"`，引号本身会被
去掉）。输入 `help` 看全部命令，输入 `quit`（或 `exit`）退出。

| 命令 | 参数 | 说明 |
|---|---|---|
| `help` | — | 列出全部命令及其参数类型 |
| `cd` | `<目录>` | 切换当前目录，提示符跟着变 |
| `run` | `<命令文件>` | 逐行执行文件里的命令（空行跳过） |
| `backup` | `<源目录> <备份文件>` | 按当前设置备份 |
| `restore` | `<备份文件> <输出目录>` | 还原，处理层自动识别 |
| `settings` | — | 打印当前的打包 / 压缩 / 加密开关 |
| `set-archive` | `<true\|false>` | 备份时是否打包 |
| `set-compress` | `<true\|false> <算法号>` | 是否压缩、用哪个（1=Huffman，2=RLE） |
| `set-encrypt` | `<true\|false> <算法号>` | 是否加密、用哪个（1=AES，2=DES） |
| `key` | `<密钥文件>` | 读入密钥文件（AES 至少 32 字节，DES 至少 8 字节） |
| `add-filter` | `<规格>` | 添加筛选条件，成功会打印编号 |
| `remove-filter` | `<编号>` | 删除筛选条件 |
| `set-filter` | `<编号> <参数>` | 改某条筛选条件的参数 |
| `filters` | — | 列出已添加的筛选条件 |
| `clear-filters` | — | 清空所有筛选条件（编号不复用） |
| `quit` / `exit` | — | 退出 |

布尔参数认 `true/false`、`1/0`、`yes/no`、`on/off`、`y/n`，不分大小写。
`backup` 与 `restore` **成功时不打印任何东西**，出错才会在日志里留一条 `[ERROR]`。

退出时会把当前的打包 / 压缩 / 加密设置与筛选条件存到**可执行文件旁边**的 `save.dat`
（程序自己加的，整个目录拷走也不会丢；删掉它就是恢复出厂设置），下次启动自动读回。
筛选条件里的编号是顺次分配的、删除也不复用，所以编号只会变大——`clear-filters` 只清空列表。

一段真实会话（`settings` 与 `filters` 两段输出是原样抄的）：

```text
D:\demo|> set-archive true
D:\demo|> set-compress true 1
D:\demo|> add-filter not:(name:*.tmp)
[INFO] [2026-10-10 01:16:06.938] [Filter] Filter #1 added: not:(name:*.tmp)
D:\demo|> backup "D:\demo source" demo.db
D:\demo|> restore demo.db out
D:\demo|> settings
archive  : on
compress : on (algorithm 1)
encrypt  : off (algorithm 0, key missing)
D:\demo|> filters
types (9): name, path, owner, type, size, time, and, or, not
filters (1), all of them must pass:
  #1  not:(name:*.tmp)
D:\demo|> quit
```

`encrypt` 关掉时算法号显示 `0`，`key missing` 表示还没读密钥。备份文件里记着每一层
用过什么，所以还原时**只要求密钥一致**，压缩/加密的开关和算法号不用设回去。

---

## 四、图形界面说明

四个页签：

**备份** —— 选源目录和备份文件位置，勾选打包/压缩/加密（压缩算法可选 Huffman 或 RLE，
加密算法可选 AES 或 DES），密钥既可以指定密钥文件，也可以直接输入口令
（界面侧对口令做 SHA-256 派生）。底部实时显示当前设置。若设置了筛选条件，
这里会显示条数。

**还原** —— 选备份文件和输出目录。**「备份内容（自动识别）」会逐层读开备份文件，
把里面用到的处理方式依次列出来**，例如：

```text
已加密 (AES) → 已压缩 (Huffman) → 已打包
```

加密层需要密钥才能看穿，所以填上密钥后会自动补全。若备份是加密的而密钥留空，
点还原会被直接拦下，不会让内核去解密一堆错误数据。

**筛选设置** —— 表格列出已添加的条件。点「添加条件…」在弹出的对话框里选类型、
按提示填写参数，下半部分会实时显示最终要提交的规格串，也可以直接改。
注意**筛选是白名单**：每一条都必须通过，条目才会被备份；想表达"排除 .tmp"
要写 `not:(name:*.tmp)`，而不是 `name:*.tmp`。

规格语法：

```text
name:*.tmp                      文件名（不含目录），* 和 ? 是通配符
path:src/*.csv                  相对路径，模式里统一写 /
owner:*admin*                   属主账户名
type:regular,hardlink           文件类型
size:min=1M / max=4K / =0 / 1M..2M    字节大小，单位 k m g 按 1024
time:min=2026-01-01             时间，可加 write / access / create 前缀
and:(name:*.txt;size:min=1M)    所有子条件都通过
or:(name:*.tmp;name:*.log)      任一子条件通过
not:(name:*.tmp)                子条件不通过
```

**运行日志** —— 内核日志的完整内容，和命令行里看到的 `[INFO]/[WARNING]/[ERROR]`
是同一条，可清空、可导出到文件。

---

## 五、从源码构建

需要 MSYS2 的 MinGW64 工具链（`g++` + `mingw32-make`），图形版还需要 Qt6。

```bash
# 安装依赖（只需一次）
pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-pkgconf

cd /d/<你的路径>/DataBackup

make            # 只编命令行版：app.exe
make gui        # 只编图形版：app-gui.exe（需要 Qt6）
make both       # 两个都要
make run        # 编完就直接跑命令行版（make run-gui 同理）
make clean      # 清掉两个产物和全部中间文件
```

构建脚本只有一份 `Makefile`，**内核源码只编一遍**：内核全在 `Core/` 下（目录层级跟以前
一样，只是套了一层），47 个源文件编到 `build/Core/**`，两个前端链接时共用这批 .o；
两个入口在根目录、界面在 `UI/`，各自编自己的 .o。所以先 `make` 再 `make gui` 时内核
一个都不会重编，改一行界面代码也牵动不到内核。

Qt6 是用 `pkg-config` 探测的：**没装 Qt6 时 `make`、`make run`、`make clean` 照常
可用**，只有 `make gui` / `make both` 会打印一句安装提示并以非零退出。

编译选项统一是 `-std=c++17 -Wall -O2`，**不带 `-g`**（共享内核一旦带上调试信息，
图形版会从 1 MB 出头涨到 24 MB）。偶尔要调试符号就临时补上：

```bash
make FLAGS="-std=c++17 -Wall -O2 -g -I."
```

### 在 VSCode 里构建

`Ctrl+Shift+B` 直接构建（默认任务是控制台版），命令面板里另有 `Clean` 与
`Build GUI` 两个任务。任务里写的是 `mingw32-make.exe` 的绝对路径，所以不用自己
配终端 PATH。

要在 PowerShell 里手敲 `make` 的话，得先让 MSYS2 的 `usr/bin` 进 PATH——
Makefile 用 `SHELL := /bin/sh`，`make clean` 还要用到 `find`：

```powershell
$env:PATH = 'C:\msys64\usr\bin;' + $env:PATH
make -j4
```

### 打一个可以直接发出去的包

```powershell
.\package.ps1               # 输出 D:\DataBackup-最终成果\ 和同名 zip
.\package.ps1 -OutRoot E:\  # 换一个输出盘
```

前提是先 `make both`。脚本把源码、构建脚本、两个 exe 连同它们依赖的运行库、说明
文件、界面截图收进一个目录再压成 zip；截图是可选的，放在 `UI/screenshots/` 里就会
被一起打进去。

---

## 六、目录结构

```text
.
├── mainConsole.cpp / mainGUI.cpp         两个前端入口（命令行 / Qt 图形界面）
├── Core/                                 内核：两个前端共用的全部业务代码
│   ├── System.cpp / System.h             系统装配：注册并初始化各个 Manager
│   ├── FileStruct/                       文件条目、元数据、归档块结构
│   ├── General/                          调试日志、序列化工具、UTF-8 转换
│   └── Managers/
│       ├── FileIO/                       文件读写（内容 / 元数据 / 硬链接 / 重解析点）
│       ├── Archive/                      打包解包
│       ├── Compression/                  压缩解压（Huffman / RLE）
│       ├── Encryption/                   加密解密（AES / DES）
│       ├── Filter/                       筛选框架与九种筛选器
│       ├── Pipeline/                     备份 / 还原的流水线编排
│       ├── Command/                      命令注册、参数解析
│       └── TestCase/                     内置单元测试
├── UI/                                   Qt 图形界面（界面外观单独在 UI/Style.*）
│   └── screenshots/                      界面截图（可选，打发布包时会收进 zip）
├── Makefile                              一份构建脚本：make / make gui / make both
├── package.ps1                           收 exe + 运行库 + 源码，打成发布 zip
├── run-tests.ps1                         自动化回归测试（15 项）
├── Test/                                 开发用样例目录与密钥（打发布包时跳过）
└── .vscode/                              构建任务（Ctrl+Shift+B）与编辑器配置
```

编译产物在 `build/` 下，**与源码目录一一对应**（路径只换个后缀）：

```text
Core/Managers/Filter/Filters/NameFilter.cpp   →   build/Core/Managers/Filter/Filters/NameFilter.o
Core/System.cpp                               →   build/Core/System.o
mainConsole.cpp                               →   build/mainConsole.o
mainGUI.cpp                                   →   build/mainGUI.o
UI/MainWindow.cpp                             →   build/UI/MainWindow.o
```

内核只编一遍，两个前端链接时共用 `build/Core/**` 那批 .o；`make clean` 会把 `build/`
和两个 exe 一起清掉。

内核与界面完全解耦：`System` 负责注册并初始化各个 Manager，命令行前端走
`System::Start()` 的交互循环，图形前端则直接调用各 Manager 的方法，
两者共用同一套业务逻辑。甚至连算法清单（编号 + 名字 + 用途）和筛选类型清单都是
内核提供的——`make gui` 只是把同一份内核链接上 Qt，前端没有任何写死的列表。

---

## 七、测试

命令行版可以跑自动化回归，逐文件比对 SHA256 与修改时间：

```powershell
.\run-tests.ps1
```

* **9 个用例**：打包 / 不打包、Huffman、RLE、AES、DES 的备份还原往返，
  以及三条筛选（白名单 `name:*.tmp`、`and:(type:regular;size:max=1k)`、
  `not:(name:*.tmp)`），每条都要求还原结果与源目录逐字节、逐时间戳一致。
* **6 项异常输入**：路径不存在、参数个数不对、算法号越界、没密钥就加密、
  全部条目被过滤、操作不存在的过滤器——要求报错清楚且不崩溃。

合计 15 项，退出码 0 表示全部通过，测试数据留在 `Test\_auto` 下方便复查。

---

## 八、已知限制

* 还原是"往目录里写"，**不会清理目标目录**，重复还原到同一目录会混入上次的残留文件。
* 布尔参数写错不报错：`set-archive 随便` 会被当成 `false`（只认前文列的几种写法）。
* 不勾「打包」时，备份产物是一个目录；此时**源目录里不要放联接点或符号链接**，
  因为这类条目的"内容"是链接目标路径，被压缩/加密包装后无法重建（当前会报失败）。
* 备份/还原是同步阻塞的，图形界面的进度条只能是不确定态，也**没有取消按钮**。
* 归档条目数用 16 位计数，单个归档**最多 65535 个条目**。
* 命令行版手敲中文路径会静默失败：程序按 UTF-8 解释参数，而中文 Windows 终端的
  输入代码页是 936。图形界面不受影响。

---

## 变更记录

按日期记录的更新日志见 **[CHANGELOG.md](CHANGELOG.md)**。
