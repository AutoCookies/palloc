/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#if !defined(PA_IN_ALLOC_C)
#error "this file should be included from 'alloc.c' (so aliases can work)"
#endif

#if defined(PA_MALLOC_OVERRIDE) && defined(_WIN32) && !(defined(PA_SHARED_LIB) && defined(_DLL))
#error "It is only possible to override "malloc" on Windows when building as a DLL (and linking the C runtime as a DLL)"
#endif

#if defined(PA_MALLOC_OVERRIDE) && !(defined(_WIN32))

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
pa_decl_externc void   vfree(void* p);
pa_decl_externc size_t malloc_size(const void* p);
pa_decl_externc size_t malloc_good_size(size_t size);
#endif

// helper definition for C override of C++ new
typedef void* pa_nothrow_t;

// ------------------------------------------------------
// Override system malloc
// ------------------------------------------------------

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__) && !PA_TRACK_ENABLED
  // gcc, clang: use aliasing to alias the exported function to one of our `pa_` functions
  #if (defined(__GNUC__) && __GNUC__ >= 9)
    #pragma GCC diagnostic ignored "-Wattributes"  // or we get warnings that nodiscard is ignored on a forward
    #define PA_FORWARD(fun)      __attribute__((alias(#fun), used, visibility("default"), copy(fun)));
  #else
    #define PA_FORWARD(fun)      __attribute__((alias(#fun), used, visibility("default")));
  #endif
  #define PA_FORWARD1(fun,x)      PA_FORWARD(fun)
  #define PA_FORWARD2(fun,x,y)    PA_FORWARD(fun)
  #define PA_FORWARD3(fun,x,y,z)  PA_FORWARD(fun)
  #define PA_FORWARD0(fun,x)      PA_FORWARD(fun)
  #define PA_FORWARD02(fun,x,y)   PA_FORWARD(fun)
#else
  // otherwise use forwarding by calling our `pa_` function
  #define PA_FORWARD1(fun,x)      { return fun(x); }
  #define PA_FORWARD2(fun,x,y)    { return fun(x,y); }
  #define PA_FORWARD3(fun,x,y,z)  { return fun(x,y,z); }
  #define PA_FORWARD0(fun,x)      { fun(x); }
  #define PA_FORWARD02(fun,x,y)   { fun(x,y); }
#endif


