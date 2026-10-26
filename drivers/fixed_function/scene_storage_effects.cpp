#include "scene_storage.h"

Error FixedFunctionSceneStorage::environment_set_effect(RID p_env, const StringName &p_name, const Variant *p_values, int p_count) {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	if (p_name == StringName() || !p_values || p_count <= 0) {
		return ERR_INVALID_PARAMETER;
	}
	Vector<Variant> values;
	if (values.resize(p_count) != OK) {
		return ERR_OUT_OF_MEMORY;
	}
	for (int i = 0; i < p_count; i++) {
		// Only value types used by Environment setters belong here; no mutable
		// containers or Object references can alias the accepted CPU snapshot.
		switch (p_values[i].get_type()) {
			case Variant::BOOL:
			case Variant::INT:
			case Variant::_RID:
				break;
			case Variant::REAL: {
				const double value = p_values[i];
				if (Math::is_nan(value) || Math::is_inf(value)) {
					return ERR_INVALID_PARAMETER;
				}
			} break;
			case Variant::COLOR:
				if (!_finite_color(p_values[i])) {
					return ERR_INVALID_PARAMETER;
				}
				break;
			case Variant::BASIS: {
				const Basis basis = p_values[i];
				for (int column = 0; column < 3; column++) {
					for (int row = 0; row < 3; row++) {
						if (Math::is_nan(basis[column][row]) || Math::is_inf(basis[column][row])) {
							return ERR_INVALID_PARAMETER;
						}
					}
				}
			} break;
			default:
				return ERR_INVALID_PARAMETER;
		}
		values.write[i] = p_values[i];
	}
	_get_environment(p_env)->optional_effects[p_name] = values;
	return OK;
}

Error FixedFunctionSceneStorage::environment_get_effect(RID p_env, const StringName &p_name, Vector<Variant> &r_values) const {
	const Error error = environment_check(p_env);
	if (error != OK) {
		return error;
	}
	const Map<StringName, Vector<Variant>>::Element *effect = _get_environment(p_env)->optional_effects.find(p_name);
	if (!effect) {
		return ERR_DOES_NOT_EXIST;
	}
	r_values = effect->get();
	return OK;
}

bool FixedFunctionSceneStorage::environment_get_depth_fog(RID p_env, Color &r_color, float &r_begin, float &r_end) const {
	const FFEnvironment *environment = _get_environment(p_env);
	if (!environment) {
		return false;
	}
	const auto *fog = environment->optional_effects.find("fog");
	if (!fog || fog->get().size() < 2 || !bool(fog->get()[0])) {
		return false;
	}
	const auto *depth = environment->optional_effects.find("fog_depth");
	if (depth && (depth->get().size() < 3 || !bool(depth->get()[0]))) {
		return false;
	}
	r_color = fog->get()[1];
	// Environment only sends its depth settings when one of them is assigned.
	r_begin = depth ? float(depth->get()[1]) : 10.0f;
	r_end = depth ? float(depth->get()[2]) : 100.0f;
	return true;
}
