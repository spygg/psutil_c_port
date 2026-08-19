# libpsutil.pri - qmake project include file.
# Equivalent of libpsutil/CMakeLists.txt for qmake-based (Qt .pro) projects.
#
# Include this file from a .pro to build libpsutil together with your target:
#   include($$PWD/libpsutil/libpsutil.pri)
#
# On Windows it also exports PSUTIL_LIBRARIES (the list of Windows system
# libraries) so the including .pro can link them into its target.

PSUTIL_LIBDIR = $$PWD

# ---------------------------------------------------------------------------
# Platform detection. Order matters: Android must be checked before Linux,
# since Android is Linux-based and also activates the linux scope in qmake.
# ---------------------------------------------------------------------------
contains(QMAKE_PLATFORM, android) | contains(ANDROID_ABI, .*) {
    # Android Bionic libc: no extra libs needed, pthread is built in
    DEFINES += PSUTIL_ANDROID
    SOURCES += $$PSUTIL_LIBDIR/arch/android/psutil_android.c
    HEADERS += $$PSUTIL_LIBDIR/arch/android/psutil_android.h
} else:contains(QMAKE_PLATFORM, freebsd|netbsd|openbsd|dragonfly) {
    DEFINES += PSUTIL_BSD
    SOURCES += $$PSUTIL_LIBDIR/arch/bsd/psutil_bsd.c
    HEADERS += $$PSUTIL_LIBDIR/arch/bsd/psutil_bsd.h
} else:linux {
    DEFINES += PSUTIL_LINUX
    SOURCES += $$PSUTIL_LIBDIR/arch/linux/psutil_linux.c
    HEADERS += $$PSUTIL_LIBDIR/arch/linux/psutil_linux.h
} else:macx {
    DEFINES += PSUTIL_MACOS
    SOURCES += $$PSUTIL_LIBDIR/arch/macos/psutil_macos.c
    HEADERS += $$PSUTIL_LIBDIR/arch/macos/psutil_macos.h
} else:win32 {
    DEFINES += PSUTIL_WINDOWS
    SOURCES += $$PSUTIL_LIBDIR/arch/windows/psutil_windows.c
    HEADERS += $$PSUTIL_LIBDIR/arch/windows/psutil_windows.h
} else {
    message(WARNING "libpsutil: unknown platform")
}

# Core library sources and public header
SOURCES += $$PSUTIL_LIBDIR/psutil.c
HEADERS += $$PSUTIL_LIBDIR/psutil.h

# Include directories
INCLUDEPATH += $$PSUTIL_LIBDIR $$PSUTIL_LIBDIR/arch

# ---------------------------------------------------------------------------
# Platform-specific compile definitions and link libraries
# ---------------------------------------------------------------------------
win32 {
    # Windows system libraries used by libpsutil
    LIBS += -lpsapi -liphlpapi -lwtsapi32 -ladvapi32 -lws2_32 -luserenv

    # Exported for the including .pro to link into its target as well
    PSUTIL_LIBRARIES = -lpsapi -liphlpapi -lwtsapi32 -ladvapi32 -lws2_32 -luserenv

    MSVC {
        # MSVC: suppress CRT deprecation warnings (strncpy, sscanf, strdup, ...)
        DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    } else:mingw {
        # MinGW: target Windows XP (0x0501) for maximum compatibility.
        # _GNU_SOURCE exposes strdup and other GNU extensions.
        DEFINES += _WIN32_WINNT=0x0501 WINVER=0x0501 _GNU_SOURCE
    }
}

linux {
    # _GNU_SOURCE exposes sched_getaffinity/sched_setaffinity and non-standard
    # utmp members; _DEFAULT_SOURCE keeps glibc from hiding other APIs.
    DEFINES += _GNU_SOURCE _DEFAULT_SOURCE
}
