
@echo off

if "%1"=="Clean" (
   del /Q *.exe *.pdb *.ilk *.obj
   echo Cleaned up all artifacts.
   exit /B 0
)

set TRACY_PROFILE_FLAGS=/DTRACY_ENABLE /DTRACY_CALLSTACK=10 /DTRACY_ALLOC /DTRACY_SAMPLING /DTRACY_ON_DEMAND
set TRACY_CLIENT_CPP="D:\softwares\tracy\public\TracyClient.cpp"

cl ^
/Od /Zi /DEBUG %TRACY_PROFILE_FLAGS% ^
/I "D:\softwares\tracy\public" ^
feditor.cpp %TRACY_CLIENT_CPP% ^
/EHsc /Fe:feditor.exe

echo Build successful.
