# dicom
Tools for processing DICOM File Format images for analysis and research.


--voxelize-mean 5 5 5 --input-folder "$(SolutionDir)\test_collateral\3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012" --output-file test_collateral\test.3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.jpg

--voxelize-stddev 5 5 5 --input-folder "$(SolutionDir)\test_collateral\3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012" --output-file test_collateral\test.3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.jpg

--normalize-image --input-file test_collateral\test.3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.jpg --output-file test_collateral\test.3D_PD_SAG_0_5ISO_NOACC_#1_RR_0012.mean.normalized.jpg

