# Piano Visualizer Virtual Camera Extension

The Virtual Camera Extension adds virtual camera support to [Piano Visualizer](https://github.com/Ominus-tch/Piano-Visualizer).

It allows you to use Piano Visualizer's output as a camera in applications such as OBS, Discord, web browsers, and other software that supports camera input.

## Installation

### Download

1. Go to the [Releases](https://github.com/Ominus-tch/VirtualCameraExtension/releases) page.
2. Download the latest `VirtualCameraExtension.dll`.
3. Place the DLL next to your `PianoVisualizer.exe`:

```text
PianoVisualizer.exe
extensions/
    └── VirtualCamera/
        └── VirtualCameraExtension.dll
```

4. Restart Piano Visualizer.

The Virtual Camera option should now be available.

The location of `VirtualCameraExtension.dll` may be subject to change in future releases.

### Building from source

If you have the Piano Visualizer source code, clone this repository directly into an `extensions` folder:

```bash
cd PianoVisualizer
git clone https://github.com/Ominus-tch/VirtualCameraExtension.git extensions/VirtualCamera
```

You can then either build the extension using the included Visual Studio project,
or, if you have it, using `msbuild`:

```bash
msbuild extensions\VirtualCamera\VirtualCameraExtension.slnx /p:Configuration=Release
```

If you do not have `msbuild` in your path, run it from a **Developer Command Prompt for Visual Studio**

You do not need to rebuild Piano Visualizer. It detects the `.dll` and initializes all components.

## Using the Virtual Camera

Once installed, start the virtual camera from within Piano Visualizer.

Your operating system will then expose the Piano Visualizer virtual camera to other applications.

You can use it with applications such as:

* OBS Studio
* Discord
* Web browsers
* Video conferencing software
* Other camera applications

If an application was already running when the virtual camera was started, you may need to restart that application before the camera appears.

## Requirements

* Windows 10 or later
* Piano Visualizer

## Related Project

[Piano Visualizer](https://github.com/Ominus-tch/Piano-Visualizer)

## Releases

Download the latest version from the [Releases](https://github.com/Ominus-tch/VirtualCameraExtension/releases) page.
