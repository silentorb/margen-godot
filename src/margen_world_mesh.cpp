#include "margen_world_mesh.h"

#include <margen.h>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/base_material3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdint>
#include <unordered_set>
#include <vector>

using namespace godot;

namespace {

struct CellKey {
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	bool operator==(const CellKey &other) const {
		return x == other.x && y == other.y && z == other.z;
	}
};

struct CellKeyHash {
	size_t operator()(const CellKey &key) const {
		const size_t hx = static_cast<size_t>(static_cast<uint32_t>(key.x));
		const size_t hy = static_cast<size_t>(static_cast<uint32_t>(key.y));
		const size_t hz = static_cast<size_t>(static_cast<uint32_t>(key.z));
		return hx * 73856093u ^ hy * 19349663u ^ hz * 83492791u;
	}
};

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

// Margen is right-handed Z-up (+Y north); Godot is right-handed Y-up.
// Position map (det +1): (x, y, z) -> (x, z, -y). See margen docs/coordinates.md.
Vector3 margen_to_godot(const MargenVec3 &v) {
	return Vector3(v.x, v.z, -v.y);
}

// Godot cell_size (sx, sy_up, sz) -> margen (sx, sz, sy_up).
Vector3 cell_size_to_margen(const Vector3 &godot_cell_size) {
	return Vector3(godot_cell_size.x, godot_cell_size.z, godot_cell_size.y);
}

Vector3 margen_point_to_godot(
		const MargenVec3 &center,
		const MargenVec3 &point,
		const Vector3 &half_size_margen) {
	const MargenVec3 world{
		center.x + point.x * half_size_margen.x,
		center.y + point.y * half_size_margen.y,
		center.z + point.z * half_size_margen.z,
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

Node3D *ensure_actors_root(Node3D *owner) {
	if (Node3D *existing = Object::cast_to<Node3D>(owner->get_node_or_null("Actors"))) {
		return existing;
	}
	Node3D *actors = memnew(Node3D);
	actors->set_name("Actors");
	owner->add_child(actors);
	if (owner->is_inside_tree()) {
		actors->set_owner(owner->get_owner());
	}
	return actors;
}

void clear_actors(Node3D *owner) {
	Node3D *actors = Object::cast_to<Node3D>(owner->get_node_or_null("Actors"));
	if (actors == nullptr) {
		return;
	}
	while (actors->get_child_count() > 0) {
		Node *child = actors->get_child(0);
		actors->remove_child(child);
		child->queue_free();
	}
}

void clear_world_collision(Node3D *owner) {
	StaticBody3D *body = Object::cast_to<StaticBody3D>(owner->get_node_or_null("WorldCollision"));
	if (body == nullptr) {
		return;
	}
	owner->remove_child(body);
	body->queue_free();
}

void apply_world_collision(Node3D *owner, MeshInstance3D *instance) {
	Ref<Mesh> mesh;
	if (instance != nullptr) {
		mesh = instance->get_mesh();
	}
	if (mesh.is_null()) {
		clear_world_collision(owner);
		return;
	}

	Ref<ConcavePolygonShape3D> trimesh = mesh->create_trimesh_shape();
	if (trimesh.is_null()) {
		UtilityFunctions::push_warning("MargenWorldMesh: create_trimesh_shape returned null");
		clear_world_collision(owner);
		return;
	}

	StaticBody3D *body = Object::cast_to<StaticBody3D>(owner->get_node_or_null("WorldCollision"));
	if (body == nullptr) {
		body = memnew(StaticBody3D);
		body->set_name("WorldCollision");
		owner->add_child(body);
		if (owner->is_inside_tree()) {
			body->set_owner(owner->get_owner());
		}
	}

	CollisionShape3D *shape_node =
			Object::cast_to<CollisionShape3D>(body->get_node_or_null("CollisionShape3D"));
	if (shape_node == nullptr) {
		shape_node = memnew(CollisionShape3D);
		shape_node->set_name("CollisionShape3D");
		// Hide the trimesh debug gizmo by default — it coincides with the render mesh and z-fights.
		// Physics still works; toggle Visible on CollisionShape3D in the editor when inspecting collision.
		shape_node->set_visible(false);
		body->add_child(shape_node);
		if (owner->is_inside_tree()) {
			shape_node->set_owner(owner->get_owner());
		}
	}

	shape_node->set_shape(trimesh);
}

void apply_actor_placements(
		Node3D *owner,
		const Vector3 &godot_cell_size,
		const MargenActorPlacement *placements,
		size_t placement_count) {
	clear_actors(owner);
	if (placements == nullptr || placement_count == 0) {
		return;
	}

	Node3D *actors = ensure_actors_root(owner);
	const Vector3 margen_cell_size = cell_size_to_margen(godot_cell_size);
	const uint32_t player_spawn_id = margen_placeable_player_spawn();
	// Small lift above the cell center so the marker sits clearly above the floor plane.
	constexpr float spawn_lift_godot_y = 0.1f;

	for (size_t i = 0; i < placement_count; ++i) {
		const MargenActorPlacement &placement = placements[i];
		if (placement.placeable_id != player_spawn_id) {
			continue;
		}

		MargenVec3 center{};
		if (margen_resolve_cell_center(
					placement.location_x,
					placement.location_y,
					placement.location_z,
					margen_cell_size.x,
					margen_cell_size.y,
					margen_cell_size.z,
					&center) != MARGEN_OK) {
			UtilityFunctions::push_warning(
					"MargenWorldMesh: failed to resolve player spawn cell center");
			continue;
		}

		Node3D *spawn = memnew(Node3D);
		spawn->set_name("PlayerSpawnLocation");
		spawn->add_to_group("player_spawn");
		spawn->set_position(margen_to_godot(center) + Vector3(0.0f, spawn_lift_godot_y, 0.0f));
		actors->add_child(spawn);
		if (owner->is_inside_tree()) {
			spawn->set_owner(owner->get_owner());
		}
	}
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
	ClassDB::bind_method(D_METHOD("set_annotations", "annotations"), &MargenWorldMesh::set_annotations);
	ClassDB::bind_method(D_METHOD("get_annotations"), &MargenWorldMesh::get_annotations);
	ClassDB::bind_method(D_METHOD("set_mesh_instance_path", "mesh_instance_path"), &MargenWorldMesh::set_mesh_instance_path);
	ClassDB::bind_method(D_METHOD("get_mesh_instance_path"), &MargenWorldMesh::get_mesh_instance_path);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_blocks"), "set_max_blocks", "get_max_blocks");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "cell_size"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_generate"), "set_auto_generate", "get_auto_generate");
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "annotations", PROPERTY_HINT_FLAGS, "Paths,CellBoundaries"),
			"set_annotations",
			"get_annotations");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mesh_instance_path"), "set_mesh_instance_path", "get_mesh_instance_path");
}

