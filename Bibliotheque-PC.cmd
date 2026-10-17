@echo off
cd /d "%~dp0"
python tools\hub-library-gui.py
if errorlevel 1 pause