#if defined(__APPLE__) && defined(PA_SHARED_LIB_EXPORT) && defined(PA_OSX_INTERPOSE)
  // define PA_OSX_IS_INTERPOSED as we should not provide forwarding definitions for
  // functions that are interposed (or the interposing does not work)
  #define PA_OSX_IS_INTERPOSED

  pa_decl_externc size_t pa_malloc_size_checked(void *p) {
    if (!pa_is_in_heap_region(p)) return 0;
    return pa_usable_size(p);
  }

  // use interposing so `DYLD_INSERT_LIBRARIES` works without `DYLD_FORCE_FLAT_NAMESPACE=1`
  // See: <https://books.google.com/books?id=K8vUkpOXhN4C&pg=PA73>
  struct pa_interpose_s {
    const void* replacement;
    const void* target;
  };
  #define PA_INTERPOSE_FUN(oldfun,newfun) { (const void*)&newfun, (const void*)&oldfun }
  #define PA_INTERPOSE_MI(fun)            PA_INTERPOSE_FUN(fun,pa_##fun)

  #define PA_INTERPOSE_DECLS(name)        __attribute__((used)) static struct pa_interpose_s name[]  __attribute__((section("__DATA, __interpose")))

  PA_INTERPOSE_DECLS(_pa_interposes) =
  {
    PA_INTERPOSE_MI(malloc),
    PA_INTERPOSE_MI(calloc),
    PA_INTERPOSE_MI(realloc),
    PA_INTERPOSE_MI(strdup),
    PA_INTERPOSE_MI(realpath),
    PA_INTERPOSE_MI(posix_memalign),
    PA_INTERPOSE_MI(reallocf),
    PA_INTERPOSE_MI(valloc),
    PA_INTERPOSE_FUN(malloc_size,pa_malloc_size_checked),
    PA_INTERPOSE_MI(malloc_good_size),
    #ifdef PA_OSX_ZONE
    // we interpose malloc_default_zone in alloc-override-osx.c so we can use pa_free safely
    PA_INTERPOSE_MI(free),
    PA_INTERPOSE_FUN(vfree,pa_free),
    #else
    // sometimes code allocates from default zone but deallocates using plain free :-( (like NxHashResizeToCapacity <https://github.com/nneonneo/osx-10.9-opensource/blob/master/objc4-551.1/runtime/hashtable2.mm>)
    PA_INTERPOSE_FUN(free,pa_cfree), // use safe free that checks if pointers are from us
    PA_INTERPOSE_FUN(vfree,pa_cfree),
    #endif
  };
  PA_INTERPOSE_DECLS(_pa_interposes_10_7) __OSX_AVAILABLE(10.7) = {
    PA_INTERPOSE_MI(strndup),
  };
  PA_INTERPOSE_DECLS(_pa_interposes_10_15) __OSX_AVAILABLE(10.15) = {
    PA_INTERPOSE_MI(aligned_alloc),
  };

  #ifdef __cplusplus
  extern "C" {
  #endif
  void  _ZdlPv(void* p);   // delete
  void  _ZdaPv(void* p);   // delete[]
  void  _ZdlPvm(void* p, size_t n);  // delete
  void  _ZdaPvm(void* p, size_t n);  // delete[]
  void* _Znwm(size_t n);  // new
  void* _Znam(size_t n);  // new[]
  void* _ZnwmRKSt9nothrow_t(size_t n, pa_nothrow_t tag); // new nothrow
  void* _ZnamRKSt9nothrow_t(size_t n, pa_nothrow_t tag); // new[] nothrow
  #ifdef __cplusplus
  }
  #endif
  __attribute__((used)) static struct pa_interpose_s _pa_cxx_interposes[]  __attribute__((section("__DATA, __interpose"))) =
  {
    PA_INTERPOSE_FUN(_ZdlPv,pa_free),
    PA_INTERPOSE_FUN(_ZdaPv,pa_free),
    PA_INTERPOSE_FUN(_ZdlPvm,pa_free_size),
    PA_INTERPOSE_FUN(_ZdaPvm,pa_free_size),
    PA_INTERPOSE_FUN(_Znwm,pa_new),
    PA_INTERPOSE_FUN(_Znam,pa_new),
    PA_INTERPOSE_FUN(_ZnwmRKSt9nothrow_t,pa_new_nothrow),
    PA_INTERPOSE_FUN(_ZnamRKSt9nothrow_t,pa_new_nothrow),
  };

#elif defined(_MSC_VER)
  // cannot override malloc unless using a dll.
  // we just override new/delete which does work in a static library.
#else
  // On all other systems forward allocation primitives to our API
  pa_decl_export void* malloc(size_t size)              PA_FORWARD1(pa_malloc, size)
  pa_decl_export void* calloc(size_t size, size_t n)    PA_FORWARD2(pa_calloc, size, n)
  pa_decl_export void* realloc(void* p, size_t newsize) PA_FORWARD2(pa_realloc, p, newsize)
  pa_decl_export void  free(void* p)                    PA_FORWARD0(pa_free, p)  
  // In principle we do not need to forward `strdup`/`strndup` but on some systems these do not use `malloc` internally (but a more primitive call)
  // We only override if `strdup` is not a macro (as on some older libc's, see issue #885)
  #if !defined(strdup)
  pa_decl_export char* strdup(const char* str)             PA_FORWARD1(pa_strdup, str)
  #endif
  #if !defined(strndup) && (!defined(__APPLE__) || (defined(MAC_OS_X_VERSION_10_7) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7))
  pa_decl_export char* strndup(const char* str, size_t n)  PA_FORWARD2(pa_strndup, str, n)   
  #endif
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__)
#pragma GCC visibility push(default)
#endif

