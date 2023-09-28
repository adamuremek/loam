# GDNet
GDNet is a networking module for Godot 4.0 designed specifically for MMO network architecture.

## Windows Quick Start
### Prerequisites
- Clone the [Godot Source](https://github.com/godotengine/godot) repository onto your machine (ideally a Godot 4.0 branch).
- Python 3.8 or higher installed on your machine.
- The SCons python library (this can simply be installed by executing `pip install SCons`).

### Clone The GDNet Repository
After cloning the godot source project onto your machine, clone the GDNet repository into the modules folder in the godot project:

    cd <godot_source_location>\godot\modules
    
    git clone https://github.com/A-K-U-dev/GDNet.git

### Building Godot Mono From Source
When building Godot Mono from source on windows, you have to set up a directory that the SCons build system can use to store Godot related .NET items (more information [here](https://docs.godotengine.org/en/stable/contributing/development/compiling/compiling_with_dotnet.html)).
Especially with development builds, the build system will need to be able to install and store its own .NET SDK for building and linking C# assemblies.

To get started, create a new directory under `%APPDATA%\NuGet` and name it something appropriate. For this example, lets call it `MyLocalNugetSource`. Then you should have the directory `%APPDATA%\NuGet\MyLocalNugetSource`.
Now open PowerShell and enter the following:

    dotnet nuget add source %APPDATA%\NuGet\MyLocalNugetSource --name MyLocalNugetSource

To verify that the source has been properly added, run the command `dotnet nuget list source` in your PowerShell prompt and you should see the source and directory listed.
Now we can move on to building the Mono editor and targets.

### Builidng the Godot Editor and Targets
In your terminal of choice (cmd or PowerShell), make sure you are in the godot source's root directory:

    cd <godot_source_location>\godot

Make sure you have a python environment installed with the SCons package as per the prerequisites.
Now execute the following to build the editor binary:

    scons p=windows target=editor module_mono_enabled=yes

Next is to build debug and release Windows export templates for exporting Godot projects:

    scons p=windows target=template_debug module_mono_enabled=yes
    scons p=windows target=template_release module_mono_enabled=yes

Now we need to generate all the C# bindings for the Godot API and editor tools:

    bin\godot.windows.editor.x86_64.mono --generate-mono-glue modules\mono\glue

Finally, the C# assemblies are built and linked:

    .\modules\mono\build_scripts\build_assemblies.py --godot-output-dir .\bin --push-nupkgs-local %APPDATA%\NuGet\MyLocalNugetSource

Thats it! Now you should have an editor binary built under the `godot\bin` directory that contains GDNet functionality.


https://github.com/godotengine/godot/issues/23687
