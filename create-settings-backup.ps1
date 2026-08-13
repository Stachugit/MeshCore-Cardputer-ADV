param(
    [Parameter(Mandatory = $true)]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
public static class MeshCoreCrc32
{
    public static uint Compute(byte[] data)
    {
        uint crc = 0xFFFFFFFFu;
        foreach (byte value in data)
        {
            crc ^= value;
            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc >> 1) ^ (0xEDB88320u & (uint)-(int)(crc & 1u));
            }
        }
        return ~crc;
    }
}
'@

$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$payload = [System.IO.File]::ReadAllBytes($sourcePath)
if ($payload.Length -lt 1 -or $payload.Length -gt 512) {
    throw "Settings payload must contain between 1 and 512 bytes; found $($payload.Length)."
}

$destinationPath = [System.IO.Path]::GetFullPath($Destination)
if ([System.IO.File]::Exists($destinationPath)) {
    throw "Refusing to overwrite existing backup: $destinationPath"
}

$stream = [System.IO.File]::Open($destinationPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write)
try {
    $magic = [byte[]](0x4D, 0x43, 0x50, 0x53) # MCPS
    $version = [System.BitConverter]::GetBytes([uint16]1)
    $length = [System.BitConverter]::GetBytes([uint16]$payload.Length)
    $crc = [System.BitConverter]::GetBytes([MeshCoreCrc32]::Compute($payload))

    $stream.Write($magic, 0, $magic.Length)
    $stream.Write($version, 0, $version.Length)
    $stream.Write($length, 0, $length.Length)
    $stream.Write($crc, 0, $crc.Length)
    $stream.Write($payload, 0, $payload.Length)
    $stream.Flush($true)
}
finally {
    $stream.Dispose()
}

[pscustomobject]@{
    SourceBytes = $payload.Length
    BackupBytes = (Get-Item -LiteralPath $destinationPath).Length
    Crc32 = ('{0:X8}' -f [MeshCoreCrc32]::Compute($payload))
    Destination = $destinationPath
}
