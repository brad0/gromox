/*
 *  define the constant for plugin's return value load, unload, reload actions.
 */
#pragma once
#include <string>
#include <gromox/common_types.hpp>

/* enumeration for indicate the ation of plugin_main function */
enum{
    PLUGIN_INIT,
    PLUGIN_FREE,
    PLUGIN_THREAD_CREATE,
	PLUGIN_THREAD_DESTROY,
	PLUGIN_RELOAD,
	PLUGIN_EARLY_INIT,
	PLUGIN_REPORT,
};

/* enumeration for the return value of xxx_load_library */
enum{
	PLUGIN_FAIL_EXECUTEMAIN = -5,
    PLUGIN_FAIL_ALLOCNODE,
    PLUGIN_NO_MAIN,
    PLUGIN_FAIL_OPEN,
    PLUGIN_ALREADY_LOADED,
    PLUGIN_LOAD_OK = 0,
};

/* enumeration for the return value of xxx_unload_library */
enum{
    PLUGIN_UNABLE_UNLOAD = -3,
    PLUGIN_SYSTEM_ERROR,
    PLUGIN_NOT_FOUND,
    PLUGIN_UNLOAD_OK = 0,
};

using PLUGIN_MAIN = BOOL (*)(int, void **);
using PLUGIN_DMAIN = BOOL (int, void **);

extern "C" GX_EXPORT PLUGIN_DMAIN
	HOOK_alias_resolve, HOOK_exmdb_local, HOOK_remote_delivery;

namespace gromox {

struct service_node {
	void *service_addr = nullptr;
	std::string service_name;
};

struct static_module {
	std::string path;
	PLUGIN_MAIN efunc;
};

struct generic_module {
	generic_module() = default;
	generic_module(generic_module &&) noexcept;
	~generic_module();
	void operator=(generic_module &&) noexcept = delete;

	std::string file_name;
	void *handle = nullptr;
	PLUGIN_MAIN lib_main = nullptr;
	bool completed_init = false;
};

}
