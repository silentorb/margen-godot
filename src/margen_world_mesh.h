#pragma once

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/timer.hpp>

namespace godot {

class MargenWorldMesh : public Node3D {
	GDCLASS(MargenWorldMesh, Node3D);

	static constexpr double REGEN_DEBOUNCE_SEC = 0.3;

	int seed = 7;
	int max_blocks = 20;
	Vector3 cell_size = Vector3(2.0f, 2.0f, 2.0f);
	bool auto_generate = true;

	NodePath mesh_instance_path;
	MeshInstance3D *mesh_instance = nullptr;
	Timer *regen_timer = nullptr;

	void ensure_regen_timer();
	void schedule_regenerate();
	void _on_regen_debounce_timeout();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	MargenWorldMesh();
	~MargenWorldMesh() override = default;

	void _ready() override;

	void set_seed(int p_seed);
	int get_seed() const;

	void set_max_blocks(int p_max_blocks);
	int get_max_blocks() const;

	void set_cell_size(const Vector3 &p_cell_size);
	Vector3 get_cell_size() const;

	void set_auto_generate(bool p_auto_generate);
	bool get_auto_generate() const;

	void set_mesh_instance_path(const NodePath &p_path);
	NodePath get_mesh_instance_path() const;

	void generate();
	void clear_mesh();
};

} // namespace godot
