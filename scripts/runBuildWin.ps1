Write-Output "Building C++ executable..."
$buildAttempt = g++ .\src\main.cpp -o .\bin\main.exe -I.\include -lws2_32 -luser32 -ldwmapi -lgdiplus -lole32 -mwindows 2>&1
if (!($buildAttempt -match "error")) {
    Write-Output "Build complete..."
    Write-Output "Running executable..."
    & ".\bin\main.exe"
    Write-Output "Execution complete."
} else {
    Write-Output "Build failed, see compiler output below."
    Write-Output $buildAttempt
}
