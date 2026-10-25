#pragma once

#include "editor/editor_export.h"

class EditorExportPlatformWindowsNT6 : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformWindowsNT6, EditorExportPlatformPC);

public:
	virtual void get_export_options(List<ExportOption> *r_options);
};
