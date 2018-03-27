# For license: see LICENSE file at top-level

# Feature checks

#
# debugging output: disabled by default
#
AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [Enable library debug outptut @<:@default=no@:>@]),
	AS_IF([test "x$enableval" = "xyes"],
		    AC_DEFINE([ENABLE_DEBUG], [1], [Enable debug])
		    ),
	[])
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = "xyes"])

#
# non-standard/proposed API features (shmemx): disabled by default
#
AC_ARG_ENABLE([experimental],
	AS_HELP_STRING([--enable-experimental],
			[Enable non-standard extensions @<:@default=no@:>@]))
AS_IF([test "x$enable_experimental" = "xyes"],
	[AC_DEFINE([ENABLE_EXPERIMENTAL], [1], [Enable non-standard extensions])
	AC_SUBST([ENABLE_EXPERIMENTAL], [1])],
	[AC_SUBST([ENABLE_EXPERIMENTAL], [0])]
	)
AM_CONDITIONAL([ENABLE_EXPERIMENTAL], [test "x$enable_experimental" = "xyes"])

#
# profiling API: disabled by default
#
AC_ARG_ENABLE([pshmem],
	AS_HELP_STRING([--enable-pshmem],
			[Enable Profiling interface @<:@default=no@:>@]))
AS_IF([test "x$enable_pshmem" = "xyes"],
	[AC_DEFINE([ENABLE_PSHMEM], [1], [Enable profiling interface])
	 AC_SUBST([ENABLE_PSHMEM], [1])],
	[AC_SUBST([ENABLE_PSHMEM], [0])]
	)
AM_CONDITIONAL([ENABLE_PSHMEM], [test "x$enable_pshmem" = "xyes"])

#
# fortran API: disabled by default
#
AC_ARG_ENABLE([fortran],
	AS_HELP_STRING([--enable-fortran],
			[Enable deprecated Fortran API support @<:@default=no@:>@]))
AS_IF([test "x$enable_fortran" = "xyes"],
	[AC_DEFINE([ENABLE_FORTRAN], [1], [Enable Fortran API])
	 AC_SUBST([ENABLE_FORTRAN], [1])],
	[AC_SUBST([ENABLE_FORTRAN], [0])]
	)
AM_CONDITIONAL([ENABLE_FORTRAN], [test "x$enable_fortran" = "xyes"])

#
# address translation: disabled by default
#
AC_ARG_ENABLE([aligned-addresses],
	AS_HELP_STRING([--enable-aligned-addresses],
			[Symmetric addresses are identical everywhere @<:@default=no@:>@]))
AS_IF([test "x$enable_aligned_addresses" = "xyes"],
	[AC_DEFINE([ENABLE_ALIGNED_ADDRESSES], [1], [Enable aligned symmetric addresses])
	 AC_SUBST([ENABLE_ALIGNED_ADDRESSES], [1])],
	[AC_SUBST([ENABLE_ALIGNED_ADDRESSES], [0])]
	)
AM_CONDITIONAL([ENABLE_ALIGNED_ADDRESSES], [test "x$enable_aligned_addresses" = "xyes"])

#
# threads: disabled by default
#
AC_ARG_ENABLE([threads],
	AS_HELP_STRING([--enable-threads],
			[This implementation will support threading levels @<:@default=no@:>@]))
AS_IF([test "x$enable_threads" = "xyes"],
	[AC_DEFINE([ENABLE_THREADS], [1], [Enable threading support])
	 AC_SUBST([ENABLE_THREADS], [1])],
	[AC_SUBST([ENABLE_THREADS], [0])]
	)
AM_CONDITIONAL([ENABLE_THREADS], [test "x$enable_threads" = "xyes"])

#
# C++ compiler/linker: enabled by default
#
AC_ARG_ENABLE([cxx],
	AS_HELP_STRING([--disable-cxx],
			[Support C++ compiler? @<:@default=yes@:>@]))
AS_IF([test "x$enable_cxx" != "xno"],
	[AC_DEFINE([ENABLE_CXX], [1], [Enable C++ compiler])
	 AC_SUBST([ENABLE_CXX], [1])],
	[AC_SUBST([ENABLE_CXX], [0])]
	)
AM_CONDITIONAL([ENABLE_CXX], [test "x$enable_cxx" != "xno"])

#
# Default symmetric heap size
#

dnl unless overridden by user
shmem_default_heap_size="32M"

AC_ARG_WITH([heap-size],
	AS_HELP_STRING([--with-heap-size=VALUE],
			[Make the default symmetric heap size VALUE @<:@default=$shmem_default_heap_size@:>@]))
AS_IF([test "x$with_heap_size" != "x"],
	[shmem_default_heap_size="$with_heap_size"])

AC_DEFINE_UNQUOTED([SHMEM_DEFAULT_HEAP_SIZE], ["$shmem_default_heap_size"], [Default symmetric heao size])
AC_SUBST([SHMEM_DEFAULT_HEAP_SIZE], ["$shmem_default_heap_size"])

AM_CONDITIONAL([SHMEM_DEFAULT_HEAP_SIZE], [test "x$with_heapsize" != "x"])