MargenWorldMesh::MargenWorldMesh() = default;

MargenWorldMesh::~MargenWorldMesh() {
	clear_path_visualization();
	clear_boundary_visualization();
}

void MargenWorldMesh::ensure_regen_timer() {
	if (regen_timer != nullptr) {
		return;
	}
	regen_timer = memnew(Timer);
	regen_timer->set_name("RegenDebounceTimer");
	regen_timer->set_one_shot(true);
	regen_timer->set_wait_time(REGEN_DEBOUNCE_SEC);
	regen_timer->set_process_mode(PROCESS_MODE_ALWAYS);
	add_child(regen_timer, false, INTERNAL_MODE_FRONT);
	regen_timer->connect("timeout", callable_mp(this, &MargenWorldMesh::_on_regen_debounce_timeout));
}

void MargenWorldMesh::schedule_regenerate() {
	if (!is_node_ready()) {
		return;
	}
	ensure_regen_timer();
	regen_timer->start(REGEN_DEBOUNCE_SEC);
}

void MargenWorldMesh::_on_regen_debounce_timeout() {
	if (!is_inside_tree()) {
		return;
	}
	generate();
}

void MargenWorldMesh::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (regen_timer != nullptr) {
			regen_timer->stop();
		}
		clear_path_visualization();
		clear_boundary_visualization();
	} else if (p_what == NOTIFICATION_ENTER_TREE) {
		update_annotation_visualizations();
	} else if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		sync_path_instance_transform();
		sync_boundary_instance_transform();
	}
}

void MargenWorldMesh::set_seed(int p_seed) {
	if (seed == p_seed) {
		return;
	}
	seed = p_seed;
	schedule_regenerate();
}

