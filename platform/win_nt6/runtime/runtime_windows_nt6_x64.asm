; Supply the static CRT import slot without requiring Windows 8's kernel export.
option casemap:none

EXTERN windows_nt6_get_system_time:PROC

.data
PUBLIC __imp_GetSystemTimePreciseAsFileTime
__imp_GetSystemTimePreciseAsFileTime DQ OFFSET windows_nt6_get_system_time

END
