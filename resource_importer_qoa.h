#ifndef RESOURCE_IMPORTER_QOA_H
#define RESOURCE_IMPORTER_QOA_H

#include "audio_stream_qoa.h"

#include "core/io/resource_importer.h"

class ResourceImporterQOA : public ResourceImporter {
	GDCLASS(ResourceImporterQOA, ResourceImporter);

public:
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;
	virtual float get_priority() const override { return 0.9; } // For WAVs, decrease priority so ResourceImporterWAV is used by default.

	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_idx) const override;

	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	static bool check_qoa(const String &p_path) { return p_path.get_extension().to_lower() == "qoa"; }
	static Ref<AudioStreamQOA> import_qoa(const String &p_path, const HashMap<StringName, Variant> &p_options);

	virtual Error import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

	ResourceImporterQOA();
};

#endif // RESOURCE_IMPORTER_QOA_H
