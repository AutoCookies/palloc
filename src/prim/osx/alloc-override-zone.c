/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#include "palloc.h"
#include "palloc/internal.h"

#if defined(PA_MALLOC_OVERRIDE)

#if !defined(__APPLE__)
#error "this file should only be included on macOS"
#endif

/* ------------------------------------------------------
   Override system malloc on macOS
   This is done through the malloc zone interface.
   It seems to be most robust in combination with interposing
   though or otherwise we may get zone errors as there are could
   be allocations done by the time we take over the
   zone.
------------------------------------------------------ */

#include <AvailabilityMacros.h>
#include <malloc/malloc.h>
#include <string.h>  // memset
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
// only available from OSX 10.6
extern malloc_zone_t* malloc_default_purgeable_zone(void) __attribute__((weak_import));
#endif

/* ------------------------------------------------------
   malloc zone members
------------------------------------------------------ */

static size_t zone_size(malloc_zone_t* zone, const void* p) {
  PA_UNUSED(zone);
  if (!pa_is_in_heap_region(p)){ return 0; } // not our pointer, bail out
  return pa_usable_size(p);
}

static void* zone_malloc(malloc_zone_t* zone, size_t size) {
  PA_UNUSED(zone);
  return pa_malloc(size);
}

static void* zone_calloc(malloc_zone_t* zone, size_t count, size_t size) {
  PA_UNUSED(zone);
  return pa_calloc(count, size);
}

static void* zone_valloc(malloc_zone_t* zone, size_t size) {
  PA_UNUSED(zone);
  return pa_malloc_aligned(size, _pa_os_page_size());
}

static void zone_free(malloc_zone_t* zone, void* p) {
  PA_UNUSED(zone);
  pa_cfree(p);
}

static void* zone_realloc(malloc_zone_t* zone, void* p, size_t newsize) {
  PA_UNUSED(zone);
  return pa_realloc(p, newsize);
}

static void* zone_memalign(malloc_zone_t* zone, size_t alignment, size_t size) {
  PA_UNUSED(zone);
  return pa_malloc_aligned(size,alignment);
}

static void zone_destroy(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  // todo: ignore for now?
}

static unsigned zone_batch_malloc(malloc_zone_t* zone, size_t size, void** ps, unsigned count) {
  unsigned i;
  for (i = 0; i < count; i++) {
    ps[i] = zone_malloc(zone, size);
    if (ps[i] == NULL) break;
  }
  return i;
}

static void zone_batch_free(malloc_zone_t* zone, void** ps, unsigned count) {
  for(size_t i = 0; i < count; i++) {
    zone_free(zone, ps[i]);
    ps[i] = NULL;
  }
}

static size_t zone_pressure_relief(malloc_zone_t* zone, size_t size) {
  PA_UNUSED(zone); PA_UNUSED(size);
  pa_collect(false);
  return 0;
}

static void zone_free_definite_size(malloc_zone_t* zone, void* p, size_t size) {
  PA_UNUSED(size);
  zone_free(zone,p);
}

static boolean_t zone_claimed_address(malloc_zone_t* zone, void* p) {
  PA_UNUSED(zone);
  return pa_is_in_heap_region(p);
}


/* ------------------------------------------------------
   Introspection members
------------------------------------------------------ */

static kern_return_t intro_enumerator(task_t task, void* p,
                            unsigned type_mask, vm_address_t zone_address,
                            memory_reader_t reader,
                            vm_range_recorder_t recorder)
{
  // todo: enumerate all memory
  PA_UNUSED(task); PA_UNUSED(p); PA_UNUSED(type_mask); PA_UNUSED(zone_address);
  PA_UNUSED(reader); PA_UNUSED(recorder);
  return KERN_SUCCESS;
}

static size_t intro_good_size(malloc_zone_t* zone, size_t size) {
  PA_UNUSED(zone);
  return pa_good_size(size);
}

static boolean_t intro_check(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  return true;
}

static void intro_print(malloc_zone_t* zone, boolean_t verbose) {
  PA_UNUSED(zone); PA_UNUSED(verbose);
  pa_stats_print(NULL);
}

static void intro_log(malloc_zone_t* zone, void* p) {
  PA_UNUSED(zone); PA_UNUSED(p);
  // todo?
}

static void intro_force_lock(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  // todo?
}

static void intro_force_unlock(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  // todo?
}

static void intro_statistics(malloc_zone_t* zone, malloc_statistics_t* stats) {
  PA_UNUSED(zone);
  // todo...
  stats->blocks_in_use = 0;
  stats->size_in_use = 0;
  stats->max_size_in_use = 0;
  stats->size_allocated = 0;
}

static boolean_t intro_zone_locked(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  return false;
}


/* ------------------------------------------------------
  At process start, override the default allocator
------------------------------------------------------ */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

