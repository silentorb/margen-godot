#pragma once

#include <margen.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/variant/rid.hpp>

#include <vector>

namespace godot {

class MargenWorldMesh : public Node3D {
	GDCLASS(MargenWorldMesh, Node3D);

	static constexpr double REGEN_DEBOUNCE_SEC = 0.3;
	static constexpr int ANNOTATION_PATHS = 1 << 0;
	static constexpr int ANNOTATION_CELL_BOUNDARIES = 1 << 1;
	static constexpr int HORIZONTAL_DIRECTION_COUNT = 18;
	static constexpr int FLAT_DIRECTION_COUNT = 6;

	int seed = 7;
	int max_blocks = 20;
	Vector3 cell_size = Vector3(3.0f, 1.0f, 3.0f);
	bool auto_generate = true;
	int annotations = 0;

	NodePath mesh_instance_path;
	MeshInstance3D *mesh_instance = nullptr;
	Timer *regen_timer = nullptr;

	std::vector<MargenCell> cached_cells;
	RID path_mesh_rid;
	RID path_instance_rid;
	Ref<StandardMaterial3D> path_material;
	RID boundary_mesh_rid;
	RID boundary_instance_rid;
	Ref<StandardMaterial3D> boundary_material;

	void ensure_regen_timer();
	void schedule_regenerate();
	void _on_regen_debounce_timeout();

	void clear_path_visualization();
	void update_path_visualization();
	void sync_path_instance_transform();

	void clear_boundary_visualization();
	void update_boundary_visualization();
	void sync_boundary_instance_transform();
	void update_annotation_visualizations();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	MargenWorldMesh();
	~MargenWorldMesh() override;

	void _ready() override;

	void set_seed(int p_seed);
	int get_seed() const;

	void set_max_blocks(int p_max_blocks);
	int get_max_blocks() const;

	void set_cell_size(const Vector3 &p_cell_size);
	Vector3 get_cell_size() const;

	void set_auto_generate(bool p_auto_generate);
	bool get_auto_generate() const;

	void set_annotations(int p_annotations);
	int get_annotations() const;

	void set_mesh_instance_path(const NodePath &p_path);
	NodePath get_mesh_instance_path() const;

	void generate();
	void clear_mesh();
};

} // namespace godot
