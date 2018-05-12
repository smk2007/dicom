param
(
    [string] $DCPFolder = ".\",

    [string] $OutputFolder = "SSIMOutput",

    [Parameter(Mandatory=$true)]
    [string] $RootFolder = "C:\Users\Administrator\Downloads\cases",

    [uint32] $VoxelSize = 5,

    [switch] $Force
)

if (!(Test-Path $DCPFolder -PathType Container))
{
    Write-Host "Path not found: " $DCPFolder;
}

if (!(Test-Path $OutputFolder -PathType Container))
{
    New-Item -ItemType Directory -Force -Path $OutputFolder
}

$OutputFolder = (Resolve-Path $OutputFolder).Path;

if (!(Test-Path $RootFolder -PathType Container))
{
    Write-Host "Path not found: " $DCPFolder;
}

Push-Location $DCPFolder;

if (!(Test-Path "dcp.exe" -PathType Leaf))
{
    Write-Host "DCP.exe not found";
}

$inputFolder = "C:\Users\Administrator\Downloads\cases\004\3D_PD_SAG_0_5ISO_CAIPI_R4_#1_0011"
$inputFolder2 = "C:\Users\Administrator\Downloads\cases\004\3D_PD_SAG_0_5ISO_CS_R4_#1_0006"
$ssimFile = "$OutputFolder\ssim.dd"
$ssimFileNormalized = "$OutputFolder\ssim.normalized.dd"
$ssimCSV = "$OutputFolder\ssim.csv"

if ($Force -or
    !(Test-Path $ssimFile -PathType Leaf))
{
    .\dcp.exe --voxelize-ssim $VoxelSize $VoxelSize $VoxelSize --input-folder $inputFolder --input-folder2 $inputFolder2 --output-file $ssimFile 
}
if ($Force -or !(Test-Path $ssimFileNormalized -PathType Leaf))
{
    .\dcp.exe --normalize-image --input-file $ssimFile --output-file $ssimFileNormalized;
}
if ($Force -or !(Test-Path $ssimCSV -PathType Leaf))
{
    .\dcp.exe --image-convert-to-csv --input-file $ssimFile --output-file $ssimCSV;
}