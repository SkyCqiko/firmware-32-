@echo off
chcp 65001 >nul
cd /d "%~dp0"

echo ============================================
echo   一键上传到 GitHub
echo   仓库: https://github.com/skyCqiko/firmware-32-
echo ============================================
echo.

echo [1/4] 当前改动:
git status --short
echo.

echo [2/4] 加入暂存区...
git add -A

echo [3/4] 提交...
set /p msg=请输入本次提交说明(直接回车则用"更新代码"): 
if "%msg%"=="" set msg=更新代码
git commit -m "%msg%"
if errorlevel 1 (
    echo.
    echo 没有需要提交的改动, 直接尝试推送...
)

echo.
echo [4/4] 推送到 GitHub...
git push

echo.
echo ============================================
echo   完成! 按任意键关闭
echo ============================================
pause >nul
