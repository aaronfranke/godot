#pragma once

#include "editor/editor_export.h"

class EditorExportPlatformWindows9x : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformWindows9x, EditorExportPlatformPC);

public:
	virtual void get_export_options(List<ExportOption> *r_options);
};
