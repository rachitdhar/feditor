
@echo off

if "%1"=="Clean" (
   del /Q *.exe *.pdb *.ilk *.obj
   echo Cleaned up all artifacts.
   exit /B 0
)

cl /Zi feditor.cpp /EHsc /Fe:feditor.exe
echo Build successful.
