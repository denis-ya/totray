![Traymond](https://github.com/fcFn/fcFn.github.io/blob/master/images/logos/traymond_logo.png) ToTray = Traymond + Mouse Jiggler
=======

A very simple app for minimizing any window to tray as an icon. Runs in the background.

In case it terminates unexpectedly, restart the app and all the icons for minimized windows will come back.

It also implements "fake" mouse input to Windows,and jiggle the mouse pointer back and forth. This might be useful for avoiding screensavers or other things triggered by idle detection.

Windows 7 or later required (but see [#3](/../../issues/3)) 

A binary is available [here](https://github.com/fcFn/traymond/releases).

Installing
------------

No installation required, just run Traymond.exe.

Controls
--------

+ __Alt + `__: Minimize the currently focused window to tray.

+ __Double click on an icon__: Bring back the corresponding hidden window.

+ __Tray icon menu__ accessible by right-clicking the Traymond tray icon:

  + __Restore all windows__: Restore all previously hidden windows.

  + __Exit__: Exit Traymond and restore all previously hidden windows.

Building
--------

### Nmake

`> nmake`

Please read [this](https://msdn.microsoft.com/en-us/library/f35ctcxw.aspx) if there are any troubles.

### Microsoft Visual Studio

Import and build using the project files (thanks, [Tyler Szabo](https://github.com/tylerszabo)).

Customizing
-------------

Defines at the top of the file control the key and the mod key for sending windows to tray (use virtual key codes from [here](https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx) and mod keys from [here](https://msdn.microsoft.com/en-us/library/windows/desktop/ms646309(v=vs.85).aspx)):
```
#define TRAY_KEY VK_Z_KEY
#define MOD_KEY MOD_WIN + MOD_SHIFT
```
Contributing
------------

See [Contributing](https://github.com/fcFn/traymond/blob/master/CONTRIBUTING.md).
