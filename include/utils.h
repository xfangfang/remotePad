#include <Common.h>
#include <stdbool.h>

#define GOLDHEN_PATH "/data/GoldHEN"

#define TEX_ICON_SYSTEM "cxml://psnotification/tex_icon_system"
#define MAX_PATH_ 260

#define STR_IMPL(x) #x
#define STR(x) STR_IMPL(x)

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

#if (__FINAL__) == 1
#define BUILD_TYPE "(Release)"
#define debug_printf(a, args...)
#else
#define BUILD_TYPE "(Debug)"
#define debug_printf(a, args...) klog("[%s] (%s:%d) " a,  __func__,__FILE__, __LINE__, ##args)
#endif

#define HOOK_DEFINE(func, ...) \
    HOOK_INIT(func); \
    typedef int32_t (*func##_def)(__VA_ARGS__); \
    int32_t func(__VA_ARGS__); \
    int32_t func##_hook(__VA_ARGS__)

#define HOOK_PASS(func, ...) \
    HOOK_CONTINUE(func, func##_def, __VA_ARGS__)

#ifndef SCE_OK
#define SCE_OK 0
#endif

#define final_printf(a, args...) klog("\033[32m(%s:%d)\033[0m " a, __FILE__, __LINE__, ##args)

#define print_proc_info() {\
    final_printf("process info\n");\
    final_printf("pid: %d\n", procInfo.pid);\
    final_printf("name: %s\n", procInfo.name);\
    final_printf("path: %s\n", procInfo.path);\
    final_printf("titleid: %s\n", procInfo.titleid);\
    final_printf("contentid: %s\n", procInfo.contentid);\
    final_printf("version: %s\n", procInfo.version);\
    final_printf("base_address: 0x%lx\n", procInfo.base_address);\
}

void Notify(const char* IconUri, const char *FMT, ...);

int32_t load_prx(const char *name, bool syscall);

// Takes hardcoded input string 2 to strlen against during compile time.
// startsWith(input_1, "input 2");
#define startsWith(str1, str2) (strncmp(str1, str2, __builtin_strlen(str2)) == 0)

bool file_exists(const char *filename);
