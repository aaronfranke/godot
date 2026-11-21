#include "test_preset_scope.h"

#include "servers/rendering/rendering_server_globals.h"

TestFixedFunctionPresetScope::TestFixedFunctionPresetScope(FixedFunctionGraphicsDevice &p_device, void *p_window) :
		_context(p_device, p_window, 128, 96, false), _targets(_context), _textures(_context.get_texture_storage(), &_targets), _facade(_context), _scene(_context, _targets), _lights(_context), _utility_facade(_context, _textures, _facade, _lights, _scene), _materials(RSG::material_storage), _utilities(RSG::utilities) {
	RSG::material_storage = &_facade;
	RSG::utilities = &_utility_facade;
}
TestFixedFunctionPresetScope::~TestFixedFunctionPresetScope() {
	_context.finalize();
	RSG::material_storage = _materials;
	RSG::utilities = _utilities;
}
