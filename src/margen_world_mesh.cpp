#include "margen_world_mesh.h"

#include <margen.h>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <vector>

using namespace godot;

namespace {

Color slot_color(int slot_index) {
	static const Color palette[] = {
		Color(0.55f, 0.55f, 0.58f),
		Color(0.35f, 0.65f, 0.38f),
		Color(0.35f, 0.42f, 0.78f),
		Color(0.78f, 0.52f, 0.28f),
	};
	if (slot_index < 0) {
		return Color(0.9f, 0.2f, 0.2f);
	}
	return palette[slot_index % 4];
}

// Margen uses Z-up (Unreal-style); Godot uses Y-up.
Vector3 margen_to_godot(const MargenVec3 &v) {
	return Vector3(v.x, v.z, v.y);
}

Vector3 margen_point_to_godot(
		const MargenVec3 &center,
		const MargenVec3 &point,
		const Vector3 &half_size) {
	const MargenVec3 world{
		center.x + point.x * half_size.x,
		center.y + point.y * half_size.y,
		center.z + point.z * half_size.z,
	};
	return margen_to_godot(world);
}

MeshInstance3D *resolve_mesh_instance(Node3D *owner, const NodePath &path, MeshInstance3D *cached) {
	if (cached != nullptr) {
		return cached;
	}
	if (path.is_empty()) {
		return Object::cast_to<MeshInstance3D>(owner->get_node_or_null("MeshInstance3D"));
	}
	return Object::cast_to<MeshInstance3D>(owner->get_node_or_null(path));
}

} // namespace

void MargenWorldMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("generate"), &MargenWorldMesh::generate);
	ClassDB::bind_method(D_METHOD("clear_mesh"), &MargenWorldMesh::clear_mesh);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &MargenWorldMesh::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &MargenWorldMesh::get_seed);
	ClassDB::bind_method(D_METHOD("set_max_blocks", "max_blocks"), &MargenWorldMesh::set_max_blocks);
	ClassDB::bind_method(D_METHOD("get_max_blocks"), &MargenWorldMesh::get_max_blocks);
	ClassDB::bind_method(D_METHOD("set_cell_size", "cell_size"), &MargenWorldMesh::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &MargenWorldMesh::get_cell_size);
	ClassDB::bind_method(D_METHOD("set_auto_generate", "auto_generate"), &MargenWorldMesh::set_auto_generate);
	ClassDB::bind_method(D_METHOD("get_auto_generate"), &MargenWorldMesh::get_auto_generate);
	ClassDB::bind_method(D_METHOD("set_mesh_instance_path", "mesh_instance_path"), &MargenWorldMesh::set_mesh_instance_path);
	ClassDB::bind_method(D_METHOD("get_mesh_instance_path"), &MargenWorldMesh::get_mesh_instance_path);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_blocks"), "set_max_blocks", "get_max_blocks");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "cell_size"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_generate"), "set_auto_generate", "get_auto_generate");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mesh_instance_path"), "set_mesh_instance_path", "get_mesh_instance_path");
}

MargenWorldMesh::MargenWorldMesh() = default;

void MargenWorldMesh::set_seed(int p_seed) {
	seed = p_seed;
}

int MargenWorldMesh::get_seed() const {
	return seed;
}

void MargenWorldMesh::set_max_blocks(int p_max_blocks) {
	max_blocks = p_max_blocks;
}

int MargenWorldMesh::get_max_blocks() const {
	return max_blocks;
}

void MargenWorldMesh::set_cell_size(const Vector3 &p_cell_size) {
	cell_size = p_cell_size;
}

Vector3 MargenWorldMesh::get_cell_size() const {
	return cell_size;
}

void MargenWorldMesh::set_auto_generate(bool p_auto_generate) {
	auto_generate = p_auto_generate;
}

bool MargenWorldMesh::get_auto_generate() const {
	return auto_generate;
}

void MargenWorldMesh::set_mesh_instance_path(const NodePath &p_path) {
	mesh_instance_path = p_path;
}

NodePath MargenWorldMesh::get_mesh_instance_path() const {
	return mesh_instance_path;
}

void MargenWorldMesh::_ready() {
	if (auto_generate) {
		generate();
	}
}

void MargenWorldMesh::clear_mesh() {
	MeshInstance3D *instance = resolve_mesh_instance(this, mesh_instance_path, mesh_instance);
	if (instance == nullptr) {
		return;
	}
	instance->set_mesh(Ref<ArrayMesh>());
}