// ------------------------------------------------------
// Override new/delete
// This is not really necessary as they usually call
// malloc/free anyway, but it improves performance.
// ------------------------------------------------------
#ifdef __cplusplus
  // ------------------------------------------------------
  // With a C++ compiler we override the new/delete operators.
  // see <https://en.cppreference.com/w/cpp/memory/new/operator_new>
  // ------------------------------------------------------
  #include <new>

  #ifndef PA_OSX_IS_INTERPOSED
    void operator delete(void* p) noexcept              PA_FORWARD0(pa_free,p)
    void operator delete[](void* p) noexcept            PA_FORWARD0(pa_free,p)

    void* operator new(std::size_t n) noexcept(false)   PA_FORWARD1(pa_new,n)
    void* operator new[](std::size_t n) noexcept(false) PA_FORWARD1(pa_new,n)

    void* operator new  (std::size_t n, const std::nothrow_t& tag) noexcept { PA_UNUSED(tag); return pa_new_nothrow(n); }
    void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept { PA_UNUSED(tag); return pa_new_nothrow(n); }

    #if (__cplusplus >= 201402L || _MSC_VER >= 1916)
    void operator delete  (void* p, std::size_t n) noexcept PA_FORWARD02(pa_free_size,p,n)
    void operator delete[](void* p, std::size_t n) noexcept PA_FORWARD02(pa_free_size,p,n)
    #endif
  #endif

  #if (__cplusplus > 201402L && defined(__cpp_aligned_new)) && (!defined(__GNUC__) || (__GNUC__ > 5))
  void operator delete  (void* p, std::align_val_t al) noexcept { pa_free_aligned(p, static_cast<size_t>(al)); }
  void operator delete[](void* p, std::align_val_t al) noexcept { pa_free_aligned(p, static_cast<size_t>(al)); }
  void operator delete  (void* p, std::size_t n, std::align_val_t al) noexcept { pa_free_size_aligned(p, n, static_cast<size_t>(al)); };
  void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept { pa_free_size_aligned(p, n, static_cast<size_t>(al)); };
  void operator delete  (void* p, std::align_val_t al, const std::nothrow_t&) noexcept { pa_free_aligned(p, static_cast<size_t>(al)); }
  void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept { pa_free_aligned(p, static_cast<size_t>(al)); }

  void* operator new( std::size_t n, std::align_val_t al)   noexcept(false) { return pa_new_aligned(n, static_cast<size_t>(al)); }
  void* operator new[]( std::size_t n, std::align_val_t al) noexcept(false) { return pa_new_aligned(n, static_cast<size_t>(al)); }
  void* operator new  (std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept { return pa_new_aligned_nothrow(n, static_cast<size_t>(al)); }
  void* operator new[](std::size_t n, std::align_val_t al, const std::nothrow_t&) noexcept { return pa_new_aligned_nothrow(n, static_cast<size_t>(al)); }
  #endif

#elif (defined(__GNUC__) || defined(__clang__))
  // ------------------------------------------------------
  // Override by defining the mangled C++ names of the operators (as
  // used by GCC and CLang).
  // See <https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling>
  // ------------------------------------------------------

  void _ZdlPv(void* p)            PA_FORWARD0(pa_free,p) // delete
  void _ZdaPv(void* p)            PA_FORWARD0(pa_free,p) // delete[]
  void _ZdlPvm(void* p, size_t n) PA_FORWARD02(pa_free_size,p,n)
  void _ZdaPvm(void* p, size_t n) PA_FORWARD02(pa_free_size,p,n)
  
  void _ZdlPvSt11align_val_t(void* p, size_t al)            { pa_free_aligned(p,al); }
  void _ZdaPvSt11align_val_t(void* p, size_t al)            { pa_free_aligned(p,al); }
  void _ZdlPvmSt11align_val_t(void* p, size_t n, size_t al) { pa_free_size_aligned(p,n,al); }
  void _ZdaPvmSt11align_val_t(void* p, size_t n, size_t al) { pa_free_size_aligned(p,n,al); }

  void _ZdlPvRKSt9nothrow_t(void* p, pa_nothrow_t tag)      { PA_UNUSED(tag); pa_free(p); }  // operator delete(void*, std::nothrow_t const&) 
  void _ZdaPvRKSt9nothrow_t(void* p, pa_nothrow_t tag)      { PA_UNUSED(tag); pa_free(p); }  // operator delete[](void*, std::nothrow_t const&)
  void _ZdlPvSt11align_val_tRKSt9nothrow_t(void* p, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); pa_free_aligned(p,al); } // operator delete(void*, std::align_val_t, std::nothrow_t const&) 
  void _ZdaPvSt11align_val_tRKSt9nothrow_t(void* p, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); pa_free_aligned(p,al); } // operator delete[](void*, std::align_val_t, std::nothrow_t const&) 
  
  #if (PA_INTPTR_SIZE==8)
    void* _Znwm(size_t n)                             PA_FORWARD1(pa_new,n)  // new 64-bit
    void* _Znam(size_t n)                             PA_FORWARD1(pa_new,n)  // new[] 64-bit
    void* _ZnwmRKSt9nothrow_t(size_t n, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_nothrow(n); }
    void* _ZnamRKSt9nothrow_t(size_t n, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_nothrow(n); }
    void* _ZnwmSt11align_val_t(size_t n, size_t al)   PA_FORWARD2(pa_new_aligned, n, al)
    void* _ZnamSt11align_val_t(size_t n, size_t al)   PA_FORWARD2(pa_new_aligned, n, al)
    void* _ZnwmSt11align_val_tRKSt9nothrow_t(size_t n, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_aligned_nothrow(n,al); }
    void* _ZnamSt11align_val_tRKSt9nothrow_t(size_t n, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_aligned_nothrow(n,al); }
  #elif (PA_INTPTR_SIZE==4)
    void* _Znwj(size_t n)                             PA_FORWARD1(pa_new,n)  // new 64-bit
    void* _Znaj(size_t n)                             PA_FORWARD1(pa_new,n)  // new[] 64-bit
    void* _ZnwjRKSt9nothrow_t(size_t n, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_nothrow(n); }
    void* _ZnajRKSt9nothrow_t(size_t n, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_nothrow(n); }
    void* _ZnwjSt11align_val_t(size_t n, size_t al)   PA_FORWARD2(pa_new_aligned, n, al)
    void* _ZnajSt11align_val_t(size_t n, size_t al)   PA_FORWARD2(pa_new_aligned, n, al)
    void* _ZnwjSt11align_val_tRKSt9nothrow_t(size_t n, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_aligned_nothrow(n,al); }
    void* _ZnajSt11align_val_tRKSt9nothrow_t(size_t n, size_t al, pa_nothrow_t tag) { PA_UNUSED(tag); return pa_new_aligned_nothrow(n,al); }
  #else
    #error "define overloads for new/delete for this platform (just for performance, can be skipped)"
  #endif
