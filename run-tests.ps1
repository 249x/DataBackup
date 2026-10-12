<#
  数据备份软件 - 自动化功能测试脚本

  用法（在 VSCode 的 PowerShell 终端里）:
      .\run-tests.ps1

  它做的事:
      1. 在 Test\_auto 下造一棵固定的测试目录树
      2. 把一批命令喂给 app.exe（等价于你在终端里一条条手敲）
      3. 对每个用例：备份 -> 还原 -> 逐文件比对 SHA256 和修改时间
      4. 打印 PASS/FAIL 汇总，退出码 0 表示全过

  覆盖用例: 打包备份/还原、压缩(Huffman/RLE)、加密(AES/DES)、
            元数据(修改时间)保全、筛选(白名单语义)、异常输入健壮性
#>
param(
    [string]$ProjectRoot = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

# 喂给 app.exe 的命令文本必须不受调用者终端影响。PowerShell 把字符串管道给原生程序时用的
# 是 $OutputEncoding，而 VS Code 的 UTF-8 控制台（或常见的 profile 写法
# $OutputEncoding = [Console]::OutputEncoding = [Text.Encoding]::UTF8）会把它设成
# "UTF-8 带 BOM"：每一段管道内容前面都会被写入 EF BB BF，第一行命令跟着坏掉
# （实测 cd 失效并报 No command is found），后面全程都在错目录里跑、9 个用例齐刷刷"缺少文件"。
# 这里固定成不带 BOM 的 UTF-8——也正好是程序期望的编码。
$OutputEncoding = New-Object System.Text.UTF8Encoding($false)

# app.exe 依赖 MinGW 运行时 DLL，测试脚本自己保证 PATH 里有
$MsysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$MingwBin = Join-Path $MsysRoot 'mingw64\bin'
if ($env:PATH -notlike "*$MingwBin*") {
    $env:PATH = "$MingwBin;$(Join-Path $MsysRoot 'usr\bin');$env:PATH"
}

$App  = Join-Path $ProjectRoot 'app.exe'
$Work = Join-Path $ProjectRoot 'Test\_auto'
$Src  = Join-Path $Work 'src'

if (-not (Test-Path -LiteralPath $App)) {
    throw "找不到 app.exe，请先在 $ProjectRoot 下执行 make"
}

# app.exe 会把 [INFO]/[WARNING]/[ERROR] 日志写到 stderr。Windows PowerShell 5.1 在
# $ErrorActionPreference = 'Stop' 下会把原生程序的 stderr 当成终止性错误（NativeCommandError），
# 异常输入那一段会被第一个 [ERROR] 直接打断，所以喂命令时临时放宽这个开关。
function Invoke-App([string]$CmdText) {
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        return ($CmdText | & $App 2>&1)
    } finally {
        $ErrorActionPreference = $saved
    }
}

# ---------------------------------------------------------------- 造测试数据
if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $Src 'sub') -Force | Out-Null

[System.IO.File]::WriteAllBytes((Join-Path $Src 'a.txt'), [System.Text.Encoding]::ASCII.GetBytes("aaa`n"))
[System.IO.File]::WriteAllBytes((Join-Path $Src 'b.tmp'), [System.Text.Encoding]::ASCII.GetBytes("bbb`n"))
[System.IO.File]::WriteAllBytes((Join-Path $Src 'c.log'), [System.Text.Encoding]::ASCII.GetBytes("ccc`n"))
[System.IO.File]::WriteAllBytes((Join-Path $Src 'sub\d.txt'), [System.Text.Encoding]::ASCII.GetBytes("ddd`n"))
[System.IO.File]::WriteAllBytes((Join-Path $Src 'sub\e.tmp'), [System.Text.Encoding]::ASCII.GetBytes("eee`n"))

# 100 KiB 的伪随机内容，便于观察压缩效果
$bytes = New-Object 'byte[]' 102400
for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = [byte](($i * 37 + 11) % 251) }
[System.IO.File]::WriteAllBytes((Join-Path $Src 'big.bin'), $bytes)

# 密钥文件：AES 要 32 字节，DES 要 8 字节
$key32 = New-Object 'byte[]' 32
$key8  = New-Object 'byte[]' 8
for ($i = 0; $i -lt 32; $i++) { $key32[$i] = [byte]($i + 1) }
for ($i = 0; $i -lt 8;  $i++) { $key8[$i]  = [byte]($i + 1) }
[System.IO.File]::WriteAllBytes((Join-Path $Work 'key32.bin'), $key32)
[System.IO.File]::WriteAllBytes((Join-Path $Work 'key8.bin'),  $key8)

