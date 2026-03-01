/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"
#include "palloc/prim.h"  // pa_prim_out_stderr

#include <stdio.h>      // stdin/stdout
#include <stdlib.h>     // abort



static long pa_max_error_count   = 16; // stop outputting errors after this (use < 0 for no limit)
static long pa_max_warning_count = 16; // stop outputting warnings after this (use < 0 for no limit)

static void pa_add_stderr_output(void);

int pa_version(void) pa_attr_noexcept {
  return PA_MALLOC_VERSION;
}


// --------------------------------------------------------
// Options
// These can be accessed by multiple threads and may be
// concurrently initialized, but an initializing data race
// is ok since they resolve to the same value.
// --------------------------------------------------------
typedef enum pa_init_e {
  UNINIT,       // not yet initialized
  DEFAULTED,    // not found in the environment, use default value
  INITIALIZED   // found in environment or set explicitly
} pa_init_t;

typedef struct pa_option_desc_s {
  long        value;  // the value
  pa_init_t   init;   // is it initialized yet? (from the environment)
  pa_option_t option; // for debugging: the option index should match the option
  const char* name;   // option name without `palloc_` prefix
  const char* legacy_name; // potential legacy option name
} pa_option_desc_t;

#define PA_OPTION(opt)                  pa_option_##opt, #opt, NULL
#define PA_OPTION_LEGACY(opt,legacy)    pa_option_##opt, #opt, #legacy

// Some options can be set at build time for statically linked libraries
// (use `-DPA_EXTRA_CPPDEFS="opt1=val1;opt2=val2"`)
//
// This is useful if we cannot pass them as environment variables
// (and setting them programmatically would be too late)

#ifndef PA_DEFAULT_VERBOSE
#define PA_DEFAULT_VERBOSE 0
#endif

#ifndef PA_DEFAULT_EAGER_COMMIT
#define PA_DEFAULT_EAGER_COMMIT 1
#endif

#ifndef PA_DEFAULT_ARENA_EAGER_COMMIT
#define PA_DEFAULT_ARENA_EAGER_COMMIT 2
#endif

// in KiB
#ifndef PA_DEFAULT_ARENA_RESERVE
 #if (PA_INTPTR_SIZE>4)
  #define PA_DEFAULT_ARENA_RESERVE 1024L*1024L
 #else
  #define PA_DEFAULT_ARENA_RESERVE 128L*1024L
 #endif
#endif

#ifndef PA_DEFAULT_DISALLOW_ARENA_ALLOC
#define PA_DEFAULT_DISALLOW_ARENA_ALLOC 0
#endif

#ifndef PA_DEFAULT_ALLOW_LARGE_OS_PAGES
#define PA_DEFAULT_ALLOW_LARGE_OS_PAGES 0
#endif

#ifndef PA_DEFAULT_RESERVE_HUGE_OS_PAGES
#define PA_DEFAULT_RESERVE_HUGE_OS_PAGES 0
#endif

#ifndef PA_DEFAULT_RESERVE_OS_MEMORY
#define PA_DEFAULT_RESERVE_OS_MEMORY 0
#endif

#ifndef PA_DEFAULT_GUARDED_SAMPLE_RATE
#if PA_GUARDED
#define PA_DEFAULT_GUARDED_SAMPLE_RATE 4000
#else
#define PA_DEFAULT_GUARDED_SAMPLE_RATE 0
#endif
#endif


#ifndef PA_DEFAULT_ALLOW_THP
#if defined(__ANDROID__)
#define PA_DEFAULT_ALLOW_THP  0
#else
#define PA_DEFAULT_ALLOW_THP  1
#endif
#endif

