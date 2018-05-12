param
(
    [string] $DCPFolder = ".\",

    [string] $OutputFolder = "KneeOutput",

    [Parameter(Mandatory=$true)]
    [string] $RootFolder = "C:\Users\Administrator\Downloads\test cases",

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

Get-ChildItem -Path $RootFolder -Directory |
    ForEach-Object { # For each patient
        $patientFolder = $_.Name;
        Get-ChildItem -Path $_.FullName -Directory |
            ForEach-Object { # For each run
                $runFolder = $_.Name;

                if (-not $runFolder.StartsWith("SUB"))
                {
                    $runFolder = "____$runFolder";
                }

                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_CAIPI_R4",  "_CAIPI"; 
                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_CAIPI",  "_CAIPI"; 
                $runFolder = $runFolder -replace "3D_PD_CAIPI_R4",  "_CAIPI"; 
                $runFolder = $runFolder -replace "SUB_CAIPI",  "SUB__CAIPI"; 
                
                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_CS_R4",     "____CS"; 
                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_CS",     "____CS";
                $runFolder = $runFolder -replace "3D_PD_CS",     "____CS"; 
                $runFolder = $runFolder -replace "SUB_CS",  "SUB_____CS"; 

                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_GRAPPA_R4", "GRAPPA"; 
                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_GRAPPA", "GRAPPA";  
                $runFolder = $runFolder -replace "3D_PD_GRAPPA_R4", "GRAPPA";  
                
                $runFolder = $runFolder -replace "3D_PD_SAG_0_5ISO_NOACC",     "_NOACC"; 
                $runFolder = $runFolder -replace "3D_PD_NOACC",     "_NOACC"; 
                $runFolder = $runFolder -replace "SUB_NOACC",     "SUB__NOACC"; 
                $runFolder = $runFolder -replace "SUB_NO_ACC",     "SUB__NOACC"; 


                Write-Output "Voxelizing:      [Patient $patientFolder] $runFolder";

                $meansFile = "$OutputFolder\$patientFolder.$runFolder.mean.dd";
                $stddevFile = "$OutputFolder\$patientFolder.$runFolder.stddev.dd";
                $meansFileNormalized = "$OutputFolder\$patientFolder.$runFolder.mean.jpg";
                $stddevFileNormalized = "$OutputFolder\$patientFolder.$runFolder.stddev.jpg";
                $meansCSV = "$OutputFolder\$patientFolder.$runFolder.mean.csv";
                $stddevCSV = "$OutputFolder\$patientFolder.$runFolder.stddev.csv";

                if ($Force -or
                    !(Test-Path $meansFile -PathType Leaf) -or
                    !(Test-Path $stddevFile -PathType Leaf))
                {
                    .\dcp.exe --voxelize-stddev $VoxelSize $VoxelSize $VoxelSize --input-folder $_.FullName --output-file $stddevFile --output-file2 $meansFile;
                }
                if ($Force -or !(Test-Path $meansFileNormalized -PathType Leaf))
                {
                    .\dcp.exe --normalize-image --input-file $meansFile --output-file $meansFileNormalized;
                }
                if ($Force -or !(Test-Path $stddevFileNormalized -PathType Leaf))
                {
                    .\dcp.exe --normalize-image --input-file $stddevFile --output-file $stddevFileNormalized;
                }
                if ($Force -or !(Test-Path $meansCSV -PathType Leaf))
                {
                    .\dcp.exe --image-convert-to-csv --input-file $meansFile --output-file $meansCSV;
                }
                if ($Force -or !(Test-Path $stddevCSV -PathType Leaf))
                {
                    .\dcp.exe --image-convert-to-csv --input-file $stddevFile --output-file $stddevCSV;
                }
            }
    };

$means =
    Get-ChildItem -Path $OutputFolder -File -Filter *mean.dd |
        Where-Object Name -CNotLike "*.SUB_*" |
            Group-Object -Property { $_.Name.Substring(0, 14) };

$means |
    ForEach-Object {
        $signalFolder = "$OutputFolder\$($_.Name)_AVERAGE";
        $signalFile = "$OutputFolder\$($_.Name)_AVERAGE.dd";
        $snrFile = "$OutputFolder\$($_.Name).snr.dd";
        $snrFileNormalized = "$OutputFolder\$($_.Name).snr.jpg";
        $snrCSV = "$OutputFolder\$($_.Name).snr.csv";

        $noiseFilter = $_.Name -replace "^[0-9]{3}\..{3}", [string]::Concat($_.Name.Substring(0,3), ".SUB");
        $noiseFilter += "*.stddev.dd";        
        $noiseFile = Get-ChildItem -Path $OutputFolder -File | Where-Object Name -CLike $noiseFilter | Select-Object FullName;
        $patientId = $_.Name.Substring(0,3);
        Write-Output "Compute SNR:     [Patient $patientId] $snrFile : $($_.Group) $($noiseFile.FullName)";

        if (!(Test-Path $signalFolder -PathType Container))
        {
            $dir = New-Item -ItemType Directory -Force -Path $signalFolder;
        }

        $_.Group | ForEach-Object { Copy-Item $_.FullName $signalFolder };

        if ($Force -or !(Test-Path $signalFile -PathType Leaf))
        {
            .\dcp.exe --image-average --input-folder $signalFolder --output-file $signalFile;
        }

        if ($Force -or !(Test-Path $snrFile -PathType Leaf))
        {
            .\dcp.exe --image-snr $signalFile $noiseFile.FullName --output-file $snrFile;
        }
        if ($Force -or !(Test-Path $snrFileNormalized -PathType Leaf))
        {           
            .\dcp.exe --normalize-image --input-file $snrFile --output-file $snrFileNormalized;
        }
        if ($Force -or !(Test-Path $snrCSV -PathType Leaf))
        {
            .\dcp.exe --image-convert-to-csv --input-file $snrFile --output-file $snrCSV;
        }
    };

function GetRValueForTechnique
{
    param
    (
        [string] $Technique
    )

    if ($Technique -eq "_____CAIPI")
    {
        return 3.9;
    }
    if ($Technique -eq "________CS")
    {
        return 4.4;
    }
    if ($Technique -eq "____GRAPPA")
    {
        return 3.9;        
    }

    throw;
}


Get-ChildItem -Path $OutputFolder -File -Filter *snr.dd |
    Where-Object Name -CNotLike "*NOACC.snr.dd" |
        ForEach-Object {
            $patientId = $_.Name.Substring(0,3);
            $noaccSignal = [string]::Concat($patientId, "._____NOACC.snr.dd");
            $noaccSignal = [string]::Concat("\", $noaccSignal);
            $noaccSignal = [string]::Concat($OutputFolder, $noaccSignal);
            $techniqueSignal = $_.FullName;

            $rvalue = GetRValueForTechnique($_.Name.Substring(4, 10));

            $base = $_.Name.Substring(0, 14);
            $base = [string]::Concat("\", $base);
            $base = [string]::Concat($OutputFolder, $base);
            
            $gfactorFile = "$base.gfactor.dd"; 
            $gfactorFileNormalized = "$base.gfactor.jpg";
            $gfactorCSV = "$base.gfactor.csv";
        
            Write-Output "Compute GFACTOR: [Patiend $patientId] $gfactorFile : $techniqueSignal $noaccSignal with rvalue=$rvalue";
        
            if ($Force -or !(Test-Path $gfactorFile -PathType Leaf))
            {
                .\dcp.exe --image-gfactor $techniqueSignal $noaccSignal --output-file $gfactorFile --gfactor-rvalue $rvalue;
            }
            if ($Force -or !(Test-Path $gfactorFileNormalized -PathType Leaf))
            {
                .\dcp.exe --normalize-image --input-file $gfactorFile --output-file $gfactorFileNormalized;
            }
            if ($Force -or !(Test-Path $gfactorCSV -PathType Leaf))
            {
                .\dcp.exe --image-convert-to-csv --input-file $gfactorFile --output-file $gfactorCSV;
            }
        };

Pop-Location