void MargenWorldMesh::generate() {
	MargenWorldFaces *handle = nullptr;
	const MargenStatus status = margen_generate_world_faces(seed, max_blocks, &handle);
	if (status != MARGEN_OK || handle == nullptr) {
		const char *err = margen_last_error();
		UtilityFunctions::push_error(
				"MargenWorldMesh: margen_generate_world_faces failed: ",
				err != nullptr ? String(err) : String("unknown error"));
		return;
	}

	const MargenVec3 *bulk_points = nullptr;
	const MargenVec2 *bulk_uvs = nullptr;
	const MargenBulkFaceDesc *bulk_faces = nullptr;
	size_t bulk_point_count = 0;
	size_t bulk_uv_count = 0;
	size_t bulk_face_count = 0;
	if (margen_world_faces_bulk(
				handle,
				&bulk_points,
				&bulk_point_count,
				&bulk_uvs,
				&bulk_uv_count,
				&bulk_faces,
				&bulk_face_count) != MARGEN_OK
			|| bulk_faces == nullptr) {
		UtilityFunctions::push_error(
				"MargenWorldMesh: margen_world_faces_bulk failed: ",
				margen_last_error() != nullptr ? String(margen_last_error()) : String("unknown"));
		margen_world_faces_free(handle);
		return;
	}

	const size_t material_count = margen_world_faces_material_count(handle);
	if (bulk_face_count == 0) {
		UtilityFunctions::push_warning("MargenWorldMesh: generation returned zero faces");
		margen_world_faces_free(handle);
		return;
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	const Vector3 half_size = cell_size * 0.5f;

	for (size_t material_index = 0; material_index < material_count; ++material_index) {
		MargenMaterialSlot slot{};
		if (margen_world_faces_get_material(handle, material_index, &slot) != MARGEN_OK) {
			continue;
		}

		PackedVector3Array vertices;
		PackedVector3Array normals;
		PackedVector2Array uvs;
		PackedInt32Array indices;

		int vertex_offset = 0;

		for (size_t face_index = 0; face_index < bulk_face_count; ++face_index) {
			const MargenBulkFaceDesc &face = bulk_faces[face_index];
			if (face.material_id != slot.slot_index) {
				continue;
			}
			if (face.point_count < 3) {
				continue;
			}
			if (static_cast<size_t>(face.point_begin) + face.point_count > bulk_point_count) {
				continue;
			}

			MargenVec3 center{};
			if (margen_resolve_cell_center(
						face.location_x,
						face.location_y,
						face.location_z,
						cell_size.x,
						cell_size.y,
						cell_size.z,
						&center) != MARGEN_OK) {
				continue;
			}

			std::vector<Vector3> face_vertices;
			face_vertices.reserve(face.point_count);

			for (uint32_t point_index = 0; point_index < face.point_count; ++point_index) {
				const MargenVec3 &point = bulk_points[face.point_begin + point_index];
				face_vertices.push_back(margen_point_to_godot(center, point, half_size));
			}

			if (face_vertices.size() < 3) {
				continue;
			}

			const Vector3 normal = (face_vertices[1] - face_vertices[0])
					.cross(face_vertices[2] - face_vertices[0])
					.normalized();

			for (size_t point_index = 0; point_index < face_vertices.size(); ++point_index) {
				vertices.push_back(face_vertices[point_index]);
				normals.push_back(normal);

				if (point_index < face.uv_count
						&& static_cast<size_t>(face.uv_begin) + point_index < bulk_uv_count) {
					const MargenVec2 &uv = bulk_uvs[face.uv_begin + point_index];
					uvs.push_back(Vector2(uv.x, uv.y));
				} else {
					uvs.push_back(Vector2(0.0f, 0.0f));
				}
			}

			for (size_t tri = 1; tri + 1 < face_vertices.size(); ++tri) {
				indices.push_back(vertex_offset);
				indices.push_back(vertex_offset + static_cast<int>(tri));
				indices.push_back(vertex_offset + static_cast<int>(tri) + 1);
			}

			vertex_offset += static_cast<int>(face_vertices.size());
		}

		if (vertices.is_empty()) {
			continue;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = vertices;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_INDEX] = indices;

		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

		Ref<StandardMaterial3D> material;
		material.instantiate();
		material->set_albedo(slot_color(slot.slot_index));
		mesh->surface_set_material(mesh->get_surface_count() - 1, material);
	}

	margen_world_faces_free(handle);

	MeshInstance3D *instance = resolve_mesh_instance(this, mesh_instance_path, mesh_instance);
	if (instance == nullptr) {
		instance = memnew(MeshInstance3D);
		instance->set_name("MeshInstance3D");
		add_child(instance);
		if (is_inside_tree()) {
			instance->set_owner(get_owner());
		}
		mesh_instance = instance;
	}

	instance->set_mesh(mesh);
}
