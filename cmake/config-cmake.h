/* CMake config.h for Hatari */

/* Define if you have a PNG compatible library */
#cmakedefine HAVE_LIBPNG 1

/* Define if you have a readline compatible library */
#cmakedefine HAVE_LIBREADLINE 1

/* Define if you have the PortAudio library */
#cmakedefine HAVE_PORTAUDIO 1

/* Define if you have a X11 environment */
#cmakedefine HAVE_X11 1

/* Define to 1 if you have the `z' library (-lz). */
#cmakedefine HAVE_LIBZ 1

/* Define to 1 if you have the <zlib.h> header file. */
#cmakedefine HAVE_ZLIB_H 1

/* Define to 1 if you have the <termios.h> header file. */
#cmakedefine HAVE_TERMIOS_H 1

/* Define to 1 if you have the <glob.h> header file. */
#cmakedefine HAVE_GLOB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#cmakedefine HAVE_STRINGS_H 1

/* Define to 1 if you have the <SDL/SDL_config.h> header file. */
#cmakedefine HAVE_SDL_SDL_CONFIG_H 1

/* Define to 1 if you have the <sys/times.h> header file. */
#cmakedefine HAVE_SYS_TIMES_H 1

/* Define to 1 if you have the `cfmakeraw' function. */
#cmakedefine HAVE_CFMAKERAW 1

/* Define to 1 if you have the 'setenv' function. */
#cmakedefine HAVE_SETENV 1

/* Define to 1 if you have the `select' function. */
#cmakedefine HAVE_SELECT 1

/* Define to 1 if you have unix domain sockets */
#cmakedefine HAVE_UNIX_DOMAIN_SOCKETS 1

/* Define to 1 if you have the 'posix_memalign' function. */
#cmakedefine HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the 'memalign' function. */
#cmakedefine HAVE_MEMALIGN 1


/* Relative path from bindir to datadir */
#define BIN2DATADIR "@BIN2DATADIR@"

/* Define to 1 to enable DSP 56k emulation for Falcon mode */
#cmakedefine ENABLE_DSP_EMU 1

/* Define to 1 to enable WINUAE cpu  */
#cmakedefine ENABLE_WINUAE_CPU 1

/* Define to 1 to use less memory - at the expense of emulation speed */
#cmakedefine ENABLE_SMALL_MEM 1

/* Define to 1 to enable trace logs - undefine to slightly increase speed */
#cmakedefine ENABLE_TRACING 1
