#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// C ABI for bundled libraries with UTF-8 filenames and FILE-based interfaces.
FILE *windows9x_fopen_utf8(const char *p_filename, const char *p_mode);

#ifdef __cplusplus
}
#endif // __cplusplus
