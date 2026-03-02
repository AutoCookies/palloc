/*
 * palloc-redirect.dll -- Windows redirection module for palloc.
 *
 * palloc.dll imports pa_allocator_init and pa_allocator_done from this DLL.
 * This module is loaded as a dependency of palloc.dll and notifies palloc
 * via _pa_redirect_entry. For full malloc/free override it must also hook
 * the C runtime (e.g. ucrtbase.dll) to redirect malloc/free to pa_malloc/pa_free;
 * that hooking is not included in this stub (override will not work until
 * IAT hooking is implemented or a prebuilt redirect is used).
 *
 * Build with: see bin/BUILD.md and bin/CMakeLists.txt
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* palloc.dll exports this; we call it from DllMain */
typedef void (__cdecl *pa_redirect_entry_fn)(DWORD reason);

static HMODULE pa_dll = NULL;

__declspec(dllexport) bool __cdecl pa_allocator_init(const char** message) {
  if (message) *message = NULL;
  return true;
}

__declspec(dllexport) void __cdecl pa_allocator_done(void) {
  /* nothing */
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
  (void)hModule;
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {
    pa_dll = GetModuleHandleW(L"palloc.dll");
    if (pa_dll) {
      pa_redirect_entry_fn entry = (pa_redirect_entry_fn)
        GetProcAddress(pa_dll, "_pa_redirect_entry");
      if (entry)
        entry(DLL_PROCESS_ATTACH);
    }
  } else if (reason == DLL_PROCESS_DETACH && pa_dll) {
    pa_redirect_entry_fn entry = (pa_redirect_entry_fn)
      GetProcAddress(pa_dll, "_pa_redirect_entry");
    if (entry)
      entry(DLL_PROCESS_DETACH);
    pa_dll = NULL;
  } else if (reason == DLL_THREAD_DETACH && pa_dll) {
    pa_redirect_entry_fn entry = (pa_redirect_entry_fn)
      GetProcAddress(pa_dll, "_pa_redirect_entry");
    if (entry)
      entry(DLL_THREAD_DETACH);
  }
  return TRUE;
}
