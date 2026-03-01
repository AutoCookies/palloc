/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// ------------------------------------------------------------------------
// mi prefixed publi definitions of various Posix, Unix, and C++ functions
// for convenience and used when overriding these functions.
// ------------------------------------------------------------------------
#include "palloc.h"
#include "palloc/internal.h"

// ------------------------------------------------------
// Posix & Unix functions definitions
// ------------------------------------------------------

#include <errno.h>
#include <string.h>  // memset
#include <stdlib.h>  // getenv

#ifdef _MSC_VER
#pragma warning(disable:4996)  // getenv _wgetenv
#endif

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif


pa_decl_nodiscard size_t pa_malloc_size(const void* p) pa_attr_noexcept {
  // if (!pa_is_in_heap_region(p)) return 0;
  return pa_usable_size(p);
}

pa_decl_nodiscard size_t pa_malloc_usable_size(const void *p) pa_attr_noexcept {
  // if (!pa_is_in_heap_region(p)) return 0;
  return pa_usable_size(p);
}

pa_decl_nodiscard size_t pa_malloc_good_size(size_t size) pa_attr_noexcept {
  return pa_good_size(size);
}

void pa_cfree(void* p) pa_attr_noexcept {
  if (pa_is_in_heap_region(p)) {
    pa_free(p);
  }
}

int pa_posix_memalign(void** p, size_t alignment, size_t size) pa_attr_noexcept {
  // Note: The spec dictates we should not modify `*p` on an error. (issue#27)
  // <http://man7.org/linux/man-pages/man3/posix_memalign.3.html>
  if (p == NULL) return EINVAL;
  if ((alignment % sizeof(void*)) != 0) return EINVAL;                 // natural alignment
  // it is also required that alignment is a power of 2 and > 0; this is checked in `pa_malloc_aligned`
  if (alignment==0 || !_pa_is_power_of_two(alignment)) return EINVAL;  // not a power of 2
  void* q = pa_malloc_aligned(size, alignment);
  if (q==NULL && size != 0) return ENOMEM;
  pa_assert_internal(((uintptr_t)q % alignment) == 0);
  *p = q;
  return 0;
}

pa_decl_nodiscard pa_decl_restrict void* pa_memalign(size_t alignment, size_t size) pa_attr_noexcept {
  void* p = pa_malloc_aligned(size, alignment);
  pa_assert_internal(((uintptr_t)p % alignment) == 0);
  return p;
}

pa_decl_nodiscard pa_decl_restrict void* pa_valloc(size_t size) pa_attr_noexcept {
  return pa_memalign( _pa_os_page_size(), size );
}

pa_decl_nodiscard pa_decl_restrict void* pa_pvalloc(size_t size) pa_attr_noexcept {
  size_t psize = _pa_os_page_size();
  if (size >= SIZE_MAX - psize) return NULL; // overflow
  size_t asize = _pa_align_up(size, psize);
  return pa_malloc_aligned(asize, psize);
}

pa_decl_nodiscard pa_decl_restrict void* pa_aligned_alloc(size_t alignment, size_t size) pa_attr_noexcept {
  // C11 requires the size to be an integral multiple of the alignment, see <https://en.cppreference.com/w/c/memory/aligned_alloc>.
  // unfortunately, it turns out quite some programs pass a size that is not an integral multiple so skip this check..
  /* if pa_unlikely((size & (alignment - 1)) != 0) { // C11 requires alignment>0 && integral multiple, see <https://en.cppreference.com/w/c/memory/aligned_alloc>
      #if PA_DEBUG > 0
      _pa_error_message(EOVERFLOW, "(pa_)aligned_alloc requires the size to be an integral multiple of the alignment (size %zu, alignment %zu)\n", size, alignment);
      #endif
      return NULL;
    }
  */
  // C11 also requires alignment to be a power-of-two (and > 0) which is checked in pa_malloc_aligned
  void* p = pa_malloc_aligned(size, alignment);
  pa_assert_internal(((uintptr_t)p % alignment) == 0);
  return p;
}

pa_decl_nodiscard void* pa_reallocarray( void* p, size_t count, size_t size ) pa_attr_noexcept {  // BSD
  void* newp = pa_reallocn(p,count,size);
  if (newp==NULL) { errno = ENOMEM; }
  return newp;
}

pa_decl_nodiscard int pa_reallocarr( void* p, size_t count, size_t size ) pa_attr_noexcept { // NetBSD
  pa_assert(p != NULL);
  if (p == NULL) {
    errno = EINVAL;
    return EINVAL;
  }
  void** op = (void**)p;
  void* newp = pa_reallocarray(*op, count, size);
  if pa_unlikely(newp == NULL) { return errno; }
  *op = newp;
  return 0;
}

void* pa__expand(void* p, size_t newsize) pa_attr_noexcept {  // Microsoft
  void* res = pa_expand(p, newsize);
  if (res == NULL) { errno = ENOMEM; }
  return res;
}

pa_decl_nodiscard pa_decl_restrict unsigned short* pa_wcsdup(const unsigned short* s) pa_attr_noexcept {
  if (s==NULL) return NULL;
  size_t len;
  for(len = 0; s[len] != 0; len++) { }
  size_t size = (len+1)*sizeof(unsigned short);
  unsigned short* p = (unsigned short*)pa_malloc(size);
  if (p != NULL) {
    _pa_memcpy(p,s,size);
  }
  return p;
}

pa_decl_nodiscard pa_decl_restrict unsigned char* pa_mbsdup(const unsigned char* s)  pa_attr_noexcept {
  return (unsigned char*)pa_strdup((const char*)s);
}

int pa_dupenv_s(char** buf, size_t* size, const char* name) pa_attr_noexcept {
  if (buf==NULL || name==NULL) return EINVAL;
  if (size != NULL) *size = 0;
  char* p = getenv(name);        // mscver warning 4996
  if (p==NULL) {
    *buf = NULL;
  }
  else {
    *buf = pa_strdup(p);
    if (*buf==NULL) return ENOMEM;
    if (size != NULL) *size = _pa_strlen(p);
  }
  return 0;
}

int pa_wdupenv_s(unsigned short** buf, size_t* size, const unsigned short* name) pa_attr_noexcept {
  if (buf==NULL || name==NULL) return EINVAL;
  if (size != NULL) *size = 0;
#if !defined(_WIN32) || (defined(WINAPI_FAMILY) && (WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP))
  // not supported
  *buf = NULL;
  return EINVAL;
#else
  unsigned short* p = (unsigned short*)_wgetenv((const wchar_t*)name);  // msvc warning 4996
  if (p==NULL) {
    *buf = NULL;
  }
  else {
    *buf = pa_wcsdup(p);
    if (*buf==NULL) return ENOMEM;
    if (size != NULL) *size = wcslen((const wchar_t*)p);
  }
  return 0;
#endif
}

pa_decl_nodiscard void* pa_aligned_offset_recalloc(void* p, size_t newcount, size_t size, size_t alignment, size_t offset) pa_attr_noexcept { // Microsoft
  return pa_recalloc_aligned_at(p, newcount, size, alignment, offset);
}

pa_decl_nodiscard void* pa_aligned_recalloc(void* p, size_t newcount, size_t size, size_t alignment) pa_attr_noexcept { // Microsoft
  return pa_recalloc_aligned(p, newcount, size, alignment);
}
