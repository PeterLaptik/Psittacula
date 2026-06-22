# ----------------------------------------------------------------
# Full clean all CMake data files and MS VS projects (optionally)
# ----------------------------------------------------------------

# Clean CMake data
$DIR_PROJECTS_PATHS = -join((Get-Item .).FullName, "/"), # Current root directory
                    "./psittacula-cli/",
                    "./psittacula-tui/",
					"./psittacula-tools/"
                    
$DIR_CMAKEFILES_DIR = "CMakeFiles"

$ROOT_FILES = "CMakeCache.txt", "cmake_install.cmake", "Build", "Makefile", "CppProperties.json"

# Remove dirs CMakeFiles
Write-Host "CMake files and directories clean..."
for ($i = 0; $i -lt $DIR_PROJECTS_PATHS.Count; $i++) {
    $dirToRemove = -join($DIR_PROJECTS_PATHS[$i], $DIR_CMAKEFILES_DIR)
    if(Test-Path -LiteralPath $dirToRemove) {
        Write-Host "Removing directory: " $dirToRemove
        rm $dirToRemove -r -Force
    }
}

# Remove files
for ($i = 0; $i -lt $DIR_PROJECTS_PATHS.Count; $i++) {
    for($j = 0; $j -lt $ROOT_FILES.Count; $j++) {
        $fileToRemove = -join($DIR_PROJECTS_PATHS[$i], $ROOT_FILES[$j])
        if(Test-Path -LiteralPath $fileToRemove) {
            Write-Host "Removing file: " $fileToRemove
            rm $fileToRemove
        }
    }
}

Write-Host "CMake files and directories have been cleaned."


# Clean Microsoft Visual Studio data

$REMOVE_VS_PROJECTS = Read-Host -Prompt "Remove Visual Studio projects files? [y/n]"
if($REMOVE_VS_PROJECTS -eq 'y') {
    for ($i = 0; $i -lt $DIR_PROJECTS_PATHS.Length; $i++) {
        if(Test-Path -LiteralPath $DIR_PROJECTS_PATHS[$i]) {
            $files = Get-ChildItem $DIR_PROJECTS_PATHS[$i] | where {$_.extension -in ".vcxproj",".user",".filters", ".sln",".dir"}
            for ($j = 0; $j -lt $files.Count; $j++) {
                $fileToRemove = $files[$j].fullname
                Write-Host "Removing file: " $fileToRemove
                rm -r -Force $fileToRemove
            }
        }
    }
        
    $VS_DIRS = ".vs", "Debug", "Release", "x64", "x86"
    for ($i = 0; $i -lt $DIR_PROJECTS_PATHS.Length; $i++) {
        for($j = 0; $j -lt $VS_DIRS.Length; $j++) {
            $VS_DIR = -join($DIR_PROJECTS_PATHS[$i], $VS_DIRS[$j])
            if(Test-Path -LiteralPath $VS_DIR) {
                Write-Host "Removing directory: " $VS_DIR
                rm -r -Force $VS_DIR
            }
        }
    }
    Write-Host "Visual Studio projects files have been cleaned."
} else {
    Write-Host "Visual Studio projects files clean skipped."
}

cmd /c 'pause'