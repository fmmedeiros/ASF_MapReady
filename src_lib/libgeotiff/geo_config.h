/* geo_config.h.  Generated automatically by configure.  */
#ifndef GEO_CONFIG_H
#define GEO_CONFIG_H

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you have the <stdlib.h> header file.  */
/* Ugly hack that comes due to libraries or installations that have
   somehow leaked their autoconf crud into the public headers.  */
#ifndef HAVE_STDLIB_H
#  define HAVE_STDLIB_H 1
#endif
/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* #undef HAVE_LIBPROJ */
/* #undef HAVE_PROJECTS_H */

#endif /* ndef GEO_CONFIG_H */
