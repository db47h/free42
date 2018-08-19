# free42 with SwissMicro's extensions for desktop and mobile

The end goal is to bring [DM42] to the desktop. This is a work in progress!

For more information about free42, see the original [free42 readme][free42-README] and [Thomas Okken's free42 page][free42].

## Status

Note that some of the changes (like the default skins) apply to all builds, may be with the exception of Android, but I haven't tested it yet. The current focus is on the GTK builds. And I will probably never touch anything Windows or Mac related. Feel free to get in touch if you want to work on these platforms.

- The default skins have been replaced by [the anthracite skin][free42skins].
- The GTK+ build has been updated to build on modern systems. It still uses GTK+ 2 but most deprecated APIs calls have been replaced.
- Decimal floating point and ALSA audio are the default in GTK builds:
  - add `BIN_MATH=1` to the make command line to enable binary floating point
  - add `NO_AUDIO=1` to the make command line to disable alsa audio and use the default `gtk_beep()`.

## Building

```bash
git clone https://github.com/db47h/free42
# pull the skins
git submodule init

cd free42/gtk
make -j8
```

This will build free42 with decimal floating point and ALSA audio.

## License

Free42 is an Open Source project. The source code and executables are released under the terms of the [GNU General Public License, version 2][GPL2].


[free42-README]: https://github.com/db47h/free42/blob/free42x/README
[DM42]: https://www.swissmicros.com/dm42.php
[free42skins]: https://github.com/db47h/free42skins
[GPL2]: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
[free42]: http://thomasokken.com/free42/