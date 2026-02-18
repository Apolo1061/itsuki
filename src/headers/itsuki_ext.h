#ifndef ITSUKI_EXT_H
#define ITSUKI_EXT_H

/*
  Itsuki Native Extension API (v1)

  Goal:
  - Allow third-party modules compiled as shared libraries (.dll/.so) to be
    imported without modifying Itsuki's core `src/` for each module.

  Notes:
  - Extensions are expected to be compiled with a compatible compiler/ABI.
  - Do not call runtime symbols directly from the extension; use `ItsukiApi`.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "itsuki_types.h"

#define ITSUKI_EXT_API_VERSION 1u

#ifdef _WIN32
  #define ITSUKI_EXT_EXPORT __declspec(dllexport)
  #define ITSUKI_EXT_CALL __cdecl
#else
  #define ITSUKI_EXT_EXPORT __attribute__((visibility("default")))
  #define ITSUKI_EXT_CALL
#endif

typedef ObjModulo ItsukiModule;
typedef Result (*ItsukiNativeFn)(Result args[], int n_args);

typedef struct ItsukiApi {
    uint32_t version;

    /* Error reporting (non-varargs for ABI simplicity) */
    void (*raise)(TipoError tipo, const char* msg);

    /* Value constructors */
    Result (*make_number)(double n);
    Result (*make_bool)(int b);
    Result (*make_null)(void);
    Result (*make_string)(const char* s); /* GC-managed string */
    Result (*make_array)(void);
    Result (*make_map)(void);

    /* Container helpers */
    int (*array_push)(Result array, Result v);
    int (*map_set)(Result map, const char* key, Result v);

    /* Module exports */
    int (*export_const)(ItsukiModule* module, const char* name, Result v);
    int (*export_native)(ItsukiModule* module, const char* name, ItsukiNativeFn fn);
} ItsukiApi;

typedef int (ITSUKI_EXT_CALL *ItsukiModuleInitFn)(const ItsukiApi* api, ItsukiModule* module);

/*
  Required entrypoint for native modules.
  Return 1 on success, 0 on failure (the runtime will raise an error).
*/
ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module);

#ifdef __cplusplus
}
#endif

#endif

