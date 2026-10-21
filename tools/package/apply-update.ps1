# Embedded in the player, not downloaded as executable instructions. Never
# elevates, installs optional runtimes, disables security, or removes user data.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Prepare','Apply')][string]$Mode,
    [Parameter(Mandatory)][string]$Work,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{64}$')][string]$Digest,
    [Parameter(Mandatory)][long]$Bytes,
    [int]$ParentId = 0,
    [long]$ParentStartTicks = 0
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# A game launched from pwsh/Python can inherit PowerShell 7's module path.
# Load the Windows PowerShell built-ins by their trusted absolute paths.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1')
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-NoLinks([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Update paths must not contain junctions or symbolic links.'
        }
        if ($item -is [IO.FileInfo]) { $item = $item.Directory } else { $item = $item.Parent }
    }
}
function Child-Path([string]$Root, [string]$Name) {
    $path = [IO.Path]::GetFullPath((Join-Path $Root $Name))
    if (!$path.StartsWith($Root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Update path escapes its directory.'
    }
    return $path
}
function Package-File([string]$Name) {
    # Explicitly exclude settings, layouts, ROMs, BIN/PAK data, saves, logs,
    # vrpreferences.json and files from unrelated applications. Unknown files
    # are skipped; only these package-owned paths may be replaced.
    return $Name -match '^(licenses/[^:]+|optional-runtimes/displayxr/[^:]+)$' -or
        $Name -match '^(starfox_pc\.exe|starfox_asset_builder\.exe|starfox_leia_sr_check\.exe|starfox_leia_sr\.dll|SDL3\.dll|dxcompiler\.dll|dxil\.dll|openxr_loader\.dll|README\.md|CREDITS\.md|THIRD_PARTY_NOTICES\.md|LICENSE-XBRZ\.txt|LEIA-SR\.txt|Diagnose-Leia-SR\.cmd)$'
}
function Safe-Name([string]$Name) {
    $name = $Name.TrimEnd('/')
    return $name.Length -gt 0 -and $name -notmatch '[\\:\x00-\x1f<>"|?*]' -and
        !$name.StartsWith('/') -and $name -notmatch '(^|/)(\.|\.\.|)(/|$)' -and
        $name -notmatch '(^|/)(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|/|$)' -and
        $name -notmatch '[. ](/|$)'
}
function Machine([string]$Path) {
    $file = [IO.File]::OpenRead($Path)
    $reader = New-Object IO.BinaryReader($file)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Update executable is not a PE file.' }
        $file.Position = 0x3c
        $offset = $reader.ReadUInt32()
        if ($offset -gt $file.Length - 6) { throw 'Invalid PE header.' }
        $file.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw 'Invalid PE signature.' }
        return $reader.ReadUInt16()
    } finally { $reader.Dispose(); $file.Dispose() }
}

