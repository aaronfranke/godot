#include "light_storage.h"

bool FixedFunctionLightStorage::has_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance) const {
	const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *current = _registrations.find(p_instance);
	return current && current->get().light == p_light;
}

Error FixedFunctionLightStorage::instance_add_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance) {
	FFLight *light = _get_light(p_light);
	if (!light || !p_instance || p_instance->base_type != VS::INSTANCE_LIGHT || _clearing) {
		return ERR_INVALID_PARAMETER;
	}
	if (_registrations.has(p_instance)) {
		return ERR_ALREADY_IN_USE;
	}
	if (_next_generation == UINT64_MAX) {
		return ERR_OUT_OF_MEMORY;
	}
	const FFRegistration registration = { ++_next_generation, p_light };
	_registrations[p_instance] = registration;
	light->dependencies[p_instance] = registration.generation;
	light->dirty = true;
	return OK;
}

void FixedFunctionLightStorage::_free_light(FFLight *p_light) {
	_lights.erase(p_light);
	_light_owner.free(p_light->self);
	// Instances outlive their source RID until the frontend releases or rebinds them.
	for (Set<FFLightInstance *>::Element *E = p_light->instances.front(); E; E = E->next()) {
		E->get()->light = nullptr;
	}
	p_light->instances.clear();
	// Retire the RID before callbacks, retaining RID_Data for release-build owns().
	while (p_light->dependencies.front()) {
		const Map<RasterizerScene::InstanceBase *, uint64_t>::Element *owner = p_light->dependencies.front();
		RasterizerScene::InstanceBase *instance = owner->key();
		const uint64_t generation = owner->get();
		p_light->dependencies.erase(instance);
		const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *current = _registrations.find(instance);
		if (current && current->get().generation == generation) {
			instance->base_removed();
			current = _registrations.find(instance);
			if (current && current->get().generation == generation) {
				_registrations.erase(instance);
			}
		}
	}
	memdelete(p_light);
}

Error FixedFunctionLightStorage::instance_remove_dependency(RID p_light, RasterizerScene::InstanceBase *p_instance) {
	const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *registration = _registrations.find(p_instance);
	if (!registration || registration->get().light != p_light) {
		return ERR_INVALID_PARAMETER;
	}
	_registrations.erase(p_instance);
	FFLight *light = _get_light(p_light);
	if (light) {
		light->dependencies.erase(p_instance);
	}
	return OK;
}

Error FixedFunctionLightStorage::update_dependencies() {
	if (_notifying || _clearing) {
		return ERR_BUSY;
	}
	Vector<FFDependencyEvent> pending;
	for (List<FFLight *>::Element *E = _lights.front(); E; E = E->next()) {
		FFLight *light = E->get();
		if (!light->dirty) {
			continue;
		}
		for (Map<RasterizerScene::InstanceBase *, uint64_t>::Element *I = light->dependencies.front(); I; I = I->next()) {
			const FFDependencyEvent event = { I->get(), light->self, I->key() };
			if (pending.push_back(event)) {
				return ERR_OUT_OF_MEMORY;
			}
		}
	}
	for (List<FFLight *>::Element *E = _lights.front(); E; E = E->next()) {
		E->get()->dirty = false;
	}
	_notifying = true;
	for (int i = 0; i < pending.size(); i++) {
		const FFDependencyEvent event = pending[i];
		const Map<RasterizerScene::InstanceBase *, FFRegistration>::Element *current = _registrations.find(event.instance);
		if (current && current->get().generation == event.generation && current->get().light == event.light) {
			event.instance->base_changed(true, false);
		}
	}
	_notifying = false;
	for (List<FFLight *>::Element *E = _lights.front(); E; E = E->next()) {
		if (E->get()->dirty) {
			return ERR_SKIP;
		}
	}
	return OK;
}
