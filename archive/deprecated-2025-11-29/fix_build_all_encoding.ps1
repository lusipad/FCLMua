<#
.SYNOPSIS
    修复 build_all.ps1 的编码问题

.DESCRIPTION
    此脚本创建一个干净的、编码正确的 build_all.ps1 副本，
    将乱码的中文注释替换为正确的UTF-8编码内容。
#>

param(
    [string]$InputFile = "$PSScriptRoot\build_all.ps1",
    [string]$OutputFile = "$PSScriptRoot\build_all_fixed.ps1"
)

$ErrorActionPreference = 'Stop'

Write-Host "Reading original file with encoding detection..."
$content = Get-Content $InputFile -Raw -Encoding UTF8

# 定义替换映射：乱码 -> 正确的中文
$replacements = @{
    '缁熶竴鏋勫缓鑴氭湰' = '统一构建脚本'
    '缁熶竴鐨勬瀯寤鸿剼鏈紝鏀寔鎵€鏈夋瀯寤哄満鏅細' = '统一的构建脚本，支持所有构建场景：'
    '浠呮瀯寤洪┍鍔?' = '仅构建驱动'
    '鏋勫缓椹卞姩骞剁鍚?' = '构建驱动并签名'
    '鏋勫缓鎵€鏈夌粍浠讹紙椹卞姩 \+ CLI Demo \+ GUI Demo锛?' = '构建所有组件（驱动 + CLI Demo + GUI Demo）'
    '鎵撳寘鍙戝竷鍖?' = '打包发布包'
    '鏇夸唬浜嗕箣鍓嶇殑涓変釜鑴氭湰锛?' = '替代了之前的三个脚本：'
    '锛堟棫锛?' = '（旧）'
    '鏋勫缓閰嶇疆锛欴ebug 鎴?Release锛堥粯璁わ細Debug锛?' = '构建配置：Debug 或 Release（默认：Debug）'
    '鐩爣骞冲彴锛歺64锛堥粯璁わ細x64锛?' = '目标平台：x64（默认：x64）'
    '鏄惁绛惧悕椹卞姩锛堥粯璁わ細鍚︼級' = '是否签名驱动（默认：否）'
    '浠呮瀯寤洪┍鍔紝涓嶆瀯寤?Demo' = '仅构建驱动，不构建 Demo'
    '璺宠繃椹卞姩鏋勫缓锛屼粎鏋勫缓 Demo' = '跳过驱动构建，仅构建 Demo'
    '璺宠繃 CLI Demo 鏋勫缓' = '跳过 CLI Demo 构建'
    '璺宠繃 GUI Demo 鏋勫缓' = '跳过 GUI Demo 构建'
    '鎵撳寘鎵€鏈変骇鐗╁埌 dist/bundle/' = '打包所有产物到 dist/bundle/'
    '浠庣姝ｅ紡鎵嶈鍑烘繁鍙湴杩涜' = '从根正式才解出深刻地进行'
    '鏋勫缓浣跨敤' = '构建使用'
    '鎸囧畾 WDK 鐗堟湰' = '指定 WDK 版本'
    '濡傛灉涓嶆寚瀹氾紝灏嗚嚜鍔ㄦ娴嬬郴缁熶笂鍙敤鐨?WDK 鐗堟湰' = '如果不指定，将自动探测系统上可用的 WDK 版本'
    '渚濊禆锛?' = '依赖：'
    '璇诲彇閰嶇疆鏂囦欢锛堝鏋滃瓨鍦級' = '读取配置文件（如果存在）'
    '鑾峰彇閰嶇疆鍊硷紙浼樺厛绾э細鍛戒护琛?> 鐜鍙橀噺 > 閰嶇疆鏂囦欢 > 榛樿鍊硷級' = '获取配置值（优先级：命令行 > 环境变量 > 配置文件 > 默认值）'
    '瀵煎叆鍏叡鍑芥暟搴?' = '导入公共函数库'
    '璇诲彇鏋勫缓閰嶇疆鏂囦欢' = '读取构建配置文件'
    '搴旂敤閰嶇疆锛堝鏋滄湭閫氳繃鍛戒护琛屾寚瀹氾級' = '应用配置（如果未通过命令行指定）'
    '璁＄畻浠撳簱鏍圭洰褰曞拰鍏抽敭璺緞' = '计算仓库根目录和关键路径'
    '鍙傛暟楠岃瘉' = '参数验证'
    '璁＄畻瑕佹墽琛岀殑姝ラ' = '计算要执行的步骤'
    '姝ラ 0: 鍑嗗渚濊禆' = '步骤 0: 准备依赖'
    '姝ラ 1: 鏋勫缓椹卞姩' = '步骤 1: 构建驱动'
    '纭繚 Musa.Runtime 閰嶇疆瀛樺湪' = '确保 Musa.Runtime 配置存在'
    '浣跨敤 MSBuild 鐩存帴鏋勫缓' = '使用 MSBuild 直接构建'
    '鏋勫缓鍙傛暟' = '构建参数'
    '楠岃瘉椹卞姩鏂囦欢瀛樺湪' = '验证驱动文件存在'
    '姝ラ 2: 绛惧悕椹卞姩锛堝鏋滈渶瑕侊級' = '步骤 2: 签名驱动（如果需要）'
    '澶嶅埗鍒?dist/ 鐩綍' = '复制到 dist/ 目录'
    '姝ラ 3: 鏋勫缓 CLI Demo' = '步骤 3: 构建 CLI Demo'
    '姝ラ 4: 鏋勫缓 GUI Demo' = '步骤 4: 构建 GUI Demo'
    '鏋勫缓瀹屾垚鎬荤粨' = '构建完成总结'
    '杈撳嚭鏋勫缓浜х墿浣嶇疆' = '输出构建产物位置'
}

Write-Host "Applying replacements..."
foreach ($pattern in $replacements.Keys) {
    $replacement = $replacements[$pattern]
    $content = $content -replace $pattern, $replacement
}

Write-Host "Writing fixed content to: $OutputFile"
$content | Out-File -FilePath $OutputFile -Encoding UTF8 -NoNewline

Write-Host "Done! Fixed file created at: $OutputFile" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Review the fixed file: $OutputFile"
Write-Host "  2. If satisfied, replace the original:"
Write-Host "     Move-Item -Force '$OutputFile' '$InputFile'"