// Static options
static pa_option_desc_t options[_pa_option_last] =
{
  // stable options
  #if PA_DEBUG || defined(PA_SHOW_ERRORS)
  { 1, UNINIT, PA_OPTION(show_errors) },
  #else
  { 0, UNINIT, PA_OPTION(show_errors) },
  #endif
  { 0, UNINIT, PA_OPTION(show_stats) },
  { PA_DEFAULT_VERBOSE, UNINIT, PA_OPTION(verbose) },

  // some of the following options are experimental and not all combinations are allowed.
  { PA_DEFAULT_EAGER_COMMIT,
       UNINIT, PA_OPTION(eager_commit) },               // commit per segment directly (4MiB)  (but see also `eager_commit_delay`)
  { PA_DEFAULT_ARENA_EAGER_COMMIT,
       UNINIT, PA_OPTION_LEGACY(arena_eager_commit,eager_region_commit) }, // eager commit arena's? 2 is used to enable this only on an OS that has overcommit (i.e. linux)
  { 1, UNINIT, PA_OPTION_LEGACY(purge_decommits,reset_decommits) },        // purge decommits memory (instead of reset) (note: on linux this uses MADV_DONTNEED for decommit)
  { PA_DEFAULT_ALLOW_LARGE_OS_PAGES,
       UNINIT, PA_OPTION_LEGACY(allow_large_os_pages,large_os_pages) },    // use large OS pages, use only with eager commit to prevent fragmentation of VMA's
  { PA_DEFAULT_RESERVE_HUGE_OS_PAGES,
       UNINIT, PA_OPTION(reserve_huge_os_pages) },      // per 1GiB huge pages
  {-1, UNINIT, PA_OPTION(reserve_huge_os_pages_at) },   // reserve huge pages at node N
  { PA_DEFAULT_RESERVE_OS_MEMORY,
       UNINIT, PA_OPTION(reserve_os_memory)     },      // reserve N KiB OS memory in advance (use `option_get_size`)
  { 0, UNINIT, PA_OPTION(deprecated_segment_cache) },   // cache N segments per thread
  { 0, UNINIT, PA_OPTION(deprecated_page_reset) },      // reset page memory on free
  { 0, UNINIT, PA_OPTION_LEGACY(abandoned_page_purge,abandoned_page_reset) },       // reset free page memory when a thread terminates
  { 0, UNINIT, PA_OPTION(deprecated_segment_reset) },   // reset segment memory on free (needs eager commit)
#if defined(__NetBSD__)
  { 0, UNINIT, PA_OPTION(eager_commit_delay) },         // the first N segments per thread are not eagerly committed
#else
  { 1, UNINIT, PA_OPTION(eager_commit_delay) },         // the first N segments per thread are not eagerly committed (but per page in the segment on demand)
#endif
  { 10,  UNINIT, PA_OPTION_LEGACY(purge_delay,reset_delay) },  // purge delay in milli-seconds
  { 0,   UNINIT, PA_OPTION(use_numa_nodes) },           // 0 = use available numa nodes, otherwise use at most N nodes.
  { 0,   UNINIT, PA_OPTION_LEGACY(disallow_os_alloc,limit_os_alloc) },           // 1 = do not use OS memory for allocation (but only reserved arenas)
  { 100, UNINIT, PA_OPTION(os_tag) },                   // only apple specific for now but might serve more or less related purpose
  { 32,  UNINIT, PA_OPTION(max_errors) },               // maximum errors that are output
  { 32,  UNINIT, PA_OPTION(max_warnings) },             // maximum warnings that are output
  { 10,  UNINIT, PA_OPTION(max_segment_reclaim)},       // max. percentage of the abandoned segments to be reclaimed per try.
  { 0,   UNINIT, PA_OPTION(destroy_on_exit)},           // release all OS memory on process exit; careful with dangling pointer or after-exit frees!
  { PA_DEFAULT_ARENA_RESERVE, UNINIT, PA_OPTION(arena_reserve) }, // reserve memory N KiB at a time (=1GiB) (use `option_get_size`)
  { 10,  UNINIT, PA_OPTION(arena_purge_mult) },         // purge delay multiplier for arena's
  { 1,   UNINIT, PA_OPTION_LEGACY(purge_extend_delay, decommit_extend_delay) },
  { 0,   UNINIT, PA_OPTION(abandoned_reclaim_on_free) },// reclaim an abandoned segment on a free
  { PA_DEFAULT_DISALLOW_ARENA_ALLOC,   UNINIT, PA_OPTION(disallow_arena_alloc) }, // 1 = do not use arena's for allocation (except if using specific arena id's)
  { 400, UNINIT, PA_OPTION(retry_on_oom) },             // windows only: retry on out-of-memory for N milli seconds (=400), set to 0 to disable retries.
#if defined(PA_VISIT_ABANDONED)
  { 1,   INITIALIZED, PA_OPTION(visit_abandoned) },     // allow visiting heap blocks in abandoned segments; requires taking locks during reclaim.
#else
  { 0,   UNINIT, PA_OPTION(visit_abandoned) },
#endif
  { 0,   UNINIT, PA_OPTION(guarded_min) },              // only used when building with PA_GUARDED: minimal rounded object size for guarded objects
  { PA_GiB, UNINIT, PA_OPTION(guarded_max) },           // only used when building with PA_GUARDED: maximal rounded object size for guarded objects
  { 0,   UNINIT, PA_OPTION(guarded_precise) },          // disregard minimal alignment requirement to always place guarded blocks exactly in front of a guard page (=0)
  { PA_DEFAULT_GUARDED_SAMPLE_RATE,
         UNINIT, PA_OPTION(guarded_sample_rate)},       // 1 out of N allocations in the min/max range will be guarded (=4000)
  { 0,   UNINIT, PA_OPTION(guarded_sample_seed)},
  { 0,   UNINIT, PA_OPTION(target_segments_per_thread) }, // abandon segments beyond this point, or 0 to disable.
  { 10000, UNINIT, PA_OPTION(generic_collect) },          // collect heaps every N (=10000) generic allocation calls
  { PA_DEFAULT_ALLOW_THP, 
         UNINIT, PA_OPTION(allow_thp) },                 // allow transparent huge pages?
  { 0,   UNINIT, PA_OPTION(adaptive_purge) }
};

