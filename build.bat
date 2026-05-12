@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Invoke-Expression (Get-Content -Path '%~dp0scripts\build.ps1' -Encoding UTF8 -Raw)"
pause