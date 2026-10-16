<#
  把最终成果打包成一个可以直接发给别人、解压即用的压缩包。

  内容：源代码 + 构建脚本 + 可执行程序（连同运行库）+ 说明 + 界面截图

  用法（在项目根目录）：
      .\package.ps1                    输出到项目旁边的上级目录
      .\package.ps1 -OutRoot E:\       换一个输出位置（会在里面建 DataBackup-最终成果）
      .\package.ps1 -MsysRoot D:\msys64 -Toolchain mingw64
                                       一般不用给，脚本会自己找工具链

  前提：app.exe 和 app-gui.exe 已经编译好（make both）。
#>
param(
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$OutRoot = (Split-Path -Parent $ProjectRoot),
    [string]$Name = 'DataBackup-最终成果',
    [string]$MsysRoot,
    [string]$Toolchain
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- 工具链探测
#   命令行参数 -> 环境变量 MSYS2_ROOT -> PATH 上的 g++ 反推 -> PATH 上的 bash 反推
#   -> %SystemDrive%\msys64，最后还找不到就报错并告诉怎么传参。
function Test-MsysLayout([string]$root, [string]$prefix) {
    if (-not $root -or -not $prefix) { return $null }
    $bash = Join-Path $root 'usr\bin\bash.exe'
    $gxx  = Join-Path $root (Join-Path $prefix 'bin\g++.exe')
    if (-not (Test-Path -LiteralPath $bash)) { return $null }
    if (-not (Test-Path -LiteralPath $gxx))  { return $null }
    return [pscustomobject]@{
        Root          = (Resolve-Path -LiteralPath $root).Path
        Prefix        = $prefix
        Bash          = $bash
        ToolchainRoot = (Join-Path $root $prefix)
    }
}

# 某个可执行文件在磁盘上的目录；eg. PATH 上的 g++.exe -> ...\ucrt64\bin
function Get-CommandDir([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($cmd -and $cmd.Source -and (Test-Path -LiteralPath $cmd.Source)) {
        return (Split-Path -Parent $cmd.Source)
    }
    return $null
}

$prefixList = @('ucrt64', 'mingw64', 'clang64')
if ($Toolchain) { $prefixList = @($Toolchain) + ($prefixList | Where-Object { $_ -ne $Toolchain }) }

$layout = $null
if ($MsysRoot) {
    foreach ($p in $prefixList) { if ($layout = Test-MsysLayout $MsysRoot $p) { break } }
}
if (-not $layout -and $env:MSYS2_ROOT) {
    foreach ($p in $prefixList) { if ($layout = Test-MsysLayout $env:MSYS2_ROOT $p) { break } }
}
if (-not $layout) {
    # 工具链前缀从 g++ 的位置就能看出来（<root>\<prefix>\bin\g++.exe）
    $gxxDir = Get-CommandDir 'g++'
    if ($gxxDir) {
        $layout = Test-MsysLayout (Split-Path (Split-Path $gxxDir -Parent) -Parent) (Split-Path $gxxDir -Leaf)
    }
}
if (-not $layout) {
    # 退一步：bash 自己也在 PATH 上，从它直接算出根目录
    $bashDir = Get-CommandDir 'bash'
    if ($bashDir) {
        $root = Split-Path (Split-Path $bashDir -Parent) -Parent
        foreach ($p in $prefixList) { if ($layout = Test-MsysLayout $root $p) { break } }
    }
}
if (-not $layout) {
    $fallback = Join-Path $env:SystemDrive 'msys64'
    foreach ($p in $prefixList) { if ($layout = Test-MsysLayout $fallback $p) { break } }
}
if (-not $layout) {
    throw ("找不到 MSYS2 工具链（需要有 <root>\usr\bin\bash.exe 和 <root>\<ucrt64|mingw64>\bin\g++.exe）。`n" +
           "请显式指定，例如： .\package.ps1 -MsysRoot '<MSYS2 安装目录>' -Toolchain ucrt64")
}

$MsysRoot      = $layout.Root
$Toolchain     = $layout.Prefix
$MsysBash      = $layout.Bash
$ToolchainRoot = $layout.ToolchainRoot

# ldd 只有在该工具链自己的环境下才能把 <prefix>/bin 下的依赖解析出来，
# 否则它只会列出系统 DLL，收集运行库时会一个都找不到。
$env:MSYSTEM = $Toolchain.ToUpper()

Write-Host ("工具链：{0} / {1}" -f $MsysRoot, $Toolchain)

function To-MsysPath([string]$path) {
    $p = $path -replace '\\', '/'
    if ($p -match '^([A-Za-z]):(.*)$') {
        return '/' + $Matches[1].ToLower() + $Matches[2]
    }
    return $p
}

$consoleExe = Join-Path $ProjectRoot 'app.exe'
$guiExe     = Join-Path $ProjectRoot 'app-gui.exe'
foreach ($exe in @($consoleExe, $guiExe)) {
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "找不到 $exe，请先执行 make both"
    }
}

$pkg = Join-Path $OutRoot $Name
$zip = "$pkg.zip"
if (Test-Path -LiteralPath $pkg) { Remove-Item -LiteralPath $pkg -Recurse -Force }
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }

$srcDir     = Join-Path $pkg '源代码'
$binDir     = Join-Path $pkg '可执行程序'
$consoleDir = Join-Path $binDir '控制台版'
$guiDir     = Join-Path $binDir '图形版'
$shotDir    = Join-Path $pkg '界面截图'
foreach ($d in @($pkg, $srcDir, $binDir, $consoleDir, $guiDir, $shotDir)) {
    New-Item -ItemType Directory -Path $d -Force | Out-Null
}

Write-Host '1/5 复制源代码…'
$skipTop = @('build', 'Test', '.git')
Get-ChildItem -LiteralPath $ProjectRoot -Force | Where-Object {
    $_.Name -notin $skipTop -and $_.Extension -ne '.zip'
} | ForEach-Object {
    if ($_.PSIsContainer) {
        Copy-Item -LiteralPath $_.FullName -Destination $srcDir -Recurse -Force
    } else {
        Copy-Item -LiteralPath $_.FullName -Destination $srcDir -Force
    }
}
# 源码目录里不应该带编译产物，也不应该带本机跑出来的运行时存档（save.dat 是
# 程序自己在可执行文件旁边生成的，带进包里会让别人一启动就拿到你的设置）。
# ⚠️ 不能写成 Get-ChildItem -LiteralPath $srcDir -Recurse -Include '*.exe'：
# -Include 配上 -LiteralPath 会被静默忽略，实测它把 124 个文件全选中了，
# 接着 Remove-Item 就把整个源码副本删空（目录还在、里面一个文件都没有）。
# 所以这里显式按扩展名筛。
Get-ChildItem -LiteralPath $srcDir -Recurse -File |
    Where-Object { $_.Extension -in @('.exe', '.o', '.d', '.dat') } |
    Remove-Item -Force

# 自检：万一哪天真没复制成功，宁可当场报错也不要打出一个空壳包
$srcCount = (Get-ChildItem -LiteralPath $srcDir -Recurse -File | Measure-Object).Count
if ($srcCount -eq 0) { throw "源代码没有复制成功（$srcDir 是空的）" }
Write-Host ("      源代码 {0} 个文件" -f $srcCount)

Write-Host '2/5 复制可执行程序…'
Copy-Item -LiteralPath $consoleExe -Destination $consoleDir -Force
Copy-Item -LiteralPath $guiExe     -Destination $guiDir -Force

Write-Host '3/5 收集运行库并随包分发…'

# bash 自己也可能往 stderr 写东西（ldd 的警告之类），而 $ErrorActionPreference = 'Stop'
# 会把原生程序的 stderr 当成终止性错误，所以跑它的时候临时放宽
function Invoke-Bash([string]$command) {
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        return (& $MsysBash -lc $command 2>&1)
    } finally {
        $ErrorActionPreference = $saved
    }
}

function Copy-Runtime([string]$exePath, [string]$targetDir) {
    $msysExe = To-MsysPath $exePath
    # `$PATH 是转义写法，让 bash 看到的是变量而不是 PowerShell 变量
    $lines = Invoke-Bash "PATH=/$Toolchain/bin:`$PATH ldd '$msysExe'"
    # 只认本工具链自己那套 bin 下的依赖
    $pattern = '=>\s+(/' + [regex]::Escape($Toolchain) + '/bin/[^\s]+)'
    $copied = 0
    foreach ($line in $lines) {
        if ($line -match $pattern) {
            $dllName = Split-Path $Matches[1] -Leaf
            $source = Join-Path $ToolchainRoot (Join-Path 'bin' $dllName)
            if (Test-Path -LiteralPath $source) {
                Copy-Item -LiteralPath $source -Destination $targetDir -Force
                $copied++
            }
        }
    }
    Write-Host ("      {0} -> {1} 个 DLL" -f (Split-Path $exePath -Leaf), $copied)
    if ($copied -eq 0) {
        Write-Warning "一个运行库都没收到：工具链前缀可能不对（当前 $Toolchain），或者 ldd 没跑起来。"
    }
}
Copy-Runtime $consoleExe $consoleDir
Copy-Runtime $guiExe $guiDir