int MargenWorldMesh::get_seed() const {
	return seed;
}

void MargenWorldMesh::set_max_blocks(int p_max_blocks) {
	const int clamped = p_max_blocks < 1 ? 1 : p_max_blocks;
	if (max_blocks == clamped) {
		return;
	}
	max_blocks = clamped;
	schedule_regenerate();
}

int MargenWorldMesh::get_max_blocks() const {
	return max_blocks;
}

void MargenWorldMesh::set_cell_size(const Vector3 &p_cell_size) {
	if (cell_size == p_cell_size) {
		return;
	}
	cell_size = p_cell_size;
	schedule_regenerate();
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

void MargenWorldMesh::set_annotations(int p_annotations) {
	if (annotations == p_annotations) {
		return;
	}
	annotations = p_annotations;
	const int needs_cells = ANNOTATION_PATHS | ANNOTATION_CELL_BOUNDARIES;
	if ((annotations & needs_cells) != 0 && cached_cells.empty() && is_inside_tree()) {
		generate();
		return;
	}
	update_annotation_visualizations();
}

int MargenWorldMesh::get_annotations() const {
	return annotations;
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

void MargenWorldMesh::clear_path_visualization() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		path_mesh_rid = RID();
		path_instance_rid = RID();
		return;
	}
	if (path_instance_rid.is_valid()) {
		rs->free_rid(path_instance_rid);
		path_instance_rid = RID();
	}
	if (path_mesh_rid.is_valid()) {
		rs->free_rid(path_mesh_rid);
		path_mesh_rid = RID();
	}
}

void MargenWorldMesh::sync_path_instance_transform() {
	if (!path_instance_rid.is_valid()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	rs->instance_set_transform(path_instance_rid, get_global_transform());
}

void MargenWorldMesh::update_annotation_visualizations() {
	update_path_visualization();
	update_boundary_visualization();
}

void MargenWorldMesh::clear_boundary_visualization() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		boundary_mesh_rid = RID();
		boundary_instance_rid = RID();
		return;
	}
	if (boundary_instance_rid.is_valid()) {
		rs->free_rid(boundary_instance_rid);
		boundary_instance_rid = RID();
	}
	if (boundary_mesh_rid.is_valid()) {
		rs->free_rid(boundary_mesh_rid);
		boundary_mesh_rid = RID();
	}
}

