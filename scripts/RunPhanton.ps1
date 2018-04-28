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
            Write-Output "`nProcessing $($_.FullName)"; `
            if (!(Test-Path "$OutputFolder\$($_.Name)\" -PathType Container)) `
            { `
                New-Item -ItemType Directory -Force -Path "$OutputFolder\$($_.Name)\" `
            } `
            .\dcp.exe --voxelize-mean   $VoxelSize $VoxelSize $VoxelSize --input-folder $_.FullName --output-file "$OutputFolder\$($_.Name)\$($_.Name).mean.jpg"; `
            .\dcp.exe --voxelize-stddev $VoxelSize $VoxelSize $VoxelSize --input-folder $_.FullName --output-file "$OutputFolder\$($_.Name)\$($_.Name).stddev.jpg"; `
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
                               $signalFile = "$OutputFolder\AVERAGE_$runNFileName\$runNFileName.mean.avg.jpg"; `
                               $snrFile = "$OutputFolder\$runNFileName.snr.raw.jpg"; `
                               $snrFileNormalized = "$OutputFolder\$($_.Name).snr.normalized.jpg"; `
                               $snrCSV = "$OutputFolder\$($_.Name).snr.csv"; `
                               $runId = [int]::Parse($runN.Substring($runN.Length - 6, 4)); `
                               $noiseRunId = ($runId + 1).ToString("D4"); `
                               $noisePrefix = $groupName+"_SUB_$noiseRunId"; `
                               $noiseFile = "$OutputFolder\$noisePrefix\$noisePrefix.stddev.jpg"; `
                               if (!(Test-Path $signalFolder -PathType Container)) `
                               { `
                                   New-Item -ItemType Directory -Force -Path $signalFolder `
                               } `
                               Copy-Item $run0 -Filter *mean.jpg -Recurse -Destination $signalFolder; `
                               Copy-Item $runN -Filter *mean.jpg -Recurse -Destination $signalFolder; `
                               .\dcp.exe --image-average --input-folder $signalFolder --output-file $signalFile; `
                               .\dcp.exe --image-snr $signalFile $noiseFile --output-file $snrFile; `
                               .\dcp.exe --normalize-image --input-file $snrFile --output-file $snrFileNormalized; `
                               .\dcp.exe --image-convert-to-csv --input-file $snrFile --output-file $snrCSV `
                           }; `
           }


#dcp.exe --normalize-image                                    --input-file     "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc\3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.jpg"    --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\normalized\3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.normalized.jpg"
#dcp.exe --voxelize-mean 5 5 5     --input-folder "C:\dev\dcm\test_collateral\3D_PD_SAG_0_5ISO_NOACC_#2_RR_0013"                                                    --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc\3D_PD_SAG_0_5ISO_NOACC_#2_RR_0013.mean.jpg"
#dcp.exe --normalize-image          --input-file     "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc\3D_PD_SAG_0_5ISO_NOACC_#2_RR_0013.mean.jpg"    --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\normalized\3D_PD_SAG_0_5ISO_NOACC_#2_RR_0013.mean.normalized.jpg"
#dcp.exe --image-average             --input-folder "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc"                                                                                  --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc_avg\3D_PD_SAG_0_5ISO_NOACC.mean.avg.jpg"
#dcp.exe --normalize-image          --input-file     "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc_avg\3D_PD_SAG_0_5ISO_NOACC.mean.avg.jpg"           --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\normalized\3D_PD_SAG_0_5ISO_NOACC.mean.normalized.jpg"
#dcp.exe --image-convert-to-csv   --input-file     "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc_avg\3D_PD_SAG_0_5ISO_NOACC.mean.avg.jpg"           --output-file "C:\dev\dcm\Debug\dcp_test\p12\555\raw\noacc_avg\3D_PD_SAG_0_5ISO_NOACC.mean.avg.csv"



Pop-Location

