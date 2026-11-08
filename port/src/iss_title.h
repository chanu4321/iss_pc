#ifndef ISS_TITLE_H
#define ISS_TITLE_H
/* Titles whose executable is the verified 1.07 build the port's code patches target.
 *
 * The European CUSA00004 1.07 executable is byte-identical to the US CUSA00223 1.07 one (same
 * unwrapped ELF, SHA-256 b3afd173...bfc605, docs/CUSA00004_CAMERA.md), and European merged 1.07
 * dumps can keep APP_VER 01.00. These metadata values never authorize a write on their own: every
 * caller still checks the executable segments and the instruction signatures at its sites, so a
 * different build is refused byte by byte. */
#include <string.h>

static inline int iss_title_v107(const char *serial, const char *version) {
    if (!serial || !version) return 0;
    if (!strcmp(serial, "CUSA00223")) return !strcmp(version, "01.07");
    if (!strcmp(serial, "CUSA00004")) return !strcmp(version, "01.07") || !strcmp(version, "01.00");
    return 0;
}

#endif