static void pa_option_init(pa_option_desc_t* desc);

static bool pa_option_has_size_in_kib(pa_option_t option) {
  return (option == pa_option_reserve_os_memory || option == pa_option_arena_reserve);
}

void _pa_options_init(void) {
  // called on process load
  pa_add_stderr_output(); // now it safe to use stderr for output
  for(int i = 0; i < _pa_option_last; i++ ) {
    pa_option_t option = (pa_option_t)i;
    long l = pa_option_get(option); PA_UNUSED(l); // initialize
  }
  pa_max_error_count = pa_option_get(pa_option_max_errors);
  pa_max_warning_count = pa_option_get(pa_option_max_warnings);
  #if PA_GUARDED
  if (pa_option_get(pa_option_guarded_sample_rate) > 0) {
    if (pa_option_is_enabled(pa_option_allow_large_os_pages)) {
      pa_option_disable(pa_option_allow_large_os_pages);
      _pa_warning_message("option 'allow_large_os_pages' is disabled to allow for guarded objects\n");
    }
  }
  #endif
  if (pa_option_is_enabled(pa_option_verbose)) { pa_options_print(); }
}

#define pa_stringifyx(str)  #str                // and stringify
#define pa_stringify(str)   pa_stringifyx(str)  // expand

void pa_options_print(void) pa_attr_noexcept
{
  // show version
  const int vermajor = PA_MALLOC_VERSION/100;
  const int verminor = (PA_MALLOC_VERSION%100)/10;
  const int verpatch = (PA_MALLOC_VERSION%10);
  _pa_message("v%i.%i.%i%s%s (built on %s, %s)\n", vermajor, verminor, verpatch,
      #if defined(PA_CMAKE_BUILD_TYPE)
      ", " pa_stringify(PA_CMAKE_BUILD_TYPE)
      #else
      ""
      #endif
      ,
      #if defined(PA_GIT_DESCRIBE)
      ", git " pa_stringify(PA_GIT_DESCRIBE)
      #else
      ""
      #endif
      , __DATE__, __TIME__);

  // show options
  for (int i = 0; i < _pa_option_last; i++) {
    pa_option_t option = (pa_option_t)i;
    long l = pa_option_get(option); PA_UNUSED(l); // possibly initialize
    pa_option_desc_t* desc = &options[option];
    _pa_message("option '%s': %ld %s\n", desc->name, desc->value, (pa_option_has_size_in_kib(option) ? "KiB" : ""));
  }

  // show build configuration
  _pa_message("debug level : %d\n", PA_DEBUG );
  _pa_message("secure level: %d\n", PA_SECURE );
  _pa_message("mem tracking: %s\n", PA_TRACK_TOOL);
  #if PA_GUARDED
  _pa_message("guarded build: %s\n", pa_option_get(pa_option_guarded_sample_rate) != 0 ? "enabled" : "disabled");
  #endif
  #if PA_TSAN
  _pa_message("thread santizer enabled\n");
  #endif
}

