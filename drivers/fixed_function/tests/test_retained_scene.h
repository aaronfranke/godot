#pragma once

#include "drivers/fixed_function/scene_renderer.h"

#include <vector>

class TestFixedFunctionRetainedScene {
	friend class TestFixedFunctionRetainedLighting;
	class FFDevice : public FixedFunctionGraphicsDevice {
		FixedFunctionGraphicsDevice &_device;
		Error _creation_error;
		Error _draw_error;
		int _creations;
		int _retained_draws;
		int _transient_draws;
		int _occlusion_draws;
		int _occlusion_refuse_after;
		Error _occlusion_error;
		bool _transient_occlusion_enabled;
		bool _normal_mapping_disabled;
		bool _indexed_triangles_enabled = false;

	public:
		FFDevice(FixedFunctionGraphicsDevice &p_device) : _device(p_device), _creation_error(OK), _draw_error(OK), _creations(0), _retained_draws(0), _transient_draws(0), _occlusion_draws(0), _occlusion_refuse_after(-1), _occlusion_error(ERR_UNAVAILABLE), _transient_occlusion_enabled(false), _normal_mapping_disabled(false) {}
		void set_transient_occlusion_enabled(bool p_enabled) { _transient_occlusion_enabled = p_enabled; }
		void set_occlusion_refusal(int p_after, Error p_error = ERR_UNAVAILABLE) {
			_occlusion_draws = 0;
			_occlusion_refuse_after = p_after;
			_occlusion_error = p_error;
		}
		int get_occlusion_draws() const { return _occlusion_draws; }
		void set_normal_mapping_disabled(bool p_disabled) { _normal_mapping_disabled = p_disabled; }
		void set_indexed_triangles_enabled(bool p_enabled) { _indexed_triangles_enabled = p_enabled; }
		bool supports_indexed_triangles() const override { return _indexed_triangles_enabled && _device.supports_indexed_triangles(); }
		void set_creation_error(Error p_error) { _creation_error = p_error; }
		void set_draw_error(Error p_error) { _draw_error = p_error; }
		int get_creations() const { return _creations; }
		int get_retained_draws() const { return _retained_draws; }
		int get_transient_draws() const { return _transient_draws; }
		Error initialize(void *p_window, uint32_t p_width, uint32_t p_height, bool p_vsync) override { return _device.initialize(p_window, p_width, p_height, p_vsync); }
		Error finalize() override { return _device.finalize(); }
		Error resize(uint32_t p_width, uint32_t p_height) override { return _device.resize(p_width, p_height); }
		Error set_use_vsync(bool p_enabled) override { return _device.set_use_vsync(p_enabled); }
		Error prepare_resources() override { return _device.prepare_resources(); }
		const FFCapabilities &get_capabilities() const override { return _device.get_capabilities(); }
		uint32_t get_max_per_pixel_lights() const override { return _device.get_max_per_pixel_lights(); }
		bool supports_lit_geometry() const override { return _device.supports_lit_geometry(); }
		bool supports_normal_mapping() const override { return !_normal_mapping_disabled && _device.supports_normal_mapping(); }
		bool supports_normal_mapped_occlusion() const override { return supports_normal_mapping() && _device.supports_normal_mapped_occlusion(); }
		bool supports_transient_occlusion_mapping() const override { return _transient_occlusion_enabled && _device.supports_transient_occlusion_mapping(); }
		bool supports_occlusion_mapping() const override { return supports_lit_geometry() && _device.supports_occlusion_mapping(); }
		uint32_t get_width() const override { return _device.get_width(); }
		uint32_t get_height() const override { return _device.get_height(); }
		uint32_t get_target_width() const override { return _device.get_target_width(); }
		uint32_t get_target_height() const override { return _device.get_target_height(); }
		Error texture_create(const Ref<Image> &p_image, RID &r_texture, bool p_allow_during_frame = false) override { return _device.texture_create(p_image, r_texture, p_allow_during_frame); }
		Error texture_create_render_target(uint32_t p_width, uint32_t p_height, RID &r_texture, bool p_transparent = false) override { return _device.texture_create_render_target(p_width, p_height, r_texture, p_transparent); }
		bool texture_is_render_target_ready(RID p_texture) const override { return _device.texture_is_render_target_ready(p_texture); }
		Error texture_free(RID p_texture) override { return _device.texture_free(p_texture); }
		Error geometry_create(const Vector<FixedFunctionVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override;
		Error geometry_create_lod_batch(const FixedFunctionGeometryData::FFGeometryBatch &p_batch, RID &r_geometry) override {
			_creations++;
			return _creation_error == OK ? _device.geometry_create_lod_batch(p_batch, r_geometry) : _creation_error;
		}
		Error geometry_create_lit(const Vector<FixedFunctionLightingVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override {
			_creations++;
			return _creation_error == OK ? _device.geometry_create_lit(p_vertices, p_indices, r_geometry) : _creation_error;
		}
		Error draw_lit_geometry(RID p_geometry, const Projection &p_matrix, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters *p_occlusion, uint32_t p_lod) override {
			_retained_draws++;
			return _draw_error == OK ? _device.draw_lit_geometry(p_geometry, p_matrix, p_parameters, p_lighting, p_texture, p_state, p_occlusion, p_lod) : _draw_error;
		}
		Error draw_lit_triangles(const FixedFunctionLightingVertex *p_vertices, uint32_t p_count, const Projection &p_matrix, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const uint16_t *p_indices, uint32_t p_index_count) override {
			_transient_draws++;
			return _device.draw_lit_triangles(p_vertices, p_count, p_matrix, p_lighting, p_texture, p_state, p_indices, p_index_count);
		}
		Error geometry_create_normal_mapped(const Vector<FixedFunctionNormalMapVertex> &p_vertices, const Vector<uint32_t> &p_indices, RID &r_geometry) override {
			_creations++;
			return _creation_error == OK ? _device.geometry_create_normal_mapped(p_vertices, p_indices, r_geometry) : _creation_error;
		}
		Error draw_normal_mapped_geometry(RID p_geometry, const Projection &p_matrix, const FFLitGeometryParameters &p_parameters, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFNormalMapParameters &p_normal, const FFOcclusionMapParameters *p_occlusion, uint32_t p_lod) override {
			_retained_draws++;
			return _draw_error == OK ? _device.draw_normal_mapped_geometry(p_geometry, p_matrix, p_parameters, p_lighting, p_texture, p_state, p_normal, p_occlusion, p_lod) : _draw_error;
		}
		Error draw_occlusion_mapped_triangles(const FixedFunctionTransientOcclusionVertex *p_vertices, uint32_t p_count, const Projection &p_matrix, const FFPixelLighting &p_lighting, RID p_texture, const FFDrawParameters &p_state, const FFOcclusionMapParameters &p_occlusion, const uint16_t *p_indices, uint32_t p_index_count) override {
			_occlusion_draws++;
			if (_occlusion_refuse_after >= 0 && _occlusion_draws > _occlusion_refuse_after) {
				return _occlusion_error;
			}
			return _device.draw_occlusion_mapped_triangles(p_vertices, p_count, p_matrix, p_lighting, p_texture, p_state, p_occlusion, p_indices, p_index_count);
		}
		Error geometry_free(RID p_geometry) override { return _device.geometry_free(p_geometry); }
		Error geometry_get_draw_info(RID p_geometry, FFGeometryDrawInfo &r_info) const override { return _device.geometry_get_draw_info(p_geometry, r_info); }
		uint64_t get_geometry_memory_usage() const override { return _device.get_geometry_memory_usage(); }
		Error begin_frame(uint32_t p_color, RID p_target, bool p_clear, bool p_depth) override { return _device.begin_frame(p_color, p_target, p_clear, p_depth); }
		Error clear_color(uint32_t p_color) override { return _device.clear_color(p_color); }
		Error clear_depth(float p_depth) override { return _device.clear_depth(p_depth); }
		Error draw_triangles(const FixedFunctionVertex *p_vertices, uint32_t p_count, const Projection &p_matrix, RID p_texture, const FFDrawParameters &p_state) override;
		Error draw_geometry(RID p_geometry, const Projection &p_matrix, RID p_texture, const FFDrawParameters &p_state, const Color *p_color, uint32_t p_lod) override;
		Error end_frame(bool p_present, bool p_discard) override { return _device.end_frame(p_present, p_discard); }
		Error read_frame(uint32_t *p_pixels, size_t p_count) override { return _device.read_frame(p_pixels, p_count); }
	};
	struct FFInstance : FixedFunctionGeometryInstance {
		FFInstance(FixedFunctionRenderContext &p_context, RID p_mesh, RID p_material);
	};
	static RenderingServerTypes::RenderInfo _render_info;

	static bool _color_matches(uint32_t p_actual, uint32_t p_expected, int p_tolerance);
	static bool _add(FixedFunctionMeshStorage &p_meshes, RID p_mesh);
	static bool _draw(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, FFInstance **p_instances, int p_count, std::vector<uint32_t> &r_pixels, bool p_open = true, const Projection *p_projection = nullptr, const FixedFunctionSceneRenderer::FFMeshLOD *p_lod = nullptr);
	static bool _run(FixedFunctionGraphicsDevice &p_device, void *p_window);
	static bool _projection();
	static bool _test_clipped_projection(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_front_material, RID p_back_material);
	static bool _test_lods(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material);
	static bool _test_multimesh_lods(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_mesh, RID p_material, const FixedFunctionSceneRenderer::FFMeshLOD &p_lod);
	static bool _test_multimesh(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material);
	static bool _test_triangle_topology(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_billboards(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_points(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_attributes(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _test_spatial_blends(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer);
	static bool _benchmark(FixedFunctionRenderContext &p_context, FixedFunctionRenderTargetManager &p_targets, FixedFunctionSceneRenderer &p_renderer, RID p_material);

public:
	static bool test(FixedFunctionGraphicsDevice &p_device, void *p_window);
};
