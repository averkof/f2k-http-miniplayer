Write-Output "Building C++ executable..."
$buildAttempt = g++ .\src\network.cpp -o .\bin\test.exe -I.\include -lws2_32 2>&1 #-lws2_32 -luser32 -ldwmapi 2>&1
if (!($buildAttempt -match "error")) {
    Write-Output "Build complete..."
    Write-Output "Running executable..."
    & ".\bin\test.exe"
    Write-Output "Execution complete."
} else {
    Write-Output "Build failed, see compiler output below."
    Write-Output $buildAttempt
}