long _pa_option_get_fast(pa_option_t option) {
  pa_assert(option >= 0 && option < _pa_option_last);
  pa_option_desc_t* desc = &options[option];
  pa_assert(desc->option == option);  // index should match the option
  //pa_assert(desc->init != UNINIT);
  return desc->value;
}


pa_decl_nodiscard long pa_option_get(pa_option_t option) {
  pa_assert(option >= 0 && option < _pa_option_last);
  if (option < 0 || option >= _pa_option_last) return 0;
  pa_option_desc_t* desc = &options[option];
  pa_assert(desc->option == option);  // index should match the option
  if pa_unlikely(desc->init == UNINIT) {
    pa_option_init(desc);
  }
  return desc->value;
}

pa_decl_nodiscard long pa_option_get_clamp(pa_option_t option, long min, long max) {
  long x = pa_option_get(option);
  return (x < min ? min : (x > max ? max : x));
}

pa_decl_nodiscard size_t pa_option_get_size(pa_option_t option) {
  const long x = pa_option_get(option);
  size_t size = (x < 0 ? 0 : (size_t)x);
  if (pa_option_has_size_in_kib(option)) {
    size *= PA_KiB;
  }
  return size;
}

void pa_option_set(pa_option_t option, long value) {
  pa_assert(option >= 0 && option < _pa_option_last);
  if (option < 0 || option >= _pa_option_last) return;
  pa_option_desc_t* desc = &options[option];
  pa_assert(desc->option == option);  // index should match the option
  desc->value = value;
  desc->init = INITIALIZED;
  // ensure min/max range; be careful to not recurse.
  if (desc->option == pa_option_guarded_min && _pa_option_get_fast(pa_option_guarded_max) < value) {
    pa_option_set(pa_option_guarded_max, value);
  }
  else if (desc->option == pa_option_guarded_max && _pa_option_get_fast(pa_option_guarded_min) > value) {
    pa_option_set(pa_option_guarded_min, value);
  }
}

void pa_option_set_default(pa_option_t option, long value) {
  pa_assert(option >= 0 && option < _pa_option_last);
  if (option < 0 || option >= _pa_option_last) return;
  pa_option_desc_t* desc = &options[option];
  if (desc->init != INITIALIZED) {
    desc->value = value;
  }
}

pa_decl_nodiscard bool pa_option_is_enabled(pa_option_t option) {
  return (pa_option_get(option) != 0);
}

void pa_option_set_enabled(pa_option_t option, bool enable) {
  pa_option_set(option, (enable ? 1 : 0));
}

void pa_option_set_enabled_default(pa_option_t option, bool enable) {
  pa_option_set_default(option, (enable ? 1 : 0));
}

void pa_option_enable(pa_option_t option) {
  pa_option_set_enabled(option,true);
}

void pa_option_disable(pa_option_t option) {
  pa_option_set_enabled(option,false);
}

static void pa_cdecl pa_out_stderr(const char* msg, void* arg) {
  PA_UNUSED(arg);
  if (msg != NULL && msg[0] != 0) {
    _pa_prim_out_stderr(msg);
  }
}

// Since an output function can be registered earliest in the `main`
// function we also buffer output that happens earlier. When
// an output function is registered it is called immediately with
// the output up to that point.
#ifndef PA_MAX_DELAY_OUTPUT
#define PA_MAX_DELAY_OUTPUT ((size_t)(16*1024))
#endif
static char out_buf[PA_MAX_DELAY_OUTPUT+1];
static _Atomic(size_t) out_len;

static void pa_cdecl pa_out_buf(const char* msg, void* arg) {
  PA_UNUSED(arg);
  if (msg==NULL) return;
  if (pa_atomic_load_relaxed(&out_len)>=PA_MAX_DELAY_OUTPUT) return;
  size_t n = _pa_strlen(msg);
  if (n==0) return;
  // claim space
  size_t start = pa_atomic_add_acq_rel(&out_len, n);
  if (start >= PA_MAX_DELAY_OUTPUT) return;
  // check bound
  if (start+n >= PA_MAX_DELAY_OUTPUT) {
    n = PA_MAX_DELAY_OUTPUT-start-1;
  }
  _pa_memcpy(&out_buf[start], msg, n);
}

