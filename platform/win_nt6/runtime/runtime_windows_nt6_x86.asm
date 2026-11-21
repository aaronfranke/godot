; Supply the static CRT import slot without requiring Windows 8's kernel export.
.386
.model flat
option casemap:none

EXTERN _windows_nt6_get_system_time@4:PROC

.data
PUBLIC __imp__GetSystemTimePreciseAsFileTime@4
__imp__GetSystemTimePreciseAsFileTime@4 DD OFFSET _windows_nt6_get_system_time@4

END
