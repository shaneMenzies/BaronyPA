# BaronyPA
A mumble plugin that adds positional audio support for Barony.  

## Installation

<details>
<summary>Windows</summary>

- Method 1: Get the Mumble Plugin file from [Releases](https://github.com/shaneMenzies/BaronyPA/releases), and open it with Mumble.  

  ![Opening in Mumble](images/Windows_mumble_plugin_open.png)  

- Method 2: Place the compiled baronyPA.dll file in your Mumble plugin directory (%appdata%/Mumble/Plugins).  

  ![dll File](images/windows_dll.png)
  ![Plugin Directory](images/Windows_plugins_location.png)  

</details>
<details>
<summary>Linux</summary>

- Method 1: Get the Mumble Plugin file from [Releases](https://github.com/shaneMenzies/BaronyPA/releases), and select it in Mumble's "Install plugin..." dialog.  

  ![Mumble Plugin Install](images/Linux_mumble_install.png)  

- Method 2: Place the compiled libbaronyPA.so file in your Mumble plugin directory (~/.local/share/Mumble/Mumble/Plugins)

  ![Plugin Directory](images/Linux_plugin_in_local.png)  

</details>

### Enabling  

For the plugin to work, it needs to have both "Enable" and "PA" checked for it, and "Link to Game and Transmit Position" needs to be set.

![Mumble Plugin Config](images/mumble_config.png)  

Currently, when enabled, the plugin should send a message that its waiting for Barony to be opened.  

![Plugin Waiting](images/baronypa_waiting.png)  

Once Barony is opened, there should be another message telling you that the plugin is linked.

![Plugin Linked](images/baronypa_linked.png)  

## Building

<details>
<summary>Windows</summary>

1. Open the plugin directory in a command prompt with the required toolset [See Here](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170).  

2. Use CMake to generate NMake Makefiles.  

  ![CMake Run](images/Windows_cmake_cmd.png)  

3. Build the plugin using NMake  

  ![NMake Build](images/Windows_cmake_built.png)  

</details>
<details>
<summary>Linux</summary>

1. Use CMake to generate the Makefile  

  ![Cmake Run](images/Linux_cmake_cmd.png)  

2. Build the plugin using Make  

  ![Make Build](images/Linux_cmake_built.png)

</details>

### Bundling  

To make the Mumble Plugin bundle, you need to make a .zip archive with the Windows and Linux plugins, and the manifest.xml file.  

![Bundle Format](images/bundle_view.png)  

Renaming the file extension from .zip to .mumble_plugin will make it a valid Mumble Plugin file.
