# check required autoconf version
AC_PREREQ([2.59])

# define distribution properties
AC_INIT([hclib], [1.0], [vivekk@iiitd.ac.in])

# check if srcdir is correct
AC_CONFIG_SRCDIR([inc/hclib.h])

AC_CONFIG_AUX_DIR([config])

AM_INIT_AUTOMAKE([1.9 tar-ustar no-define foreign dist-bzip2])

AC_COPYRIGHT([])

# configure script revision
AC_REVISION([$Revision: 0.1 $])

AC_PROG_CXX
AC_PROG_LIBTOOL
AC_LANG(C++)

CPPFLAGS=$CPPFLAGS_BACKUP

###################################################################
########### GET LOW LEVEL DETAILS IN THE INSTALLATION #############
###################################################################

### ENABLE PERFCOUNTER
AC_ARG_ENABLE(perfcounter,
    AS_HELP_STRING([--enable-perfcounter],
    [turn on HC performance counters (Default is false)]),
    [with_perfcounter=$enableval],
    [with_perfcounter=no;])

AS_IF([test "x$with_perfcounter" != xno],
      [ AC_MSG_NOTICE([Enabled HC performance counters]) ],
      [ AC_MSG_NOTICE([Disabled HC performance counters]) ])

AM_CONDITIONAL(PERFCOUNTER, test "x$with_perfcounter" != xno)
### End PERFCOUNTER

# Variable substitutions.
AC_SUBST([ac_aux_dir])
AC_SUBST([abs_top_srcdir])

#
# Config file to process
#
AC_CONFIG_FILES([
Makefile
src/Makefile
])

# Generates and runs config.status, which in turn creates the makefiles 
# and any other files resulting from configuration
AC_OUTPUT