#endif // __cplusplus

// ------------------------------------------------------
// Further Posix & Unix functions definitions
// ------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PA_OSX_IS_INTERPOSED
  // Forward Posix/Unix calls as well
  void*  reallocf(void* p, size_t newsize) PA_FORWARD2(pa_reallocf,p,newsize)
  size_t malloc_size(const void* p)        PA_FORWARD1(pa_usable_size,p)
  #if !defined(__ANDROID__) && !defined(__FreeBSD__) && !defined(__DragonFly__)
  size_t malloc_usable_size(void *p)       PA_FORWARD1(pa_usable_size,p)
  #else
  size_t malloc_usable_size(const void *p) PA_FORWARD1(pa_usable_size,p)
  #endif

  // No forwarding here due to aliasing/name mangling issues
  void*  valloc(size_t size)               { return pa_valloc(size); }
  void   vfree(void* p)                    { pa_free(p); }
  size_t malloc_good_size(size_t size)     { return pa_malloc_good_size(size); }
  int    posix_memalign(void** p, size_t alignment, size_t size) { return pa_posix_memalign(p, alignment, size); }

  // `aligned_alloc` is only available when __USE_ISOC11 is defined.
  // Note: it seems __USE_ISOC11 is not defined in musl (and perhaps other libc's) so we only check
  // for it if using glibc.
  // Note: Conda has a custom glibc where `aligned_alloc` is declared `static inline` and we cannot
  // override it, but both _ISOC11_SOURCE and __USE_ISOC11 are undefined in Conda GCC7 or GCC9.
  // Fortunately, in the case where `aligned_alloc` is declared as `static inline` it
  // uses internally `memalign`, `posix_memalign`, or `_aligned_malloc` so we  can avoid overriding it ourselves.
  #if !defined(__GLIBC__) || __USE_ISOC11
  void* aligned_alloc(size_t alignment, size_t size) { return pa_aligned_alloc(alignment, size); }
  #endif
