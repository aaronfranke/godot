#pragma once

#include "editor/editor_export.h"

class EditorExportPlatformWindowsNT5 : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformWindowsNT5, EditorExportPlatformPC);

public:
	virtual void get_export_options(List<ExportOption> *r_options);
};
