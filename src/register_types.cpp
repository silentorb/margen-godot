#include "register_types.h"

#include "margen_world_mesh.h"

#include <cstdio>

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifndef MARGEN_GODOT_API_VERSION
#define MARGEN_GODOT_API_VERSION "unknown"
#endif

using namespace godot;

void initialize_margen_godot_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(MargenWorldMesh);
	UtilityFunctions::push_warning(
			"margen_godot: initialized (MargenWorldMesh registered, api=",
			MARGEN_GODOT_API_VERSION,
			")");
}

void uninitialize_margen_godot_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT margen_godot_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	// Early log before UtilityFunctions is available: host may capture stderr.
	fprintf(stderr, "margen_godot: library_init entered (api=%s)\n", MARGEN_GODOT_API_VERSION);
	fflush(stderr);

	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
	init_obj.register_initializer(initialize_margen_godot_module);
	init_obj.register_terminator(uninitialize_margen_godot_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
	const GDExtensionBool ok = init_obj.init();
	fprintf(stderr, "margen_godot: library_init returned %s\n", ok ? "true" : "false");
	fflush(stderr);
	return ok;
}
}
