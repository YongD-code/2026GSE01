param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Original', 'Refined')]
    [string]$Variant
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$original = Join-Path $PSScriptRoot 'CharacterVersions/Original'
$refined = Join-Path $PSScriptRoot 'CharacterRefinement'
$names = @('walk.png', 'run.png', 'cast.png', 'preview.png')
$sources = @{}
foreach ($name in $names)
{
    if ($Variant -eq 'Original')
    {
        $sources[$name] = Join-Path $original $name
    }
    elseif ($name -eq 'preview.png')
    {
        $sources[$name] = Join-Path $refined 'final_perspective.png'
    }
    else
    {
        $sources[$name] = Join-Path $refined "Sprites/$name"
    }
    if (-not (Test-Path -LiteralPath $sources[$name] -PathType Leaf))
    {
        throw "Missing asset: $($sources[$name])"
    }
}

$targets = @((Join-Path $projectRoot 'SimpleGame/Assets/Characters/Blender'))
$debugAssets = Join-Path $projectRoot 'x64/Debug/Assets/Characters/Blender'
if (Test-Path -LiteralPath $debugAssets -PathType Container)
{
    $targets += $debugAssets
}
foreach ($target in $targets)
{
    foreach ($name in $names)
    {
        $destination = Join-Path $target $name
        # Replace by rename so image previews can keep their old mapped file open.
        $staged = Join-Path $target ('.' + [guid]::NewGuid().ToString('N') + '.png')
        Copy-Item -LiteralPath $sources[$name] -Destination $staged
        [System.IO.File]::Move($staged, $destination, $true)
        if ((Get-FileHash -LiteralPath $sources[$name]).Hash -ne
            (Get-FileHash -LiteralPath $destination).Hash)
        {
            throw "Asset copy verification failed: $destination"
        }
    }
}
Write-Output "Character assets selected: $Variant. Restart the game to reload textures."