static malloc_introspection_t pa_introspect = {
  .enumerator = &intro_enumerator,
  .good_size = &intro_good_size,
  .check = &intro_check,
  .print = &intro_print,
  .log = &intro_log,
  .force_lock = &intro_force_lock,
  .force_unlock = &intro_force_unlock,
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) && !defined(__ppc__)
  .statistics = &intro_statistics,
  .zone_locked = &intro_zone_locked,
#endif
};

static malloc_zone_t pa_malloc_zone = {
  // note: even with designators, the order is important for C++ compilation
  //.reserved1 = NULL,
  //.reserved2 = NULL,
  .size = &zone_size,
  .malloc = &zone_malloc,
  .calloc = &zone_calloc,
  .valloc = &zone_valloc,
  .free = &zone_free,
  .realloc = &zone_realloc,
  .destroy = &zone_destroy,
  .zone_name = "palloc",
  .batch_malloc = &zone_batch_malloc,
  .batch_free = &zone_batch_free,
  .introspect = &pa_introspect,
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) && !defined(__ppc__)
  #if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
  .version = 10,
  #else
  .version = 9,
  #endif
  // switch to version 9+ on OSX 10.6 to support memalign.
  .memalign = &zone_memalign,
  .free_definite_size = &zone_free_definite_size,
  #if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
  .pressure_relief = &zone_pressure_relief,
  #endif
  #if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
  .claimed_address = &zone_claimed_address,
  #endif
#else
  .version = 4,
#endif
};

#ifdef __cplusplus
}
#endif


#if defined(PA_OSX_INTERPOSE) && defined(PA_SHARED_LIB_EXPORT)

// ------------------------------------------------------
// Override malloc_xxx and malloc_zone_xxx api's to use only
// our palloc zone. Since even the loader uses malloc
// on macOS, this ensures that all allocations go through
// palloc (as all calls are interposed).
// The main `malloc`, `free`, etc calls are interposed in `alloc-override.c`,
// Here, we also override macOS specific API's like
// `malloc_zone_calloc` etc. see <https://github.com/aosm/libmalloc/blob/master/man/malloc_zone_malloc.3>
// ------------------------------------------------------

static inline malloc_zone_t* pa_get_default_zone(void)
{
  static bool init;
  if pa_unlikely(!init) {
    init = true;
    malloc_zone_register(&pa_malloc_zone);  // by calling register we avoid a zone error on free (see <http://eatmyrandom.blogspot.com/2010/03/mallocfree-interception-on-mac-os-x.html>)
  }
  return &pa_malloc_zone;
}

pa_decl_externc int  malloc_jumpstart(uintptr_t cookie);
pa_decl_externc void _malloc_fork_prepare(void);
pa_decl_externc void _malloc_fork_parent(void);
pa_decl_externc void _malloc_fork_child(void);


static malloc_zone_t* pa_malloc_create_zone(vm_size_t size, unsigned flags) {
  PA_UNUSED(size); PA_UNUSED(flags);
  return pa_get_default_zone();
}

static malloc_zone_t* pa_malloc_default_zone (void) {
  return pa_get_default_zone();
}

static malloc_zone_t* pa_malloc_default_purgeable_zone(void) {
  return pa_get_default_zone();
}

static void pa_malloc_destroy_zone(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  // nothing.
}

static kern_return_t pa_malloc_get_all_zones (task_t task, memory_reader_t mr, vm_address_t** addresses, unsigned* count) {
  PA_UNUSED(task); PA_UNUSED(mr);
  if (addresses != NULL) *addresses = NULL;
  if (count != NULL) *count = 0;
  return KERN_SUCCESS;
}

static const char* pa_malloc_get_zone_name(malloc_zone_t* zone) {
  return (zone == NULL ? pa_malloc_zone.zone_name : zone->zone_name);
}

static void pa_malloc_set_zone_name(malloc_zone_t* zone, const char* name) {
  PA_UNUSED(zone); PA_UNUSED(name);
}

static int pa_malloc_jumpstart(uintptr_t cookie) {
  PA_UNUSED(cookie);
  return 1; // or 0 for no error?
}

static void pa__malloc_fork_prepare(void) {
  // nothing
}
static void pa__malloc_fork_parent(void) {
  // nothing
}
static void pa__malloc_fork_child(void) {
  // nothing
}

static void pa_malloc_printf(const char* fmt, ...) {
  PA_UNUSED(fmt);
}

static bool zone_check(malloc_zone_t* zone) {
  PA_UNUSED(zone);
  return true;
}

static malloc_zone_t* zone_from_ptr(const void* p) {
  PA_UNUSED(p);
  return pa_get_default_zone();
}

static void zone_log(malloc_zone_t* zone, void* p) {
  PA_UNUSED(zone); PA_UNUSED(p);
}

static void zone_print(malloc_zone_t* zone, bool b) {
  PA_UNUSED(zone); PA_UNUSED(b);
}

static void zone_print_ptr_info(void* p) {
  PA_UNUSED(p);
}