static void pa_out_buf_flush(pa_output_fun* out, bool no_more_buf, void* arg) {
  if (out==NULL) return;
  // claim (if `no_more_buf == true`, no more output will be added after this point)
  size_t count = pa_atomic_add_acq_rel(&out_len, (no_more_buf ? PA_MAX_DELAY_OUTPUT : 1));
  // and output the current contents
  if (count>PA_MAX_DELAY_OUTPUT) count = PA_MAX_DELAY_OUTPUT;
  out_buf[count] = 0;
  out(out_buf,arg);
  if (!no_more_buf) {
    out_buf[count] = '\n'; // if continue with the buffer, insert a newline
  }
}


// Once this module is loaded, switch to this routine
// which outputs to stderr and the delayed output buffer.
static void pa_cdecl pa_out_buf_stderr(const char* msg, void* arg) {
  pa_out_stderr(msg,arg);
  pa_out_buf(msg,arg);
}



// --------------------------------------------------------
// Default output handler
// --------------------------------------------------------

// Should be atomic but gives errors on many platforms as generally we cannot cast a function pointer to a uintptr_t.
// For now, don't register output from multiple threads.
static pa_output_fun* volatile pa_out_default; // = NULL
static _Atomic(void*) pa_out_arg; // = NULL

static pa_output_fun* pa_out_get_default(void** parg) {
  if (parg != NULL) { *parg = pa_atomic_load_ptr_acquire(void,&pa_out_arg); }
  pa_output_fun* out = pa_out_default;
  return (out == NULL ? &pa_out_buf : out);
}

void pa_register_output(pa_output_fun* out, void* arg) pa_attr_noexcept {
  pa_out_default = (out == NULL ? &pa_out_stderr : out); // stop using the delayed output buffer
  pa_atomic_store_ptr_release(void,&pa_out_arg, arg);
  if (out!=NULL) pa_out_buf_flush(out,true,arg);         // output all the delayed output now
}

// add stderr to the delayed output after the module is loaded
static void pa_add_stderr_output(void) {
  pa_assert_internal(pa_out_default == NULL);
  pa_out_buf_flush(&pa_out_stderr, false, NULL); // flush current contents to stderr
  pa_out_default = &pa_out_buf_stderr;           // and add stderr to the delayed output
}

// --------------------------------------------------------
// Messages, all end up calling `_pa_fputs`.
// --------------------------------------------------------
static _Atomic(size_t) error_count;   // = 0;  // when >= max_error_count stop emitting errors
static _Atomic(size_t) warning_count; // = 0;  // when >= max_warning_count stop emitting warnings

// When overriding malloc, we may recurse into pa_vfprintf if an allocation
// inside the C runtime causes another message.
// In some cases (like on macOS) the loader already allocates which
// calls into palloc; if we then access thread locals (like `recurse`)
// this may crash as the access may call _tlv_bootstrap that tries to
// (recursively) invoke malloc again to allocate space for the thread local
// variables on demand. This is why we use a _pa_preloading test on such
// platforms. However, C code generator may move the initial thread local address
// load before the `if` and we therefore split it out in a separate function.
static pa_decl_thread bool recurse = false;

static pa_decl_noinline bool pa_recurse_enter_prim(void) {
  if (recurse) return false;
  recurse = true;
  return true;
}

static pa_decl_noinline void pa_recurse_exit_prim(void) {
  recurse = false;
}

static bool pa_recurse_enter(void) {
  #if defined(__APPLE__) || defined(__ANDROID__) || defined(PA_TLS_RECURSE_GUARD)
  if (_pa_preloading()) return false;
  #endif
  return pa_recurse_enter_prim();
}

static void pa_recurse_exit(void) {
  #if defined(__APPLE__) || defined(__ANDROID__) || defined(PA_TLS_RECURSE_GUARD)
  if (_pa_preloading()) return;
  #endif
  pa_recurse_exit_prim();
}

void _pa_fputs(pa_output_fun* out, void* arg, const char* prefix, const char* message) {
  if (out==NULL || (void*)out==(void*)stdout || (void*)out==(void*)stderr) { // TODO: use pa_out_stderr for stderr?
    if (!pa_recurse_enter()) return;
    out = pa_out_get_default(&arg);
    if (prefix != NULL) out(prefix, arg);
    out(message, arg);
    pa_recurse_exit();
  }
  else {
    if (prefix != NULL) out(prefix, arg);
    out(message, arg);
  }
}