void MargenWorldMesh::sync_boundary_instance_transform() {
	if (!boundary_instance_rid.is_valid()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	rs->instance_set_transform(boundary_instance_rid, get_global_transform());
}

void MargenWorldMesh::update_boundary_visualization() {
	clear_boundary_visualization();

	if ((annotations & ANNOTATION_CELL_BOUNDARIES) == 0 || !is_inside_tree()) {
		return;
	}
	if (cached_cells.empty()) {
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}

	Ref<World3D> world = get_world_3d();
	if (world.is_null()) {
		UtilityFunctions::push_warning("MargenWorldMesh: boundary viz skipped (no World3D)");
		return;
	}

	std::unordered_set<CellKey, CellKeyHash> occupancy;
	occupancy.reserve(cached_cells.size());
	for (const MargenCell &cell : cached_cells) {
		occupancy.insert(CellKey{ cell.x, cell.y, cell.z });
	}

	const Vector3 margen_cell_size = cell_size_to_margen(cell_size);
	const Color boundary_color(0.15f, 0.35f, 1.0f, 0.9f);

	PackedVector3Array vertices;
	PackedColorArray colors;

	MargenIVec3 flat_dirs[FLAT_DIRECTION_COUNT]{};
	for (int direction = 0; direction < FLAT_DIRECTION_COUNT; ++direction) {
		if (margen_resolve_grid_direction(direction, &flat_dirs[direction]) != MARGEN_OK) {
			UtilityFunctions::push_warning("MargenWorldMesh: grid direction resolve failed");
			return;
		}
	}

	for (const MargenCell &cell : cached_cells) {
		MargenVec3 corners[MARGEN_HEX_CELL_CORNER_COUNT]{};
		if (margen_resolve_default_cell_corners(
					cell.x,
					cell.y,
					cell.z,
					margen_cell_size.x,
					margen_cell_size.y,
					margen_cell_size.z,
					corners) != MARGEN_OK) {
			continue;
		}

		const bool has_cell_below =
				occupancy.find(CellKey{ cell.x, cell.y, cell.z - 1 }) != occupancy.end();

		for (int i = 0; i < FLAT_DIRECTION_COUNT; ++i) {
			const int next = (i + 1) % FLAT_DIRECTION_COUNT;
			if (!has_cell_below) {
				vertices.push_back(margen_to_godot(corners[i]));
				vertices.push_back(margen_to_godot(corners[next]));
				colors.push_back(boundary_color);
				colors.push_back(boundary_color);
			}

			const CellKey neighbor_a{
				cell.x + flat_dirs[i].x,
				cell.y + flat_dirs[i].y,
				cell.z + flat_dirs[i].z,
			};
			const CellKey neighbor_b{
				cell.x + flat_dirs[next].x,
				cell.y + flat_dirs[next].y,
				cell.z + flat_dirs[next].z,
			};
			if (occupancy.find(neighbor_a) == occupancy.end()
					&& occupancy.find(neighbor_b) == occupancy.end()) {
				vertices.push_back(margen_to_godot(corners[i]));
				vertices.push_back(margen_to_godot(corners[i + 6]));
				colors.push_back(boundary_color);
				colors.push_back(boundary_color);
			}
		}
	}

	if (vertices.is_empty()) {
		return;
	}

	if (boundary_material.is_null()) {
		boundary_material.instantiate();
		boundary_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		boundary_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		boundary_material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		boundary_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		boundary_material->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		boundary_material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		boundary_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
		boundary_material->set_render_priority(126);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_COLOR] = colors;

	boundary_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(boundary_mesh_rid, RenderingServer::PRIMITIVE_LINES, arrays);
	rs->mesh_surface_set_material(boundary_mesh_rid, 0, boundary_material->get_rid());

	boundary_instance_rid = rs->instance_create();
	rs->instance_set_base(boundary_instance_rid, boundary_mesh_rid);
	rs->instance_set_scenario(boundary_instance_rid, world->get_scenario());
	rs->instance_set_transform(boundary_instance_rid, get_global_transform());
	rs->instance_set_visible(boundary_instance_rid, true);

	set_notify_transform(true);
}

void MargenWorldMesh::update_path_visualization() {
	clear_path_visualization();

	if ((annotations & ANNOTATION_PATHS) == 0 || !is_inside_tree()) {
		return;
	}
	if (cached_cells.empty()) {
		return;
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}

	Ref<World3D> world = get_world_3d();
	if (world.is_null()) {
		UtilityFunctions::push_warning("MargenWorldMesh: path viz skipped (no World3D)");
		return;
	}

	const Vector3 margen_cell_size = cell_size_to_margen(cell_size);
	const Vector3 half_spoke = margen_cell_size * 0.45f;
	// Lift spokes above the floor plane so they read clearly (UE used a small Z bump + foreground draw).
	const Vector3 height_offset_scaled(0.0f, 0.0f, 0.25f * margen_cell_size.z);

	// Near-opaque: thin PRIMITIVE_LINES are easy to miss at 50% alpha.
	const Color flat_color(0.15f, 0.95f, 0.2f, 0.95f);
	const Color slope_color(0.1f, 0.9f, 0.85f, 0.95f);

	PackedVector3Array vertices;
	PackedColorArray colors;

	for (const MargenCell &cell : cached_cells) {
		if (cell.paths == 0) {
			continue;
		}

		MargenVec3 center{};
		if (margen_resolve_cell_center(
					cell.x,
					cell.y,
					cell.z,
					margen_cell_size.x,
					margen_cell_size.y,
					margen_cell_size.z,
					&center) != MARGEN_OK) {
			continue;
		}

		center.x += height_offset_scaled.x;
		center.y += height_offset_scaled.y;
		center.z += height_offset_scaled.z;

		for (int direction = 0; direction < HORIZONTAL_DIRECTION_COUNT; ++direction) {
			if ((cell.paths & (1u << direction)) == 0) {
				continue;
			}

			MargenVec3 dir{};
			if (margen_resolve_direction_vector(direction, &dir) != MARGEN_OK) {
				continue;
			}

			const MargenVec3 end{
				center.x + dir.x * half_spoke.x,
				center.y + dir.y * half_spoke.y,
				center.z + dir.z * half_spoke.z,
			};

			const Color color = direction < 6 ? flat_color : slope_color;
			vertices.push_back(margen_to_godot(center));
			vertices.push_back(margen_to_godot(end));
			colors.push_back(color);
			colors.push_back(color);
		}
	}

	if (vertices.is_empty()) {
		UtilityFunctions::push_warning("MargenWorldMesh: path viz enabled but no path bits in grid");
		return;
	}

	if (path_material.is_null()) {
		path_material.instantiate();
		path_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		path_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		path_material->set_flag(BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		path_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
		path_material->set_flag(BaseMaterial3D::FLAG_DISABLE_FOG, true);
		// Match Unreal foreground debug lines: draw on top of solid world geometry.
		path_material->set_flag(BaseMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
		path_material->set_depth_draw_mode(BaseMaterial3D::DEPTH_DRAW_DISABLED);
		path_material->set_render_priority(127);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_COLOR] = colors;

	path_mesh_rid = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(path_mesh_rid, RenderingServer::PRIMITIVE_LINES, arrays);
	rs->mesh_surface_set_material(path_mesh_rid, 0, path_material->get_rid());

	path_instance_rid = rs->instance_create();
	rs->instance_set_base(path_instance_rid, path_mesh_rid);
	rs->instance_set_scenario(path_instance_rid, world->get_scenario());
	rs->instance_set_transform(path_instance_rid, get_global_transform());
	rs->instance_set_visible(path_instance_rid, true);

	set_notify_transform(true);
}

void MargenWorldMesh::clear_mesh() {
	MeshInstance3D *instance = resolve_mesh_instance(this, mesh_instance_path, mesh_instance);
	if (instance != nullptr) {
		instance->set_mesh(Ref<ArrayMesh>());
	}
	clear_world_collision(this);
	cached_cells.clear();
	clear_path_visualization();
	clear_boundary_visualization();
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

	const Vector3 margen_cell_size = cell_size_to_margen(cell_size);
	const Vector3 half_size = margen_cell_size * 0.5f;

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
						margen_cell_size.x,
						margen_cell_size.y,
						margen_cell_size.z,
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

			// Godot ArrayMesh front faces are clockwise when viewed from the front
			// (default CULL_BACK). Margen face IR is RH-outward (CCW from outside).
			// Reverse fan order and derive the normal from that order so exterior
			// faces are front-facing. See margen docs/coordinates.md.
			const Vector3 normal = (face_vertices[2] - face_vertices[0])
					.cross(face_vertices[1] - face_vertices[0])
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
				indices.push_back(vertex_offset + static_cast<int>(tri) + 1);
				indices.push_back(vertex_offset + static_cast<int>(tri));
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

	const MargenActorPlacement *placements = nullptr;
	size_t placement_count = 0;
	if (margen_world_faces_placements_bulk(handle, &placements, &placement_count) != MARGEN_OK) {
		UtilityFunctions::push_warning(
				"MargenWorldMesh: margen_world_faces_placements_bulk failed: ",
				margen_last_error() != nullptr ? String(margen_last_error()) : String("unknown"));
		placements = nullptr;
		placement_count = 0;
	}

	// Copy placements and cells before freeing the world handle (pointers are handle-owned).
	std::vector<MargenActorPlacement> placement_copy;
	if (placements != nullptr && placement_count > 0) {
		placement_copy.assign(placements, placements + placement_count);
	}

	const MargenCell *cells = nullptr;
	size_t cell_count = 0;
	if (margen_world_faces_cells_bulk(handle, &cells, &cell_count) != MARGEN_OK) {
		UtilityFunctions::push_warning(
				"MargenWorldMesh: margen_world_faces_cells_bulk failed: ",
				margen_last_error() != nullptr ? String(margen_last_error()) : String("unknown"));
		cells = nullptr;
		cell_count = 0;
	}
	cached_cells.clear();
	if (cells != nullptr && cell_count > 0) {
		cached_cells.assign(cells, cells + cell_count);
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

	apply_world_collision(this, instance);

	apply_actor_placements(
			this,
			cell_size,
			placement_copy.empty() ? nullptr : placement_copy.data(),
			placement_copy.size());

	update_annotation_visualizations();
}
