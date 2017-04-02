$file = "capstone_static.vcxproj"
Copy-Item "..\capstone\msvc\capstone_static\$file" .
(Get-Content $file).replace('<RuntimeLibrary>MultiThreaded', '<RuntimeLibrary>MultiThreadedDll').replace('CAPSTONE_HAS_ARM;', '').replace('CAPSTONE_HAS_ARM64;', '').replace('CAPSTONE_HAS_MIPS;', '').replace('CAPSTONE_HAS_POWERPC;', '').replace('CAPSTONE_HAS_SPARC;', '').replace('CAPSTONE_HAS_SYSZ;', '').replace('CAPSTONE_HAS_XCORE;', '').replace('..\..\', '..\capstone\') | Set-Content $file
