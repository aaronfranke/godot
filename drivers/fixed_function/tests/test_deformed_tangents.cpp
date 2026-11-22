#include "test_scene_cpu.h"

#include <stdio.h>
#include <string.h>

#define FF_TANGENT_CHECK(m_condition)                                            \
	do {                                                                         \
		if (!(m_condition)) {                                                    \
			printf("Deformed tangent FAIL at %d: %s\n", __LINE__, #m_condition); \
			return false;                                                        \
		}                                                                        \
	} while (false)

bool TestFixedFunctionSceneCPU::_deformed_tangents() {
	// One uncompressed 3.x vertex: position, normal, tangent, four byte bones and weights.
	const float source[] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, -1, 0, 1, 0, 0, 0 };
	const uint32_t stride = sizeof(source);
	FixedFunctionMeshStorage::FFSurfaceData surface;
	surface.format = VS::ARRAY_FORMAT_VERTEX | VS::ARRAY_FORMAT_NORMAL | VS::ARRAY_FORMAT_TANGENT | VS::ARRAY_FORMAT_BONES | VS::ARRAY_FORMAT_WEIGHTS;
	surface.vertex_count = 1;
	surface.offsets[VS::ARRAY_NORMAL] = 12;
	surface.offsets[VS::ARRAY_TANGENT] = 24;
	surface.offsets[VS::ARRAY_BONES] = 40;
	surface.offsets[VS::ARRAY_WEIGHTS] = 44;
	for (int i = 0; i < VS::ARRAY_MAX; i++) {
		surface.strides[i] = stride;
	}
	surface.array.resize(stride);
	{
		PoolVector<uint8_t>::Write write = surface.array.write();
		memcpy(write.ptr(), source, stride);
	}
	PoolVector<float> weights;
	weights.push_back(0.5f);
	Vector<Transform> bones;
	Transform bone;
	bone.basis.set_axis(0, Vector3(-2, 0, 0));
	bone.basis.set_axis(1, Vector3(0.5, 1, 0));
	bone.basis.set_axis(2, Vector3(0, 0, 3));
	bone.origin = Vector3(1, 2, 3);
	bones.push_back(bone);
	for (int relative = 0; relative < 2; relative++) {
		float target[] = { 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0 };
		if (relative) {
			target[5] = 0; // Normal delta.
			target[6] = -1; // Tangent delta from +X to +Y; its sign is not morphed.
			target[9] = target[11] = 0;
		}
		PoolVector<uint8_t> shape;
		shape.resize(stride);
		{
			PoolVector<uint8_t>::Write write = shape.write();
			memcpy(write.ptr(), target, stride);
		}
		FixedFunctionSceneRenderer::FFDrawPacket packet;
		packet.surface = surface;
		packet.surface.blend_shapes.push_back(shape);
		const VS::BlendShapeMode mode = relative ? VS::BLEND_SHAPE_MODE_RELATIVE : VS::BLEND_SHAPE_MODE_NORMALIZED;
		FF_TANGENT_CHECK(FixedFunctionSceneRenderer::_prepare_deformation(packet, bones, weights, mode, true) == OK);
		FF_TANGENT_CHECK(packet.deformed_vertices.size() == 1 && packet.deformed_tangents.size() == 1);
		const FixedFunctionSceneRenderer::FFDeformedVertex &vertex = packet.deformed_vertices[0];
		const FixedFunctionSceneRenderer::FFDeformedTangentFrame &frame = packet.deformed_tangents[0];
		FF_TANGENT_CHECK(vertex.position == Vector3(1, 2, 3) && vertex.normal == Vector3(0, 0, 1));
		FF_TANGENT_CHECK(frame.tangent.distance_to(Vector3(-1.5, 1, 0).normalized()) < real_t(0.00001));
		FF_TANGENT_CHECK(frame.bitangent.distance_to(Vector3(-2.5, -1, 0).normalized()) < real_t(0.00001));
		// Shear makes these nonorthogonal: reconstructing B after skinning loses information.
		FF_TANGENT_CHECK(Math::abs(frame.tangent.dot(frame.bitangent)) > real_t(0.1));
		FixedFunctionSceneRenderer::FFDrawPacket ordinary;
		ordinary.surface = packet.surface;
		FF_TANGENT_CHECK(FixedFunctionSceneRenderer::_prepare_deformation(ordinary, bones, weights, mode) == OK);
		FF_TANGENT_CHECK(ordinary.deformed_tangents.empty() && ordinary.deformed_vertices[0].position == vertex.position && ordinary.deformed_vertices[0].normal == vertex.normal);
	}
	{
		const PoolVector<uint8_t>::Read unchanged = surface.array.read();
		FF_TANGENT_CHECK(memcmp(unchanged.ptr(), source, stride) == 0);
	}
	printf("PASS: CPU tangent morphs, base sign preservation, reflected/sheared skinning, optional allocation and immutable source.\n");
	return true;
}

#undef FF_TANGENT_CHECK