#endif

// no forwarding here due to aliasing/name mangling issues
void  cfree(void* p)                                    { pa_free(p); }
void* pvalloc(size_t size)                              { return pa_pvalloc(size); }
void* memalign(size_t alignment, size_t size)           { return pa_memalign(alignment, size); }
void* _aligned_malloc(size_t alignment, size_t size)    { return pa_aligned_alloc(alignment, size); }
void* reallocarray(void* p, size_t count, size_t size)  { return pa_reallocarray(p, count, size); }
// some systems define reallocarr so mark it as a weak symbol (#751)
pa_decl_weak int reallocarr(void* p, size_t count, size_t size)    { return pa_reallocarr(p, count, size); }

#if defined(__wasi__)
  // forward __libc interface (see PR #667)
  void* __libc_malloc(size_t size)                      PA_FORWARD1(pa_malloc, size)
  void* __libc_calloc(size_t count, size_t size)        PA_FORWARD2(pa_calloc, count, size)
  void* __libc_realloc(void* p, size_t size)            PA_FORWARD2(pa_realloc, p, size)
  void  __libc_free(void* p)                            PA_FORWARD0(pa_free, p)
  void* __libc_memalign(size_t alignment, size_t size)  { return pa_memalign(alignment, size); }

#elif defined(__linux__)
  // forward __libc interface (needed for glibc-based and musl-based Linux distributions)
  void* __libc_malloc(size_t size)                      PA_FORWARD1(pa_malloc,size)
  void* __libc_calloc(size_t count, size_t size)        PA_FORWARD2(pa_calloc,count,size)
  void* __libc_realloc(void* p, size_t size)            PA_FORWARD2(pa_realloc,p,size)
  void  __libc_free(void* p)                            PA_FORWARD0(pa_free,p)
  void  __libc_cfree(void* p)                           PA_FORWARD0(pa_free,p)

  void* __libc_valloc(size_t size)                      { return pa_valloc(size); }
  void* __libc_pvalloc(size_t size)                     { return pa_pvalloc(size); }
  void* __libc_memalign(size_t alignment, size_t size)  { return pa_memalign(alignment,size); }
  int   __posix_memalign(void** p, size_t alignment, size_t size) { return pa_posix_memalign(p,alignment,size); }
#endif

#ifdef __cplusplus
}
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__)
#pragma GCC visibility pop
#endif

#endif // PA_MALLOC_OVERRIDE && !_WIN32