$Work = [IO.Path]::GetFullPath($Work).TrimEnd('\','/')
$Target = [IO.Path]::GetFullPath($Target).TrimEnd('\','/')
$backup = $null
$replaced = New-Object 'Collections.Generic.List[string]'
try {
    if ((Split-Path -Leaf $Work) -notmatch '^sfe-update-[0-9a-f]+$' -or
        $Work -eq $Target -or $Work.StartsWith($Target + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $Target.StartsWith($Work + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Unsafe update workspace.'
    }
    Assert-NoLinks $Work
    Assert-NoLinks $Target
    $exe = Child-Path $Target 'starfox_pc.exe'
    if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Installed game executable is missing.' }
    # Do not replace a developer's build or a different application's folder.
    # The player also rejects any build whose ancestor is a source checkout.
    if (Test-Path -LiteralPath (Join-Path $Target '.git')) { throw 'Source checkout: install updates manually.' }
    if ($Mode -eq 'Apply') {
        if ($ParentId -le 0 -or $ParentStartTicks -le 0) { throw 'Missing parent process identity.' }
        $parentProcess = Get-Process -Id $ParentId -ErrorAction SilentlyContinue
        if ($null -ne $parentProcess -and
            ($parentProcess.StartTime.ToUniversalTime().Ticks -ne $ParentStartTicks -or
             ![string]::Equals($parentProcess.Path, $exe, [StringComparison]::OrdinalIgnoreCase))) {
            throw 'Game process identity changed; refusing to replace files.'
        }
        # The game must see this receipt before it agrees to exit. A blocked
        # helper launch or policy failure must not silently close the player.
        [IO.File]::WriteAllText((Child-Path $Work 'handoff-ready'), 'ready')
    }
    $archivePath = Child-Path $Work 'payload.zip'
    if ($Bytes -le 0 -or $Bytes -gt 4GB -or
        (Get-Item -LiteralPath $archivePath).Length -ne $Bytes -or
        (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $Digest) {
        throw 'Update checksum or file size mismatch. Nothing was installed.'
    }
    $stage = Child-Path $Work 'stage'
    if ($Mode -eq 'Prepare') {
        if (Test-Path -LiteralPath $stage) { throw 'Staging directory already exists.' }
        [IO.Directory]::CreateDirectory($stage) | Out-Null
        $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
        try {
            $names = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            $files = New-Object 'Collections.Generic.List[object]'
            [long]$expanded = 0
            if ($archive.Entries.Count -gt 20000) { throw 'Update contains too many files.' }
            # Validate ALL entries before extraction, including skipped paths.
            foreach ($entry in $archive.Entries) {
                $name = $entry.FullName
                if (!(Safe-Name $name) -or
                    !$names.Add($name.TrimEnd('/')) -or
                    (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000 -or
                    ($entry.ExternalAttributes -band 0x400) -ne 0) {
                    throw 'Unsafe or duplicate path in update ZIP.'
                }
                if ($entry.Length -lt 0 -or $entry.Length -gt 8GB - $expanded) { throw 'Update exceeds the extraction limit.' }
                $expanded += $entry.Length
                if ($name.EndsWith('/')) { continue }
                if (Package-File $name) { $files.Add($entry) }
            }
            $drive = New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Work))
            if ($drive.AvailableFreeSpace -lt $expanded + 256MB) { throw 'Not enough space to stage the update.' }
            $targetDrive = New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Target))
            if ($targetDrive.AvailableFreeSpace -lt 2 * $expanded + 256MB) { throw 'Not enough space for update rollback.' }
            if (!$names.Contains('starfox_pc.exe')) { throw 'Wrong update package (game executable missing).' }
            $buffer = New-Object byte[] 65536
            foreach ($entry in $files) {
                $destination = Child-Path $stage $entry.FullName
                [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
                $inputStream = $entry.Open()
                $outputStream = $null
                try {
                    $outputStream = [IO.File]::Open($destination, [IO.FileMode]::CreateNew)
                    [long]$actual = 0
                    while (($count = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                        $actual += $count
                        if ($actual -gt $entry.Length) { throw 'Expanded ZIP size does not match its directory.' }
                        $outputStream.Write($buffer, 0, $count)
                    }
                    if ($actual -ne $entry.Length) { throw 'Truncated ZIP entry.' }
                } finally {
                    if ($null -ne $outputStream) { $outputStream.Dispose() }
                    $inputStream.Dispose()
                }
            }
        } finally { $archive.Dispose() }
        if ((Machine $exe) -ne (Machine (Child-Path $stage 'starfox_pc.exe'))) { throw 'Update CPU architecture does not match.' }
        $manifest = @(Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
            @{ name = $_.FullName.Substring($stage.Length + 1).Replace('\','/');
               hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        })
        ConvertTo-Json -InputObject @($manifest) -Depth 4 | Set-Content -LiteralPath (Child-Path $Work 'prepared.json') -Encoding UTF8
        exit 0
    }

    if ($ParentId -le 0 -or $ParentStartTicks -le 0) { throw 'Missing parent process identity.' }
    $parentProcess = Get-Process -Id $ParentId -ErrorAction SilentlyContinue
    if ($null -ne $parentProcess) {
        if ($parentProcess.StartTime.ToUniversalTime().Ticks -ne $ParentStartTicks -or
            ![string]::Equals($parentProcess.Path, $exe, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Game process identity changed; refusing to replace files.'
        }
        if (!$parentProcess.WaitForExit(120000)) { throw 'Game has not exited. No files were replaced.' }
    }
    Assert-NoLinks $stage
    $manifest = ConvertFrom-Json -InputObject ([IO.File]::ReadAllText((Child-Path $Work 'prepared.json')))
    if ($manifest -isnot [Array] -or $manifest.Count -eq 0 -or $manifest.Count -gt 20000) { throw 'Invalid update inventory.' }
    [long]$backupBytes = 0
    $names = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $manifest) {
        if (!(Safe-Name $file.name) -or !(Package-File $file.name) -or !$names.Add($file.name)) { throw 'Invalid staged update file.' }
        $source = Child-Path $stage $file.name
        Assert-NoLinks $source
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.hash) { throw 'Staged update was altered.' }
        $destination = Child-Path $Target $file.name
        $existing = $destination
        while (!(Test-Path -LiteralPath $existing)) { $existing = Split-Path -Parent $existing }
        Assert-NoLinks $existing
        if (Test-Path -LiteralPath $destination -PathType Leaf) { $backupBytes += (Get-Item -LiteralPath $destination).Length }
    }
    if (!$names.Contains('starfox_pc.exe')) { throw 'Incomplete staged update.' }
    # The backup lives on the target volume so each replacement is a rename,
    # not a partial cross-volume copy. Never delete a pre-existing backup.
    $backup = Child-Path $Target ((Split-Path -Leaf $Work) + '-rollback')
    if (Test-Path -LiteralPath $backup) { throw 'Rollback directory already exists.' }
    $drive = New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Target))
    $stageBytes = (Get-ChildItem -LiteralPath $stage -File -Recurse | Measure-Object Length -Sum).Sum
    if ($drive.AvailableFreeSpace -lt $stageBytes + $backupBytes + 256MB) { throw 'Not enough space for a safe update.' }
    [IO.Directory]::CreateDirectory($backup) | Out-Null
    # First copy all new files into the rollback directory on the same volume.
    # Old files are not touched until this completes and all hashes match.
    foreach ($file in $manifest) {
        $incoming = Child-Path $backup ('incoming/' + $file.name)
        [IO.Directory]::CreateDirectory((Split-Path -Parent $incoming)) | Out-Null
        Copy-Item -LiteralPath (Child-Path $stage $file.name) -Destination $incoming
        if ((Get-FileHash -LiteralPath $incoming -Algorithm SHA256).Hash -ne $file.hash) { throw 'Update copy failed verification.' }
    }
    foreach ($file in $manifest) {
        $destination = Child-Path $Target $file.name
        $old = Child-Path $backup ('original/' + $file.name)
        [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        [IO.Directory]::CreateDirectory((Split-Path -Parent $old)) | Out-Null
        # Record before moving: rollback also handles a failure between renames.
        $replaced.Add($file.name)
        if (Test-Path -LiteralPath $destination) { Move-Item -LiteralPath $destination -Destination $old }
        Move-Item -LiteralPath (Child-Path $backup ('incoming/' + $file.name)) -Destination $destination
    }
    Start-Process -FilePath $exe -WorkingDirectory $Target | Out-Null
    # These absolute directories were created and validated above. No glob,
    # source checkout, application folder or user-data directory is removed.
    try {
        Remove-Item -LiteralPath $backup -Recurse -Force
        $backup = $null
        Remove-Item -LiteralPath $Work -Recurse -Force
    } catch { Write-Warning 'Update installed; temporary files could not be removed.' }
} catch {
    $failure = $_.Exception.Message
    $rollbackFailed = $false
    $rollbackOperations = 0
    if ($null -ne $backup -and $replaced.Count -gt 0) {
        for ($i = $replaced.Count - 1; $i -ge 0; --$i) {
            try {
                $destination = Child-Path $Target $replaced[$i]
                $old = Child-Path $backup ('original/' + $replaced[$i])
                if (Test-Path -LiteralPath $old) {
                    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
                    Move-Item -LiteralPath $old -Destination $destination
                    ++$rollbackOperations
                } elseif (!(Test-Path -LiteralPath (Child-Path $backup ('incoming/' + $replaced[$i])))) {
                    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force; ++$rollbackOperations }
                }
            } catch { $rollbackFailed = $true }
        }
    }
    if ($rollbackFailed) { $failure += " Rollback needs attention; keep the backup at $backup." }
    if ($rollbackOperations -gt 0) { $failure += " Rollback restored $rollbackOperations file operations." }
    try { $failure | Set-Content -LiteralPath (Child-Path $Work 'error.txt') -Encoding UTF8 } catch {}
    if ($Mode -eq 'Apply') {
        Add-Type -AssemblyName System.Windows.Forms
        [Windows.Forms.MessageBox]::Show("Update could not be installed: $failure`nDetails: $Work", 'Star Fox Enhanced update') | Out-Null
    }
    Write-Error $failure -ErrorAction Continue
    exit 1
}
