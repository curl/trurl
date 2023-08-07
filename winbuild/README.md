<!--
Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.

SPDX-License-Identifier: curl
-->

# Building trurl with Microsoft C++ Build Tools

Download and install [Microsoft C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)

When installing, choose the `Desktop development with C++` option.

## Open a command prompt

Open the **x64 Native Tools Command Prompt for VS 2022**, or if you are on an x86 platform **x86 Native Tools Command Prompt for VS 2022**

## Set up the vcpkg repository

Note: The location of the vcpkg repository does not necessarily need to correspond to the trurl directory, it can be set up anywhere. But it is recommended to use a short path such as `C:\src\vcpkg` or `C:\dev\vcpkg`, since otherwise you may run into path issues for some port build systems.

Once you are in the console, run the below commands to clone the vcpkg repository and set up the curl dependencies:

~~~
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install curl:x86-windows-static-md
.\vcpkg\vcpkg install curl:x64-windows-static-md
.\vcpkg\vcpkg integrate install
~~~

Once the vcpkg repository is set up you do not need to run these commands again. If a newer version of curl is released, you may need to run `git pull` in the vcpkg repository and then `vcpkg upgrade` to fetch the new version.

## Build in the console

Once the vcpkg repository and dependencies are set up, go to the winbuild directory in the trurl sources:

    cd trurl\winbuild

Then you can call the build command with the desired parameters. The builds will be placed in an output directory as described below.

## Parameters

 - The `Configuration` parameter can be set to either `Debug` or `Release`
 - The `Platform` parameter can be set to either `x86` or `x64`

## Build commands

 - x64 Debug: `msbuild /m /t:Clean,Build /p:Configuration=Debug /p:Platform=x64 trurl.sln`
 - x64 Release: `msbuild /m /t:Clean,Build /p:Configuration=Release /p:Platform=x64 trurl.sln`
 - x86 Debug: `msbuild /m /t:Clean,Build /p:Configuration=Debug /p:Platform=x86 trurl.sln`
 - x86 Release: `msbuild /m /t:Clean,Build /p:Configuration=Release /p:Platform=x86 trurl.sln`

Note: If you are using the x64 Native Tools Command Prompt you can also run the x86 build commands.

## Output directories

The output files will be placed in: `winbuild\bin\<Platform>\<Configuration>\`
PDB files will be generated in the same directory as the executable for Debug builds, but they will not be generated for release builds.

Intermediate files will be placed in: `winbuild\obj\<Platform>\<Configuration>\`
These include build logs and obj files.

## Tests

Tests can be run by going to the directory of the output files in the console and running `perl .\..\..\..\..\test.pl`
You will need perl installed to run the tests, such as [Strawberry Perl](https://strawberryperl.com/)