static void zone_register(malloc_zone_t* zone) {
  PA_UNUSED(zone);
}

static void zone_unregister(malloc_zone_t* zone) {
  PA_UNUSED(zone);
}

// use interposing so `DYLD_INSERT_LIBRARIES` works without `DYLD_FORCE_FLAT_NAMESPACE=1`
// See: <https://books.google.com/books?id=K8vUkpOXhN4C&pg=PA73>
struct pa_interpose_s {
  const void* replacement;
  const void* target;
};
#define PA_INTERPOSE_FUN(oldfun,newfun) { (const void*)&newfun, (const void*)&oldfun }
#define PA_INTERPOSE_MI(fun)            PA_INTERPOSE_FUN(fun,pa_##fun)
#define PA_INTERPOSE_ZONE(fun)          PA_INTERPOSE_FUN(malloc_##fun,fun)
__attribute__((used)) static const struct pa_interpose_s _pa_zone_interposes[]  __attribute__((section("__DATA, __interpose"))) =
{

  PA_INTERPOSE_MI(malloc_create_zone),
  PA_INTERPOSE_MI(malloc_default_purgeable_zone),
  PA_INTERPOSE_MI(malloc_default_zone),
  PA_INTERPOSE_MI(malloc_destroy_zone),
  PA_INTERPOSE_MI(malloc_get_all_zones),
  PA_INTERPOSE_MI(malloc_get_zone_name),
  PA_INTERPOSE_MI(malloc_jumpstart),
  PA_INTERPOSE_MI(malloc_printf),
  PA_INTERPOSE_MI(malloc_set_zone_name),
  PA_INTERPOSE_MI(_malloc_fork_child),
  PA_INTERPOSE_MI(_malloc_fork_parent),
  PA_INTERPOSE_MI(_malloc_fork_prepare),

  PA_INTERPOSE_ZONE(zone_batch_free),
  PA_INTERPOSE_ZONE(zone_batch_malloc),
  PA_INTERPOSE_ZONE(zone_calloc),
  PA_INTERPOSE_ZONE(zone_check),
  PA_INTERPOSE_ZONE(zone_free),
  PA_INTERPOSE_ZONE(zone_from_ptr),
  PA_INTERPOSE_ZONE(zone_log),
  PA_INTERPOSE_ZONE(zone_malloc),
  PA_INTERPOSE_ZONE(zone_memalign),
  PA_INTERPOSE_ZONE(zone_print),
  PA_INTERPOSE_ZONE(zone_print_ptr_info),
  PA_INTERPOSE_ZONE(zone_realloc),
  PA_INTERPOSE_ZONE(zone_register),
  PA_INTERPOSE_ZONE(zone_unregister),
  PA_INTERPOSE_ZONE(zone_valloc)
};


#else

// ------------------------------------------------------
// hook into the zone api's without interposing
// This is the official way of adding an allocator but
// it seems less robust than using interpose.
// ------------------------------------------------------

static inline malloc_zone_t* pa_get_default_zone(void)
{
  // The first returned zone is the real default
  malloc_zone_t** zones = NULL;
  unsigned count = 0;
  kern_return_t ret = malloc_get_all_zones(0, NULL, (vm_address_t**)&zones, &count);
  if (ret == KERN_SUCCESS && count > 0) {
    return zones[0];
  }
  else {
    // fallback
    return malloc_default_zone();
  }
}

#if defined(__clang__)
__attribute__((constructor(101))) // highest priority
#else
__attribute__((constructor))      // priority level is not supported by gcc
#endif
__attribute__((used))
static void _pa_macos_override_malloc(void) {
  malloc_zone_t* purgeable_zone = NULL;

  #if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  // force the purgeable zone to exist to avoid strange bugs
  if (malloc_default_purgeable_zone) {
    purgeable_zone = malloc_default_purgeable_zone();
  }
  #endif

  // Register our zone.
  // thomcc: I think this is still needed to put us in the zone list.
  malloc_zone_register(&pa_malloc_zone);
  // Unregister the default zone, this makes our zone the new default
  // as that was the last registered.
  malloc_zone_t *default_zone = pa_get_default_zone();
  // thomcc: Unsure if the next test is *always* false or just false in the
  // cases I've tried. I'm also unsure if the code inside is needed. at all
  if (default_zone != &pa_malloc_zone) {
    malloc_zone_unregister(default_zone);

    // Reregister the default zone so free and realloc in that zone keep working.
    malloc_zone_register(default_zone);
  }

  // Unregister, and re-register the purgeable_zone to avoid bugs if it occurs
  // earlier than the default zone.
  if (purgeable_zone != NULL) {
    malloc_zone_unregister(purgeable_zone);
    malloc_zone_register(purgeable_zone);
  }

}
#endif  // PA_OSX_INTERPOSE

#endif // PA_MALLOC_OVERRIDE
