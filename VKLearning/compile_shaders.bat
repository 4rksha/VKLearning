set ShaderPath=Shaders
set SPVPath=..\CompiledShaders
echo "Current Path: " %cd%
@echo on
for %%f in (%ShaderPath%\*) do (
    C:\VulkanSDK\1.2.198.1\Bin\glslc.exe "%ShaderPath%\%%~nxf" -o "%SPVPath%\%%~nxf.spv"
)