// Define our own limited `fprintf` that avoids memory allocation.
// We do this using `_pa_vsnprintf` with a limited buffer.
static void pa_vfprintf( pa_output_fun* out, void* arg, const char* prefix, const char* fmt, va_list args ) {
  char buf[512];
  if (fmt==NULL) return;
  if (!pa_recurse_enter()) return;
  _pa_vsnprintf(buf, sizeof(buf)-1, fmt, args);
  pa_recurse_exit();
  _pa_fputs(out,arg,prefix,buf);
}

void _pa_fprintf( pa_output_fun* out, void* arg, const char* fmt, ... ) {
  va_list args;
  va_start(args,fmt);
  pa_vfprintf(out,arg,NULL,fmt,args);
  va_end(args);
}

static void pa_vfprintf_thread(pa_output_fun* out, void* arg, const char* prefix, const char* fmt, va_list args) {
  if (prefix != NULL && _pa_strnlen(prefix,33) <= 32 && !_pa_is_main_thread()) {
    char tprefix[64];
    _pa_snprintf(tprefix, sizeof(tprefix), "%sthread 0x%tx: ", prefix, (uintptr_t)_pa_thread_id());
    pa_vfprintf(out, arg, tprefix, fmt, args);
  }
  else {
    pa_vfprintf(out, arg, prefix, fmt, args);
  }
}

void _pa_message(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  pa_vfprintf_thread(NULL, NULL, "palloc: ", fmt, args);
  va_end(args);
}

void _pa_trace_message(const char* fmt, ...) {
  if (pa_option_get(pa_option_verbose) <= 1) return;  // only with verbose level 2 or higher
  va_list args;
  va_start(args, fmt);
  pa_vfprintf_thread(NULL, NULL, "palloc: ", fmt, args);
  va_end(args);
}

void _pa_verbose_message(const char* fmt, ...) {
  if (!pa_option_is_enabled(pa_option_verbose)) return;
  va_list args;
  va_start(args,fmt);
  pa_vfprintf(NULL, NULL, "palloc: ", fmt, args);
  va_end(args);
}

static void pa_show_error_message(const char* fmt, va_list args) {
  if (!pa_option_is_enabled(pa_option_verbose)) {
    if (!pa_option_is_enabled(pa_option_show_errors)) return;
    if (pa_max_error_count >= 0 && (long)pa_atomic_increment_acq_rel(&error_count) > pa_max_error_count) return;
  }
  pa_vfprintf_thread(NULL, NULL, "palloc: error: ", fmt, args);
}

void _pa_warning_message(const char* fmt, ...) {
  if (!pa_option_is_enabled(pa_option_verbose)) {
    if (!pa_option_is_enabled(pa_option_show_errors)) return;
    if (pa_max_warning_count >= 0 && (long)pa_atomic_increment_acq_rel(&warning_count) > pa_max_warning_count) return;
  }
  va_list args;
  va_start(args,fmt);
  pa_vfprintf_thread(NULL, NULL, "palloc: warning: ", fmt, args);
  va_end(args);
}


#if PA_DEBUG
pa_decl_noreturn pa_decl_cold void _pa_assert_fail(const char* assertion, const char* fname, unsigned line, const char* func ) pa_attr_noexcept {
  _pa_fprintf(NULL, NULL, "palloc: assertion failed: at \"%s\":%u, %s\n  assertion: \"%s\"\n", fname, line, (func==NULL?"":func), assertion);
  abort();
}
#endif

// --------------------------------------------------------
// Errors
// --------------------------------------------------------

static pa_error_fun* volatile  pa_error_handler; // = NULL
static _Atomic(void*) pa_error_arg;     // = NULL

static void pa_error_default(int err) {
  PA_UNUSED(err);
#if (PA_DEBUG>0)
  if (err==EFAULT) {
    #ifdef _MSC_VER
    __debugbreak();
    #endif
    abort();
  }
#endif
#if (PA_SECURE>0)
  if (err==EFAULT) {  // abort on serious errors in secure mode (corrupted meta-data)
    abort();
  }
#endif
#if defined(PA_XMALLOC)
  if (err==ENOMEM || err==EOVERFLOW) { // abort on memory allocation fails in xmalloc mode
    abort();
  }
#endif
}

