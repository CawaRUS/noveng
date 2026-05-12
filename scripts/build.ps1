$MAKE = ".\make\make.exe"

$LANGMODE = "RU"
$chcpOutput = & chcp.com | Out-String
if ($chcpOutput -match "437") {
    $LANGMODE = "EN"
}

if ($LANGMODE -eq "RU") {
    $MSG_THREADS = "Обнаружено потоков CPU:"
    $MSG_RECOMMENDED = "Рекомендуемое количество:"
    $MSG_ENTER = "Введите количество потоков (Enter ="
    $MSG_MENU1 = "1 - Сборка"
    $MSG_MENU2 = "2 - Сборка и запуск"
    $MSG_MENU3 = "3 - Очистка"
    $MSG_CHOICE = "Выберите действие: "
    $MSG_DONE = "Сборка завершена за"
} else {
    $MSG_THREADS = "Detected CPU threads:"
    $MSG_RECOMMENDED = "Recommended threads:"
    $MSG_ENTER = "Enter number of threads (Enter ="
    $MSG_MENU1 = "1 - Build"
    $MSG_MENU2 = "2 - Build and run"
    $MSG_MENU3 = "3 - Clean"
    $MSG_CHOICE = "Select action:"
    $MSG_DONE = "Build finished in"
}

$procCount = $env:NUMBER_OF_PROCESSORS
Write-Host ""
Write-Host "$MSG_THREADS $procCount"
$defaultThreads = [Math]::Max(1, [int]$procCount - 1)
Write-Host "$MSG_RECOMMENDED $defaultThreads"
Write-Host ""

$threadsInput = Read-Host "$MSG_ENTER $defaultThreads)"
if ([string]::IsNullOrWhiteSpace($threadsInput)) {
    $threads = $defaultThreads
} else {
    $threads = $threadsInput
}

Write-Host ""
Write-Host $MSG_MENU1
Write-Host $MSG_MENU2
Write-Host $MSG_MENU3
Write-Host ""

$action = Read-Host $MSG_CHOICE
Write-Host ""

$start = Get-Date

if ($action -eq "1") {
    & cmd.exe /c "`"$MAKE`" -j$threads"
} elseif ($action -eq "2") {
    & cmd.exe /c "`"$MAKE`" -j$threads run"
} elseif ($action -eq "3") {
    & cmd.exe /c "`"$MAKE`" clean"
}

$end = Get-Date

Write-Host ""
Write-Host "$MSG_DONE $($start.ToString('HH:mm:ss,ff')) > $($end.ToString('HH:mm:ss,ff'))"

