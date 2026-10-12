function Select-CiOwnedSnapshot {
    param([object[]]$Snapshot,[int]$RootId,[datetime]$RootBirth)
    $ciById=@{}
    foreach($ciRecord in $Snapshot){
        $ciId=[int]$ciRecord.ProcessId
        if($ciById.ContainsKey($ciId)){throw 'Duplicate process identity in snapshot'}
        $ciById[$ciId]=$ciRecord
    }
    if(!$ciById.ContainsKey($RootId) -or !$ciById[$RootId].CreationDate){throw 'Owned compiler missing from snapshot'}
    $ciRootTicks=$RootBirth.ToUniversalTime().Ticks
    if([Math]::Abs($ciById[$RootId].CreationDate.ToUniversalTime().Ticks-$ciRootTicks) -gt 9){throw 'Owned compiler snapshot birth changed'}
    $ciSelected=@{};$ciSelected[$RootId]=$ciById[$RootId]
    $ciIgnoredOlder=0
    $ciChanged=$true
    while($ciChanged){
        $ciChanged=$false
        foreach($ciRecord in $Snapshot){
            $ciId=[int]$ciRecord.ProcessId;$ciParentId=[int]$ciRecord.ParentProcessId
            if($ciSelected.ContainsKey($ciId) -or !$ciSelected.ContainsKey($ciParentId)){continue}
            if(!$ciRecord.CreationDate){throw 'Cannot authenticate descendant birth'}
            # ParentProcessId outlives the parent. After Windows recycles that
            # PID, an older unrelated child is not in the current owner tree.
            if($ciRecord.CreationDate.ToUniversalTime().Ticks -lt $ciSelected[$ciParentId].CreationDate.ToUniversalTime().Ticks){
                ++$ciIgnoredOlder
                continue
            }
            $ciSelected[$ciId]=$ciRecord;$ciChanged=$true
        }
    }
    [pscustomobject]@{Members=@($ciSelected.Values);IgnoredOlder=$ciIgnoredOlder}
}

function Read-CiOwnedCompilerResources {
    param([Diagnostics.Process]$Native,[datetime]$Birth,[string]$Program)
    $Native.Refresh()
    if($Native.HasExited){return [pscustomobject]@{Completed=$true;Private=0L;Workers=@();IgnoredRecycled=0}}
    if($Native.StartTime.ToUniversalTime().Ticks -ne $Birth.ToUniversalTime().Ticks -or $Native.Path -ine $Program){throw 'Owned compiler handle identity changed'}
    $ciSnapshot=@(Get-CimInstance Win32_Process)
    $Native.Refresh()
    if($Native.HasExited){return [pscustomobject]@{Completed=$true;Private=0L;Workers=@();IgnoredRecycled=0}}
    $ciTree=Select-CiOwnedSnapshot -Snapshot $ciSnapshot -RootId $Native.Id -RootBirth $Birth
    $ciPrivate=0L;$ciWorkers=@();$ciIgnored=$ciTree.IgnoredOlder
    foreach($ciRecord in $ciTree.Members){
        if([int]$ciRecord.ProcessId -eq $Native.Id){$ciLive=$Native}else{
            $ciLive=Get-Process -Id $ciRecord.ProcessId -ErrorAction SilentlyContinue
            if(!$ciLive){continue}
        }
        try {
            $ciLive.Refresh()
            if($ciLive.HasExited){continue}
            $ciLiveBirth=$ciLive.StartTime
            if(!$ciLiveBirth){throw 'Cannot read selected process birth'}
            if([Math]::Abs($ciLiveBirth.ToUniversalTime().Ticks-$ciRecord.CreationDate.ToUniversalTime().Ticks) -gt 9){
                if($ciLive.Id -eq $Native.Id){throw 'Owned compiler birth changed after snapshot'}
                ++$ciIgnored;continue
            }
            if(!$ciLive.Path -or !$ciRecord.ExecutablePath){throw 'Cannot authenticate selected process image'}
            if($ciLive.Path -ine [IO.Path]::GetFullPath($ciRecord.ExecutablePath)){
                if($ciLive.Id -eq $Native.Id){throw 'Owned compiler image changed after snapshot'}
                ++$ciIgnored;continue
            }
            $ciBytes=[long]$ciLive.PrivateMemorySize64;$ciPrivate+=$ciBytes
            $ciWorkers+=,[ordered]@{pid=$ciLive.Id;birth_utc=$ciLiveBirth.ToUniversalTime().ToString('o');name=$ciLive.ProcessName;private_bytes=$ciBytes;
                source_names=@([regex]::Matches($ciRecord.CommandLine,'[A-Za-z0-9_]+\.(?:cpp|c|hlsl)\b')|ForEach-Object {$_.Value}|Sort-Object -Unique)}
        } finally {
            if($ciLive.Id -ne $Native.Id){$ciLive.Dispose()}
        }
    }
    $Native.Refresh()
    [pscustomobject]@{Completed=$Native.HasExited;Private=$ciPrivate;Workers=$ciWorkers;IgnoredRecycled=$ciIgnored}
}

