<#
  把最终成果打包成一个可以直接发给别人、解压即用的压缩包。

  内容：源代码 + 构建脚本 + 可执行程序（连同运行库）+ 说明 + 界面截图

  用法（在项目根目录）：
      .\package.ps1                    输出 D:\DataBackup-最终成果\ 和同名 .zip
      .\package.ps1 -OutRoot E:\       换一个输出盘

  前提：app.exe 和 app-gui.exe 已经编译好（make both）。
#>
param(
    [string]$ProjectRoot = $PSScriptRoot,
    [string]$OutRoot = 'D:\',
    [string]$Name = 'DataBackup-最终成果',
    [string]$MingwRoot = 'C:\msys64\mingw64',
    [string]$MsysBash = 'C:\msys64\usr\bin\bash.exe'
)

$ErrorActionPreference = 'Stop'

# ldd 只有在 MinGW 环境下才能把 /mingw64/bin 下的依赖解析出来，
# 否则它只会列出系统 DLL，收集运行库时会一个都找不到。
$env:MSYSTEM = 'MINGW64'

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
# 源码目录里不应该带编译产物
Get-ChildItem -LiteralPath $srcDir -Recurse -Include '*.exe', '*.o', '*.d' -File |
    Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host '2/5 复制可执行程序…'
Copy-Item -LiteralPath $consoleExe -Destination $consoleDir -Force
Copy-Item -LiteralPath $guiExe     -Destination $guiDir -Force

Write-Host '3/5 收集运行库并随包分发…'
function Copy-Runtime([string]$exePath, [string]$targetDir) {
    $msysExe = To-MsysPath $exePath
    # `$PATH 是转义写法，让 bash 看到的是变量而不是 PowerShell 变量
    $lines = & $MsysBash -lc "PATH=/mingw64/bin:`$PATH ldd '$msysExe'"
    $copied = 0
    foreach ($line in $lines) {
        if ($line -match '=>\s+(/mingw64/bin/[^\s]+)') {
            $dllName = Split-Path $Matches[1] -Leaf
            $source = Join-Path $MingwRoot (Join-Path 'bin' $dllName)
            if (Test-Path -LiteralPath $source) {
                Copy-Item -LiteralPath $source -Destination $targetDir -Force
                $copied++
            }
        }
    }
    Write-Host ("      {0} -> {1} 个 DLL" -f (Split-Path $exePath -Leaf), $copied)
}
Copy-Runtime $consoleExe $consoleDir
Copy-Runtime $guiExe $guiDir

# Qt 的平台插件是运行时按名字加载的，ldd 看不到，必须手动带上
foreach ($pluginDir in @('platforms', 'styles')) {
    $source = Join-Path $MingwRoot (Join-Path 'share\qt6\plugins' $pluginDir)
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
