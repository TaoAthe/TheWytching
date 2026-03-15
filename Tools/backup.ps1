param()
$source = "C:\Users\Sarah\Documents\Unreal Projects\TheWytching"
$dest   = "F:\TheWytching"
$log    = "F:\TheWytching\_backup_log.txt"
$timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
Write-Host ""
Write-Host "[$timestamp] Aisling Backup - TheWytching -> F:\" -ForegroundColor Cyan
Write-Host "Source: $source"
Write-Host "Dest:   $dest"
Write-Host ""
robocopy $source $dest /E /Z /W:1 /R:2 /NP /NDL /TEE /LOG+:"$log" /XD "$source\Binaries" "$source\DerivedDataCache" "$source\Intermediate" "$source\Saved" "$source\.git" "$source\.vs" "$source\.idea"
$exitCode = $LASTEXITCODE
Write-Host ""
if ($exitCode -le 3) {
    Write-Host "[$timestamp] Backup complete (robocopy exit $exitCode)." -ForegroundColor Green
} else {
    Write-Host "[$timestamp] Backup warnings (robocopy exit $exitCode). Check $log" -ForegroundColor Yellow
}
Write-Host ""