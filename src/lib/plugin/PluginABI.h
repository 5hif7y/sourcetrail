#ifndef SOURCETRAIL_PLUGIN_ABI_H
#define SOURCETRAIL_PLUGIN_ABI_H

/*
 * C ABI v1 for dynamically loaded Sourcetrail plugins.
 *
 * A plugin exports:
 *   const st_plugin_descriptor* sourcetrail_plugin_descriptor(void);
 *
 * The descriptor and every pointer it contains must stay valid until the
 * plugin is unloaded. Effects registered through host->add_effect are disposed
 * before the library is closed.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  define ST_PLUGIN_EXPORT __declspec(dllexport)
#else
#  define ST_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define ST_PLUGIN_ABI_VERSION 1

typedef struct st_plugin_manifest
{
	uint32_t abi_version;
	const char* id;
	const char* version;
	const char* name;
	const char* const* inject;
	size_t inject_count;
	int unloadable;
} st_plugin_manifest;

typedef struct st_plugin_host_api
{
	void* user_data;

	/* Registers a reversible effect. `dispose(effect_data)` runs on unload.
	 * Returns 0 on success. */
	int (*add_effect)(void* user_data, void (*dispose)(void*), void* effect_data);

	/* Optional diagnostics channel; may be NULL. */
	void (*log)(void* user_data, const char* message);
} st_plugin_host_api;

typedef int (*st_plugin_apply_fn)(st_plugin_host_api* host);

typedef struct st_plugin_descriptor
{
	uint32_t abi_version;
	st_plugin_manifest manifest;
	st_plugin_apply_fn apply;
} st_plugin_descriptor;

#ifdef __cplusplus
}
#endif

#endif	  // SOURCETRAIL_PLUGIN_ABI_H
