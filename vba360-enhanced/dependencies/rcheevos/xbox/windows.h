/* Xbox 360: rcheevos includes <windows.h> when _WIN32 is defined; the
   console's version of it is <xtl.h>. WINVER is removed so rcheevos keeps to
   what the console has (CRITICAL_SECTION locks, fopen_s; no SRW locks or wide
   file names). */
#include <xtl.h>
#undef WINVER
