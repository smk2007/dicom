param
(
    [string] $DCPFolder = ".\",

    [Parameter(Mandatory=$true)]
    [string] $OutputFolder,

    [Parameter(Mandatory=$true)]
    [string] $RootFolder,

    [uint32] $VoxelSize = 5
)

if (!(Test-Path $DCPFolder -PathType Container))
{
    Write-Host "Path not found: " $DCPFolder;
}

if (!(Test-Path $OutputFolder -PathType Container))
{
    New-Item -ItemType Directory -Force -Path $OutputFolder
}

if (!(Test-Path $RootFolder -PathType Container))
{
    Write-Host "Path not found: " $DCPFolder;
}

Push-Location $DCPFolder;

if (!(Test-Path "dcp.exe" -PathType Leaf))
{
    Write-Host "DCP.exe not found";
}

Get-ChildItem -Path $RootFolder -Directory | `
    Where-Object Name -CNotLike "HASTE*" | `
        ForEach-Object { `
            Write-Output "Voxelizing $($_.FullName)"; `
            if (!(Test-Path "$OutputFolder\$($_.Name)\" -PathType Container)) `
            { `
                New-Item -ItemType Directory -Force -Path "$OutputFolder\$($_.Name)\"; `
            } `
            .\dcp.exe --voxelize-mean   $VoxelSize $VoxelSize $VoxelSize --input-folder $_.FullName --output-file "$OutputFolder\$($_.Name)\$($_.Name).mean.dd"; `
            .\dcp.exe --voxelize-stddev $VoxelSize $VoxelSize $VoxelSize --input-folder $_.FullName --output-file "$OutputFolder\$($_.Name)\$($_.Name).stddev.dd"; `
        }

Get-ChildItem -Path $OutputFolder -Directory | `
   Group-Object -Property { $_.Name.Substring(0, $_.Name.Length - 5) } | `
       Where-Object Count -EQ 4 | `
           ForEach-Object { `
               $groupName = $_.Name; `
               $first = $_.Group[0]; `
               Write-Host "$($first.FullName)`n"
               $_.Group | `
                   Select-Object -skip 1 | `
                           ForEach-Object { `
                               $run0 = "$($first.FullName)\*"; `
                               $runN = "$($_.FullName)\*"; `
                               $runNFileName = "$($_.Name)"; `
                               $signalFolder = "$OutputFolder\AVERAGE_$runNFileName"; `
                               $signalFile = "$OutputFolder\AVERAGE_$runNFileName\$runNFileName.mean.avg.dd"; `
                               $snrFile = "$OutputFolder\$runNFileName.snr.raw.dd"; `
                               $snrFileNormalized = "$OutputFolder\$($_.Name).snr.normalized.jpg"; `
                               $snrCSV = "$OutputFolder\$($_.Name).snr.csv"; `
                               $runId = [int]::Parse($runN.Substring($runN.Length - 6, 4)); `
                               $noiseRunId = ($runId + 1).ToString("D4"); `
                               $noisePrefix = $groupName+"_SUB_$noiseRunId"; `
                               $noiseFile = "$OutputFolder\$noisePrefix\$noisePrefix.stddev.dd"; `
                               if (!(Test-Path $signalFolder -PathType Container)) `
                               { `
                                   New-Item -ItemType Directory -Force -Path $signalFolder `
                               } `
                               Copy-Item $run0 -Filter *mean.dd -Recurse -Destination $signalFolder; `
                               Copy-Item $runN -Filter *mean.dd -Recurse -Destination $signalFolder; `
                               Write-Output "SNR: $snrFile"; `
                               .\dcp.exe --image-average --input-folder $signalFolder --output-file $signalFile; `
                               .\dcp.exe --image-snr $signalFile $noiseFile --output-file $snrFile; `
                               .\dcp.exe --normalize-image --input-file $snrFile --output-file $snrFileNormalized; `
                               .\dcp.exe --image-convert-to-csv --input-file $snrFile --output-file $snrCSV `
                           }; `
           }


$accGroup = Get-ChildItem $OutputFolder -Filter *snr.raw.dd -File | `
               Where-Object Name -CLike "GRAPPA_R4_ACC_X4*";
$noaccGroup = Get-ChildItem $OutputFolder -Filter *snr.raw.dd -File | `
                Where-Object Name -CLike "NOACC_ACC_BASE*";

$sosGroup = Get-ChildItem $OutputFolder -Filter *snr.raw.dd -File | `
                Where-Object Name -CLike "GRAPPA_R4_SOS_BASE_X4*";
$noaccSOSGroup = Get-ChildItem $OutputFolder -Filter *snr.raw.dd -File | `
                Where-Object Name -CLike "NOACC_SOS_BASE_X4*";

function ComputeGFactor
{
    param
    (
        [string] $techniqueSignal,
        [string] $noaccSignal
    )

    $gfactorFile = "$techniqueSignal.gfactor.raw.dd"; 
    $gfactorFileNormalized = "$techniqueSignal.gfactor.normalized.jpg";
    $gfactorCSV = "$techniqueSignal.gfactor.csv";

    Write-Output "GFACTOR: $gfactorFile";

    .\dcp.exe --image-gfactor $techniqueSignal $noaccSignal --output-file $gfactorFile --gfactor-rvalue 4;
    .\dcp.exe --normalize-image --input-file $gfactorFile --output-file $gfactorFileNormalized;
    .\dcp.exe --image-convert-to-csv --input-file $gfactorFile --output-file $gfactorCSV;
}

for($index = 0; $index -lt $accGroup.Length; $index++)
{
    $techniqueSignal = $accGroup[$index].FullName;
    $noaccSignal = $noaccGroup[$index].FullName;
    
    ComputeGFactor -techniqueSignal $techniqueSignal -noaccSignal $noaccSignal;
}

for($index = 0; $index -lt $sosGroup.Length; $index++)
{
    Write-Output $sosGroup[$index].FullName;
    $techniqueSignal = $sosGroup[$index].FullName;
    $noaccSignal = $noaccSOSGroup[$index].FullName;
    ComputeGFactor -techniqueSignal $techniqueSignal -noaccSignal $noaccSignal;
}

Pop-Location