# 源目录里所有文件（用于"应完整还原"的用例）
$allFiles = @('a.txt', 'b.tmp', 'big.bin', 'c.log', 'sub\d.txt', 'sub\e.tmp')

# ------------------------------------------------------------------ 测试用例
# Expect 为 $null 表示"必须和源目录完全一致"，否则是期望的文件清单
$cases = @(
    @{ Name = '打包备份 + 还原（默认设置）'
       Cmds = @('set-archive true', 'set-compress false 0', 'set-encrypt false 0',
                'backup src c1.db', 'restore c1.db r1')
       Dir = 'r1'; Expect = $null }

    @{ Name = '不打包（备份产物是目录树）'
       Cmds = @('set-archive false', 'backup src c2.out', 'restore c2.out r2')
       Dir = 'r2'; Expect = $null }

    @{ Name = '打包 + Huffman 压缩'
       Cmds = @('set-archive true', 'set-compress true 1',
                'backup src c3_h.db', 'restore c3_h.db r3')
       Dir = 'r3'; Expect = $null }

    @{ Name = '打包 + RLE 压缩'
       Cmds = @('set-compress true 2', 'backup src c4_r.db', 'restore c4_r.db r4')
       Dir = 'r4'; Expect = $null }

    @{ Name = '打包 + AES 加密'
       Cmds = @('set-compress false 0', 'set-encrypt true 1', 'key key32.bin',
                'backup src c5_aes.db', 'restore c5_aes.db r5')
       Dir = 'r5'; Expect = $null }

    @{ Name = '打包 + DES 加密'
       Cmds = @('set-encrypt true 2', 'key key8.bin',
                'backup src c6_des.db', 'restore c6_des.db r6')
       Dir = 'r6'; Expect = $null }

    @{ Name = '筛选：name:*.tmp（白名单，只备份 .tmp）'
       Cmds = @('set-encrypt false 0', 'add-filter name:*.tmp',
                'backup src c7.db', 'restore c7.db r7', 'remove-filter 1')
       Dir = 'r7'; Expect = @('b.tmp', 'sub\e.tmp') }

    @{ Name = '筛选：and:(type:regular;size:max=1k)'
       Cmds = @('add-filter and:(type:regular;size:max=1k)',
                'backup src c8.db', 'restore c8.db r8', 'remove-filter 2')
       Dir = 'r8'; Expect = @('a.txt', 'b.tmp', 'c.log', 'sub\d.txt', 'sub\e.tmp') }

    @{ Name = '筛选：not:(name:*.tmp)'
       Cmds = @('add-filter not:(name:*.tmp)',
                'backup src c9.db', 'restore c9.db r9', 'remove-filter 3')
       Dir = 'r9'; Expect = @('a.txt', 'big.bin', 'c.log', 'sub\d.txt') }
)

# 命令一次性喂给程序，最后一定要 quit
$script = New-Object System.Collections.Generic.List[string]
$script.Add("cd $Work")

foreach ($case in $cases) {
    # 还原目标目录必须先清掉：restore 是"往目录里写"，不会删除上一次的残留
    $target = Join-Path $Work $case.Dir
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
    $script.AddRange([string[]]$case.Cmds)
}

$script.Add('quit')
$output = Invoke-App ($script -join "`n")

# 先确认命令真的被吃进去了：cd 生效的话，提示符里会出现工作目录。
# 看不到就说明输入被（终端编码之类）搞坏了，直接报出来。
if (($output -join "`n") -notlike "*$Work|>*") {
    throw "app.exe 似乎没进到 $Work，喂进去的命令可能被破坏了（提示符里看不到工作目录）"
}

# ------------------------------------------------------------------ 结果校验
function Get-Tree([string]$Root) {
    if (-not (Test-Path -LiteralPath $Root)) { return @() }
    Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($Root.Length + 1)
        [pscustomobject]@{
            Rel  = $rel
            Hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            Time = $_.LastWriteTime
        }
    } | Sort-Object Rel
}

$srcTree  = Get-Tree $Src
$failures = New-Object System.Collections.Generic.List[string]
$rows     = New-Object System.Collections.Generic.List[object]

