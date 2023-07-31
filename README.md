# A lightweight **whenever** wrapper: **whenever_tray**

<!-- @import "[TOC]" {cmd="toc" depthFrom=1 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [A lightweight **whenever** wrapper: **whenever\_tray**](#a-lightweight-whenever-wrapper-whenever_tray)
  - [Configuration](#configuration)
  - [Usage](#usage)
  - [Requirements](#requirements)
  - [Credits](#credits)
    - [Libraries](#libraries)
    - [Graphics](#graphics)
  - [License](#license)

<!-- /code_chunk_output -->


**whenever_tray** aims at being the most essential, lightweight, and cross-platform wrapper for the [**whenever**](https://github.com/almostearthling/whenever) utility. Its purpose is to launch **whenever** and control it through its I/O based interface, so that

* the scheduler is started when **whenever_tray** runs
* it stops when the **whenever_tray** exits
* **whenever** activity can be paused or resumed leaving the scheduler running

via menu entries exposed by the wrapper. The menu can be accessed by clicking on an icon in the _tray notification area_, a common paradigm for applications that run in the background but still require some sporadic user interaction in modern graphical environments. **whenever_tray** also allows to show the log of **whenever** (provided that at least a text editor is available) via a menu entry, and to start the scheduler at lower/lowest priority by specifying it in the configuration file.

The functionality of **whenever_tray** is intentionally limited to the lowest possible extent, in order to keep the code essential (thus reducing the need of specific code for specific platforms) and to use the least possibile computational resources. While CPU consumption should not be a problem, as both **whenever_tray** and **whenever** itself spend most of their time _waiting_, having a small application that uses a low amount of RAM could be desirable, in order to have the possibility that the **whenever** "suite" would run on a desktop system without a noticeable impact on it -- except when it checks conditions or executes tasks that, by user design, are resource hungry.

Once configured, **whenever_tray** takes care of

* starting the **whenever** scheduler at the desired priority (the _lowest_ possible is a good option)
* forcing **whenever** to log to the specified location and with the specified level
* capturing the I/O of the scheduler in order to send commands to _pause_, _resume_, or _exit_ upon request
* hiding the _console window_ on systems that would show it, such as Windows.

It also can open a text editor, or another app capable of viewing log files if specified in the configuration, to display the log for the current session: the choice of using a third party application to view the log file, thus avoiding to integrate such functionality in the resident application, contributes in keeping its memory footprint low.


## Configuration

The TOML configuration file of **whenever_tray** has the following form:

```toml
# The whenever_tray section is mandatory, and the sample values provided below
# correspond to the default values.
[whenever_tray]

# if the full path is not provided, the executable must be in the PATH
whenever_command = "whenever"

# log level can be one of: trace, debug, info, warn, error (as string)
whenever_loglevel = "info"

# APP_DATA is the application data directory (environment variables not used)
whenever_logfile = 'APP_DATA/whenever.log'
whenever_config = 'APP_DATA/whenever.toml'

# priority can be one of: normal, low, minimum (as string)
whenever_priority = "minimum"

# path to the text processor used to view the log file
logview_command = 'gnome-text-editor'
```

The configuration file must be named _whenever_tray.toml_ and be available in the so-called _application data directory_. The position of this directory varies on different operating systems:

* `%AppData%\whenever_tray\` on Windows
* `~/.whenever_tray/` on UNIX/Linux
* `~/Library/Application Support/.whenever_tray/` on Mac

and the directory, as well as a well-formed configuration file, have to be present before **whenever_tray** is launched -- otherwise the application will complain that the configuration file cannot be read, before running using the default values. All entries are optional, but an empty file should at least contain an empty `[whenever_tray]` section for the application not to show an error pop-up at startup. In the `whenever_command` and `logview_command` entries, the full path to the executable can be omitted if the executable itself is in a location within the search _PATH_. A sample _whenever_tray.toml_ with all entries is provided in the repository.

At the moment **whenever_tray** does not perform any substitution in the paths provided in the configuration file: a `~` is thus not expanded to the user home directory, and environment variable mentions are not replaced by their values. Since all paths will be relative to the path from which the application is launched, it is recommended to explicitly specify full paths for both `whenever_logfile` and `whenever_config`.

> **NOTE**: the default values shown above yield for UNIX/Linux systems, while on Windows the default value for `whenever_command` is _whenever.exe_ and the default value for `logview_command` is actually _notepad.exe_. The new _gnome-text-editor_ is the default viewer when not compiling on Windows, however it might not be available, for instance on MacOSX or on versions of GNOME prior to the current one: in such cases it should either be explicitly specified or replaced, in the configuration file, with an available application.[^1]


## Usage

As long as no installation utility is provided, all the setup to allow **whenever_tray** to be launched at session startup has to be done by hand. The steps actually differ on different platforms, but share some concepts that, nowadays, are considered quite common in graphical desktop environments -- such as the definition of _startup applications_. On both Linux and Windows the streamlined process for requiring an application to automatically run at the beginning of a session consists of the following steps:

1. create a _shortcut_ to the application (which includes assigning an icon): the shortcut is a file ending in _.desktop_ on GNOME and in _.lnk_ on Windows.
2. add this shortcut to the list of applications that start at the beginning of the session ([startup applications](https://help.gnome.org/users/gnome-help/stable/shell-apps-auto-start.html.en) on GNOME and [startup apps](https://support.microsoft.com/en-us/windows/add-an-app-to-run-automatically-at-startup-in-windows-10-150da165-dcd9-7230-517b-cf3c295d89dd) on Windows)
3. log out and log in again.

If everything is set up correctly,[^2] the tray notification area shows, from now on, a small clock icon from which it is possible to access the above described functionalities to interact with a running instance of **whenever**.


## Requirements

In order to correctly build **whenever_tray**, the following requirements need to be fulfilled:

* _WxWidgets_ and its development libraries and headers must be available: the _3.2_ version has been used to develop this software, previous versions are not supported. On Windows the library has been built from scratch in order to obtain statically linkable libraries and not to depend on DLLs: to reproduce this and compile **whenever_tray** the `WXWIN` [variable](https://wiki.wxwidgets.org/Downloading_and_installing_wxWidgets) has to be set prior to running _CMake_; on Linux it is possible to use the [packages](https://docs.codelite.org/wxWidgets/repo320/) kindly provided by the folks at [_CodeLite_](https://codelite.org/);
* the preliminary steps to build the application are performed by [_CMake_](https://cmake.org/) with the hope to mitigate the hassle of being multiplatform: the _CMake_ build has been tested on Linux and Windows. In other words, the appropriate _CMake_ package is necessary for building the application on all supported systems;
* the STL-based [_toml11_](https://github.com/ToruNiina/toml11) TOML library has been used to interpret the configuration file: in order to compile **whenever_tray** the latest released version (at the time of writing: version 3.7.1) should be downloaded from the _Releases_ page and uncompressed in a directory called `toml11` under `src`. The active master works as well, but it is preferable to rely on released versions;
* although not properly a requirement, on Windows the _Visual Studio Community Edition_ IDE is easier to use to build a release version of the application.

> **NOTE**: on many recent Linux distributions (namely, the ones that include GNOME 3.26 or higher), the _tray notification area_ is no more supported natively on GNOME, at least in the form used by the _WxWidgets_ library: a GNOME shell extension (for example: [Appindicator](https://extensions.gnome.org/extension/615/appindicator-support/) or [Tray Icons: Reloaded](https://extensions.gnome.org/extension/2890/tray-icons-reloaded/)) might have to be installed. Moreover, there are still many problems with _WxWidgets_ on _Wayland_, especially when using _Xwayland_ (by defining/exporting `GDK_BACKEND=x11` before the command that launches **whenever_tray**) which is needed for the tray icon to be shown, because _WxWidgets_ uses a legacy protocol to display an icon on the notification area that is only supported on _X11_. The best solution so far to have **whenever_tray** working on a recent Linux desktop, is to start the session in _Xorg mode_. To achieve this, still on the login screen, the user should click the small icon on the lower right corner that appears upon selection of an account, and click the _GNOME on Xorg_ entry. The choice will be remembered for the following sessions. However this might also result in a different user experience, either snappier or slower depending on how the desktop system is used.[^3]

At the moment the specific requirements for MacOSX are not known: however they should be similar to the ones summarized above. Where possible, the use of a binary release might be a more viable alternative, even though there are no specific installers or packages for now.


## Credits

### Libraries

The TOML library used in **whenever_tray** is [_toml11_](https://github.com/ToruNiina/toml11), and the cross platform graphical framework is [_WxWidgets 3.2_](https://www.wxwidgets.org/), which at the time of writing is the default supported version on _Debian 12_ -- used as the main Linux testbed for the application. The [_wx_cmake_template_](https://github.com/lszl84/wx_cmake_template) has been used for scaffolding the application so that it could remain cross platform in terms of both operating and development environment: there is no need, however, to download thistemplate, since all the files generated using the template itself are directly included in the source repository.

### Graphics

The icon used in the _About Box_ has been released for free by Rafi at [GraphicsFuel](https://www.graphicsfuel.com/). The clock icon used in the _tray notification area_ is an interpretation/reworking of a glyph provided by Google in its [Material Design Icons](https://fonts.google.com/icons) collection.


## License

This tool is licensed under the LGPL v2.1 (may change to LGPL v3 in the future): see the provided LICENSE file for details.


[^1]: the best option is actually to use a viewer made explicitly for log files: for instance [glogg](https://glogg.bonnefon.org/) is free, fast, lightweight, and cross-platform; it also sports features such as searching/highlighting that come handy when dealing with verbose log files like the ones that **whenever** generates when the log level is set to _trace_.
[^2]: that is: all required libraries are present (especially on Linux), **whenever** itself is installed and usable, the [configuration](#configuration) and the _application data directory_ exist, and are correctly positioned.
[^3]: there is plenty of discussions about weaknesses and strengths of the two systems, compared to each other: in some aspects it looks like _Wayland_ is still not on par withh a quite mature project as _Xorg_ actually is; on the other hand _Wayland_ is very actively developed, therefore many issues and bugs are fixed very quickly. Being adopted by many major players in the Linux ecosystem, _Wayland_ is becoming the _de facto_ standard for Linux desktop. This feeds the hope that many of the quirks affecting it at the moment will soon be addressed.
