

                                 Previous 0.4


                    http://previous.alternative-system.com/


Contents:
---------
1. License
2. About Previous
3. Compiling and installing
4. Known problems
5. Running Previous
6. Contributors
7. Contact


 1) License
 ----------

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Soft-
ware Foundation; either version 2 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the
 Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston,
 MA  02110-1301, USA


 2) About Previous
 -----------------

Previous is a NeXT Computer emulator based on the Atari emulator Hatari. It uses
the latest CPU emulation core from WinUAE. Previous is confirmed to compile and 
run on Linux, Mac OS X and Windows. It may also work on other Systems which are 
supported by the SDL library, like FreeBSD, NetBSD and BeOS.

For now it emulates the following machines:
 NeXT Computer (original 68030 Cube)
 NeXTcube
 NeXTstation
 NeXTstation Color

Actually it does not emulate Turbo machines.


 3) Compiling and installing
 ---------------------------

For using Previous, you need to have installed the following libraries:

Required:
- The SDL library v1.2.15 (v2.x won't work) (http://www.libsdl.org)
- The zlib compression library (http://www.gzip.org/zlib/)


Don't forget to also install the header files of these libraries for compiling
Previous (some Linux distributions use separate development packages for these
header files)!

For compiling Previous, you need a C compiler (preferably GNU C), and a working
CMake installation (see http://www.cmake.org/ for details).

CMake can generate makefiles for various flavors of "Make" (like GNU-Make)
and various IDEs like Xcode on Mac OS X. To run CMake, you've got to pass the
path to the sources of Previous as parameter, for example run the following if
you are in the topmost directory of the Previous source tree:
	cmake .

If you're tracking Previous version control, it's preferable to do
the build in a separate build directory as above would overwrite
the (non-CMake) Makefiles coming with Previous:
	mkdir -p build
	cd build
	cmake ..

Have a look at the manual of CMake for other options. Alternatively, you can
use the "cmake-gui" program to configure the sources with a graphical
application.

After cmake finished the configuration successfully, you can compile Previous
by typing "make". If all works fine, you'll get the executable "Previous" in 
the src/ subdirectory of the build tree.


 4) Known problems
 -----------------

WARNING:
This is software in an early development state. Always make sure you only work 
on copies of your images or other data. There is a risk of data loss.

Previous is still work in progress. Some hardware is not yet emulated. Status:
CPU		good
MMU		good
FPU		good
DSP		missing
DMA		partial
NeXTbus		missing
Memory		good
2-bit graphics	good
Color graphics	good
RTC		good
Timers		buggy
SCSI drive	good
MO drive	buggy
Floppy drive	missing
Ethernet	dummy
Serial		missing
Printer		missing
Sound		missing
Keyboard	good
Mouse		good

The Turbo chipset is not emulated.

There are remaining problems with the host to emulated machine interface for
input devices, mainly keyboard.


Known issues (version 0.4):
- Using NeXTstep 3.0 or later on emulated 68030 NeXT Computer causes disk
  image corruption in certain situations.
- Un-emulated hardware may cause problems in certain situations (see above).
- The kernel sometimes hangs during the boot process after printing "root on"
  (this is called "root on hang"). NeXTstep 3.0 and later are affected.
- The MO drive causes problems (mainly hangs) when both drives are used.
- Keys are not mapped correctly depending on the host keyboard layout.
- Shortcuts do not work properly or overlap with host commands on some platforms.
- The clock does not tick accurately. Real time clock power-on test may fail
  sporadically on fast host systems.
- FPU only works on x86 hosts.
- Mac OS X: When minimizing and maximizing the application window there might
  occur a blue layer above the window's contents. This is an SDL bug.
- Mac OS X: The native GUI is still showing Hatari's preferences.


 5) Running Previous
 -------------------

For running the emulator, you need an image of the boot ROM of the emulated 
machine.

While the emulator is running, you can open the configuration menu by
pressing F12, toggle between fullscreen and windowed mode by pressing F11 
and initiate a clean shut down by pressing F10 (emulates the power button).


 6) Contributors
 ---------------

Many thanks go to the members of the NeXT International Forums for their
help. Special thanks go to Gavin Thomas Nicol, Piotr Twarecki, Toni Wilen,
Michael Bosshard, Thomas Huth, Olivier Galibert, Vaughan Kaufman and 
Peter Leonard!
This emulator would not exist without their help.


 7) Contact
 ----------

If you want to contact the authors of Previous, please have a look at the 
NeXT International Forums (http://www.nextcomputers.org/forums).

Visit the project page of Previous for more details:

 http://previous.alternative-system.com/