foreach ($case in $cases) {
    $tree    = Get-Tree (Join-Path $Work $case.Dir)
    $expect  = if ($null -eq $case.Expect) { $allFiles } else { $case.Expect }
    $got     = @($tree | ForEach-Object { $_.Rel })
    $missing = @($expect | Where-Object { $got -notcontains $_ })
    $extra   = @($got | Where-Object { $expect -notcontains $_ })

    # 内容比对：逐个文件核对 SHA256，同时核对修改时间
    $contentBad = New-Object System.Collections.Generic.List[string]
    $timeBad    = New-Object System.Collections.Generic.List[string]
    foreach ($f in $tree) {
        $s = $srcTree | Where-Object { $_.Rel -eq $f.Rel } | Select-Object -First 1
        if ($null -eq $s) { continue }
        if ($s.Hash -ne $f.Hash) { $contentBad.Add($f.Rel) }
        if ($s.Time -ne $f.Time) { $timeBad.Add($f.Rel) }
    }

    $ok = ($missing.Count -eq 0) -and ($extra.Count -eq 0) -and
          ($contentBad.Count -eq 0) -and ($timeBad.Count -eq 0)

    if (-not $ok) {
        $failures.Add($case.Name)
        if ($missing.Count)    { $failures.Add("    缺少: " + ($missing -join ', ')) }
        if ($extra.Count)      { $failures.Add("    多出: " + ($extra -join ', ')) }
        if ($contentBad.Count) { $failures.Add("    内容不符: " + ($contentBad -join ', ')) }
        if ($timeBad.Count)    { $failures.Add("    时间戳不符: " + ($timeBad -join ', ')) }
    }

    $rows.Add([pscustomobject]@{
        Result = $(if ($ok) { 'PASS' } else { 'FAIL' })
        Case   = $case.Name
        Files  = $tree.Count
    })
}

# 备份产物大小对比
"`n===== 备份产物 ====="
Get-ChildItem -LiteralPath $Work -File -Filter 'c*' |
    Sort-Object Name |
    Select-Object Name, Length |
    Format-Table -AutoSize | Out-String -Width 200 | Write-Host

"`n===== 用例结果 ====="
$rows | Format-Table -AutoSize | Out-String -Width 200 | Write-Host

# ------------------------------------------------------------ 异常输入健壮性
$badCmds = @(
    # 这里刻意用 ASCII 路径：中文参数经管道喂进来时终端按 GBK 编码，程序按 UTF-8 解释会
    # 得到无效路径（win32 error 123），报的是另一条错误——那是"控制台版手敲中文路径
    # 会失败"这个已知限制（见 README），不该跟"路径不存在要报错"混在一起测
    'backup no-such-dir x.db'
    'backup src'
    'set-compress true 9'
    'set-encrypt true 1'
    'backup src c_nokey.db'
    'set-encrypt false 0'
    'add-filter name:绝不存在'
    'backup src c_empty.db'
    'remove-filter 1'
    'remove-filter 99'
    'quit'
)
$badScript = New-Object System.Collections.Generic.List[string]
$badScript.Add("cd $Work")
$badScript.AddRange([string[]]$badCmds)
$badOut = Invoke-App ($badScript -join "`n")
$badText = $badOut -join "`n"

$checks = @(
    @{ Desc = '备份不存在的路径 -> 报错但不崩溃'; Need = 'Path does not exist' }
    @{ Desc = '参数个数不对 -> 明显提示';       Need = 'Argument count' }
    @{ Desc = '压缩算法号越界 -> 明显提示';     Need = 'No compression handler' }
    @{ Desc = '未加载密钥就加密 -> 拒绝执行';   Need = 'No key is loaded' }
    @{ Desc = '全部条目被过滤 -> 拒绝生成空备份'; Need = 'All entries are filtered out' }
    @{ Desc = '操作不存在的过滤器 -> 明显提示'; Need = 'No filter with id' }
)

"`n===== 异常输入健壮性 ====="
foreach ($c in $checks) {
    $ok = $badText -like "*$($c.Need)*"
    if (-not $ok) { $failures.Add($c.Desc) }
    "{0,-6} {1}" -f $(if ($ok) { 'PASS' } else { 'FAIL' }), $c.Desc
}

"`n测试数据保留在: $Work"

if ($failures.Count -gt 0) {
    "`n$($failures.Count) 项未通过:" | Write-Host
    $failures | ForEach-Object { "  $_" | Write-Host }
    exit 1
}

"`n全部通过。" | Write-Host
exit 0
