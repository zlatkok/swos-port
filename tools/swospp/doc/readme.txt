                     SWOS++ - swos-port helper edition
                     =================================

This source tree is an internal compatibility tool for swos-port. It is used
to run the original DOS game as a behavioral reference and to record data for
comparison tests. It is not the normal standalone SWOS++ distribution. See
../README.md for its current purpose and relationship to the separately
maintained SWOS++ project.

The installation instructions below apply only when a developer intentionally
builds this helper and installs it into a backed-up SWOS 96/97 directory.

=== Installation and uninstallation

To use SWOS++ you must have PC version of Sensible World Of Soccer 96/97
correctly installed with all supporting files. It is recommended that you make
a backup of SWOS executable before doing anything listed below.

To install SWOS++, you must do the following:

1) copy files:
    - patchit.com
    - loader.bin
    - swospp.bin

to directory where SWOS is installed.

2) run patchit.com, and select '1' (install) from the menu. If everything went
all right, you should get the reply "Patched OK."

Now SWOS++ is installed and ready to use.

To uninstall it, first run patchit, and select '2' (uninstall) from menu. You
should get same reply "Patched OK.". Then delete files:
    - patchit.com
    - loader.bin
    - swospp.bin


=== System requirements

SWOS++ requires SWOS to work, so any computer capable of running SWOS will host
it quite happily. It requires some small amount of additional memory (currently
about 300kb) to load. Network play requires DOSBox, without it it might work
only in local IPX network, but that has never been tested.


=== Additional considerations

If you are using any versions of key patch, you will have to remove it because
of incompatibilities between it and SWOS++. First remove keypatch, and then
install SWOS++. Do not worry about support for two players on keyboard, it has
been added to SWOS++. Just go to controls menu and select controls that you
like.


=== Author

This software was written by Zlatko Karakas and is distributed as freeware.
I take no responsibility for any damage or problems caused by using it
but I do welcome comments and suggestions.


=== Copyright

The original SWOS++ material is copyright (c) 2002-2026 Zlatko Karakas.
The games SWOS, SWOS '95/'96, SWOS Euro Edition, SWOS '96/'97 and their data
files are (c) 1995, 1996, 1997. Sensible Software Ltd.

You may freely use and distribute this archive as long as the contents remain
intact.

Zlatko Karakas,
zlatko.karakas#gmail.com
zkz#eunet.rs

(replace `#' with `@' when posting ;-))
