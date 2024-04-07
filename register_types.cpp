#include "register_types.h"

#include "audio_stream_qoa.h"

#ifdef TOOLS_ENABLED
#include "core/config/engine.h"
#include "core/version.h"
#include "resource_importer_qoa.h"
#endif

void initialize_qoa_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		Ref<ResourceImporterQOA> qoa_import;
		qoa_import.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(qoa_import);
	}

#if VERSION_MAJOR >= 4
#if VERSION_MINOR >= 3
	ClassDB::APIType prev_api = ClassDB::get_current_api();
	ClassDB::set_current_api(ClassDB::API_EDITOR);
#if VERSION_MINOR >= 2
	// Required to document import options in the class reference.
	GDREGISTER_CLASS(ResourceImporterQOA);
#endif // VERSION_MINOR >= 2
	ClassDB::set_current_api(prev_api);
#endif // VERSION_MINOR >= 3
#endif // VERSION_MAJOR >= 4
#endif // TOOLS_ENABLED

	GDREGISTER_CLASS(AudioStreamQOA);
}

void uninitialize_qoa_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