# Qt 的平台插件是运行时按名字加载的，ldd 看不到，必须手动带上
foreach ($pluginDir in @('platforms', 'styles')) {
    $source = Join-Path $ToolchainRoot (Join-Path 'share\qt6\plugins' $pluginDir)
    if (Test-Path -LiteralPath $source) {
        $target = Join-Path $guiDir $pluginDir
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        Copy-Item -Path (Join-Path $source '*.dll') -Destination $target -Force
    }
}

Write-Host '4/5 写说明文件…'
$readme = Join-Path $ProjectRoot 'README.md'
if (Test-Path -LiteralPath $readme) {
    Copy-Item -LiteralPath $readme -Destination (Join-Path $pkg '项目说明.md') -Force
}

$changelog = Join-Path $ProjectRoot 'CHANGELOG.md'
if (Test-Path -LiteralPath $changelog) {
    Copy-Item -LiteralPath $changelog -Destination (Join-Path $pkg '更新日志.md') -Force
}

$shots = Join-Path $ProjectRoot 'UI\screenshots'
if (Test-Path -LiteralPath $shots) {
    Copy-Item -Path (Join-Path $shots '*.png') -Destination $shotDir -Force
}

$consoleReadme = @'
控制台版 使用说明
=================

运行：在本目录里打开终端，输入  .\app.exe

启动后会出现提示符，形如  D:\demo|>  ，直接输入命令即可：

  help                     查看全部命令
  cd D:/demo               切换当前目录（只认 Windows 风格路径）
  backup src pack.db       备份 src 到 pack.db
  restore pack.db out      把 pack.db 还原到 out 目录
  settings                 查看打包/压缩/加密的当前开关
  set-archive true         是否把多文件拼成一个归档文件
  set-compress true 1      压缩：1=Huffman，2=RLE
  set-encrypt true 1       加密：1=AES，2=DES
  key key32.bin            设置密钥文件（AES≥32 字节，DES≥8 字节）
  add-filter name:*.tmp    添加筛选条件（白名单：所有条件都通过才备份）
  filters                  查看已添加的筛选条件
  quit                     退出

注意：
  * 用管道或文件喂命令时，最后一定要写 quit，否则程序在读到输入结束后
    会一直刷提示符。
  * 手敲中文路径会静默失败（终端代码页是 936，程序按 UTF-8 解释参数）。
    演示时请用英文路径。
  * 还原是"往目录里写"，不会清理目标目录，建议每次还原都换一个新目录。
'@
Set-Content -LiteralPath (Join-Path $consoleDir '使用说明.txt') -Value $consoleReadme -Encoding UTF8

$guiReadme = @'
图形版 使用说明
===============

运行：双击 app-gui.exe

四个页签：

  【备份】选源目录和备份文件位置，勾选打包/压缩/加密。
          压缩可选 Huffman 或 RLE；加密可选 AES 或 DES，
          密钥既可以指定密钥文件，也可以直接输入口令。
          底部实时显示当前设置。

  【还原】选备份文件和输出目录。
          「备份内容（自动识别）」会逐层读开备份文件，把里面的处理方式列出来，
          例如：已加密 (AES) → 已压缩 (Huffman) → 已打包
          加密的备份必须填对密钥才能还原；留空会被直接拦住。

  【筛选设置】点「添加条件…」，选类型后按提示填写；对话框下半部分实时显示
              最终规格串，也可以直接改。
              筛选是白名单：每一条都必须通过才会被备份。
              想排除 .tmp 要写 not:(name:*.tmp)。

  【运行日志】内核日志，和命令行里的 [INFO]/[WARNING]/[ERROR] 是同一条，
              可以清空或导出到文件。

注意：还原不会清理目标目录已有的文件，建议每次换一个新目录。
'@
Set-Content -LiteralPath (Join-Path $guiDir '使用说明.txt') -Value $guiReadme -Encoding UTF8

Write-Host '5/5 压缩…'
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $pkg, $zip, [System.IO.Compression.CompressionLevel]::Optimal, $true)

$sizeMb = [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)
Write-Host ''
Write-Host "完成：$zip  ($sizeMb MB)"
Write-Host "目录：$pkg"