void pa_register_error(pa_error_fun* fun, void* arg) {
  pa_error_handler = fun;  // can be NULL
  pa_atomic_store_ptr_release(void,&pa_error_arg, arg);
}

void _pa_error_message(int err, const char* fmt, ...) {
  // show detailed error message
  va_list args;
  va_start(args, fmt);
  pa_show_error_message(fmt, args);
  va_end(args);
  // and call the error handler which may abort (or return normally)
  if (pa_error_handler != NULL) {
    pa_error_handler(err, pa_atomic_load_ptr_acquire(void,&pa_error_arg));
  }
  else {
    pa_error_default(err);
  }
}

// --------------------------------------------------------
// Initialize options by checking the environment
// --------------------------------------------------------

// TODO: implement ourselves to reduce dependencies on the C runtime
#include <stdlib.h> // strtol
#include <string.h> // strstr


static void pa_option_init(pa_option_desc_t* desc) {
  // Read option value from the environment
  char s[64 + 1];
  char buf[64+1];
  _pa_strlcpy(buf, "palloc_", sizeof(buf));
  _pa_strlcat(buf, desc->name, sizeof(buf));
  bool found = _pa_getenv(buf, s, sizeof(s));
  if (!found && desc->legacy_name != NULL) {
    _pa_strlcpy(buf, "palloc_", sizeof(buf));
    _pa_strlcat(buf, desc->legacy_name, sizeof(buf));
    found = _pa_getenv(buf, s, sizeof(s));
    if (found) {
      _pa_warning_message("environment option \"palloc_%s\" is deprecated -- use \"palloc_%s\" instead.\n", desc->legacy_name, desc->name);
    }
  }

  if (found) {
    size_t len = _pa_strnlen(s, sizeof(buf) - 1);
    for (size_t i = 0; i < len; i++) {
      buf[i] = _pa_toupper(s[i]);
    }
    buf[len] = 0;
    if (buf[0] == 0 || strstr("1;TRUE;YES;ON", buf) != NULL) {
      desc->value = 1;
      desc->init = INITIALIZED;
    }
    else if (strstr("0;FALSE;NO;OFF", buf) != NULL) {
      desc->value = 0;
      desc->init = INITIALIZED;
    }
    else {
      char* end = buf;
      long value = strtol(buf, &end, 10);
      if (pa_option_has_size_in_kib(desc->option)) {
        // this option is interpreted in KiB to prevent overflow of `long` for large allocations
        // (long is 32-bit on 64-bit windows, which allows for 4TiB max.)
        size_t size = (value < 0 ? 0 : (size_t)value);
        bool overflow = false;
        if (*end == 'K') { end++; }
        else if (*end == 'M') { overflow = pa_mul_overflow(size,PA_KiB,&size); end++; }
        else if (*end == 'G') { overflow = pa_mul_overflow(size,PA_MiB,&size); end++; }
        else if (*end == 'T') { overflow = pa_mul_overflow(size,PA_GiB,&size); end++; }
        else { size = (size + PA_KiB - 1) / PA_KiB; }
        if (end[0] == 'I' && end[1] == 'B') { end += 2; } // KiB, MiB, GiB, TiB
        else if (*end == 'B') { end++; }                  // Kb, Mb, Gb, Tb
        if (overflow || size > PA_MAX_ALLOC_SIZE) { size = (PA_MAX_ALLOC_SIZE / PA_KiB); }
        value = (size > LONG_MAX ? LONG_MAX : (long)size);
      }
      if (*end == 0) {
        pa_option_set(desc->option, value);
      }
      else {
        // set `init` first to avoid recursion through _pa_warning_message on palloc_verbose.
        desc->init = DEFAULTED;
        if (desc->option == pa_option_verbose && desc->value == 0) {
          // if the 'palloc_verbose' env var has a bogus value we'd never know
          // (since the value defaults to 'off') so in that case briefly enable verbose
          desc->value = 1;
          _pa_warning_message("environment option palloc_%s has an invalid value.\n", desc->name);
          desc->value = 0;
        }
        else {
          _pa_warning_message("environment option palloc_%s has an invalid value.\n", desc->name);
        }
      }
    }
    pa_assert_internal(desc->init != UNINIT);
  }
  else if (!_pa_preloading()) {
    desc->init = DEFAULTED;
  }
}
