#pragma once

/**
 * @file
 * some rare compiler specific macros
 */

#if defined(__clang__)
#  define BEGIN_NOWARN_BLOCK \
        _Pragma("clang diagnostic push") \
        _Pragma("clang diagnostic ignored \"-Wmissing-field-initializers\"")

#  define END_NOWARN_BLOCK \
        _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER)
#  define BEGIN_NOWARN_BLOCK \
        __pragma(warning(push, 3))

#  define END_NOWARN_BLOCK \
        __pragma(warning(pop))
#else
#  warning "missing definitions for BEGIN/END NOWARN BLOCKs"
#endif
