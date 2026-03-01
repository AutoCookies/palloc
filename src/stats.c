/* ----------------------------------------------------------------------------
Copyright (c) 2018-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "palloc.h"
#include "palloc/internal.h"
#include "palloc/atomic.h"
#include "palloc/prim.h"

#include <string.h> // memset

#if defined(_MSC_VER) && (_MSC_VER < 1920)
#pragma warning(disable:4204)  // non-constant aggregate initializer
#endif

/* -----------------------------------------------------------
  Statistics operations
----------------------------------------------------------- */

static bool pa_is_in_main(void* stat) {
  return ((uint8_t*)stat >= (uint8_t*)&_pa_stats_main
         && (uint8_t*)stat < ((uint8_t*)&_pa_stats_main + sizeof(pa_stats_t)));
}

static void pa_stat_update(pa_stat_count_t* stat, int64_t amount) {
  if (amount == 0) return;
  if pa_unlikely(pa_is_in_main(stat))
  {
    // add atomically (for abandoned pages)
    int64_t current = pa_atomic_addi64_relaxed(&stat->current, amount);
    // if (stat == &_pa_stats_main.committed) { pa_assert_internal(current + amount >= 0); };
    pa_atomic_maxi64_relaxed(&stat->peak, current + amount);
    if (amount > 0) {
      pa_atomic_addi64_relaxed(&stat->total,amount);
    }
  }
  else {
    // add thread local
    stat->current += amount;
    if (stat->current > stat->peak) { stat->peak = stat->current; }
    if (amount > 0) { stat->total += amount; }
  }
}

void _pa_stat_counter_increase(pa_stat_counter_t* stat, size_t amount) {
  if (pa_is_in_main(stat)) {
    pa_atomic_addi64_relaxed( &stat->total, (int64_t)amount );
  }
  else {
    stat->total += amount;
  }
}

void _pa_stat_increase(pa_stat_count_t* stat, size_t amount) {
  pa_stat_update(stat, (int64_t)amount);
}

void _pa_stat_decrease(pa_stat_count_t* stat, size_t amount) {
  pa_stat_update(stat, -((int64_t)amount));
}


static void pa_stat_adjust(pa_stat_count_t* stat, int64_t amount) {
  if (amount == 0) return;
  if pa_unlikely(pa_is_in_main(stat))
  {
    // adjust atomically 
    pa_atomic_addi64_relaxed(&stat->current, amount);
    pa_atomic_addi64_relaxed(&stat->total,amount);
  }
  else {
    // adjust local
    stat->current += amount;
    stat->total += amount;
  }
}

void _pa_stat_adjust_decrease(pa_stat_count_t* stat, size_t amount) {
  pa_stat_adjust(stat, -((int64_t)amount));
}


// must be thread safe as it is called from stats_merge
static void pa_stat_count_add_mt(pa_stat_count_t* stat, const pa_stat_count_t* src) {
  if (stat==src) return;
  pa_atomic_void_addi64_relaxed(&stat->total, &src->total); 
  const int64_t prev_current = pa_atomic_addi64_relaxed(&stat->current, src->current);

  // Global current plus thread peak approximates new global peak
  // note: peak scores do really not work across threads.
  // we used to just add them together but that often overestimates in practice.
  // similarly, max does not seem to work well. The current approach
  // by Artem Kharytoniuk (@artem-lunarg) seems to work better, see PR#1112 
  // for a longer description.
  pa_atomic_maxi64_relaxed(&stat->peak, prev_current + src->peak);
}

static void pa_stat_counter_add_mt(pa_stat_counter_t* stat, const pa_stat_counter_t* src) {
  if (stat==src) return;
  pa_atomic_void_addi64_relaxed(&stat->total, &src->total);
}

#define PA_STAT_COUNT(stat)    pa_stat_count_add_mt(&stats->stat, &src->stat);
#define PA_STAT_COUNTER(stat)  pa_stat_counter_add_mt(&stats->stat, &src->stat);

// must be thread safe as it is called from stats_merge
static void pa_stats_add(pa_stats_t* stats, const pa_stats_t* src) {
  if (stats==src) return;

  // copy all fields
  PA_STAT_FIELDS()

  #if PA_STAT>1
  for (size_t i = 0; i <= PA_BIN_HUGE; i++) {
    pa_stat_count_add_mt(&stats->malloc_bins[i], &src->malloc_bins[i]);
  }
  #endif
  for (size_t i = 0; i <= PA_BIN_HUGE; i++) {
    pa_stat_count_add_mt(&stats->page_bins[i], &src->page_bins[i]);
  }
}

#undef PA_STAT_COUNT
#undef PA_STAT_COUNTER

/* -----------------------------------------------------------
  Display statistics
----------------------------------------------------------- */

// unit > 0 : size in binary bytes
// unit == 0: count as decimal
// unit < 0 : count in binary
static void pa_printf_amount(int64_t n, int64_t unit, pa_output_fun* out, void* arg, const char* fmt) {
  char buf[32]; buf[0] = 0;
  int  len = 32;
  const char* suffix = (unit <= 0 ? " " : "B");
  const int64_t base = (unit == 0 ? 1000 : 1024);
  if (unit>0) n *= unit;

  const int64_t pos = (n < 0 ? -n : n);
  if (pos < base) {
    if (n!=1 || suffix[0] != 'B') {  // skip printing 1 B for the unit column
      _pa_snprintf(buf, len, "%lld   %-3s", (long long)n, (n==0 ? "" : suffix));
    }
  }
  else {
    int64_t divider = base;
    const char* magnitude = "K";
    if (pos >= divider*base) { divider *= base; magnitude = "M"; }
    if (pos >= divider*base) { divider *= base; magnitude = "G"; }
    const int64_t tens = (n / (divider/10));
    const long whole = (long)(tens/10);
    const long frac1 = (long)(tens%10);
    char unitdesc[8];
    _pa_snprintf(unitdesc, 8, "%s%s%s", magnitude, (base==1024 ? "i" : ""), suffix);
    _pa_snprintf(buf, len, "%ld.%ld %-3s", whole, (frac1 < 0 ? -frac1 : frac1), unitdesc);
  }
  _pa_fprintf(out, arg, (fmt==NULL ? "%12s" : fmt), buf);
}


static void pa_print_amount(int64_t n, int64_t unit, pa_output_fun* out, void* arg) {
  pa_printf_amount(n,unit,out,arg,NULL);
}

static void pa_print_count(int64_t n, int64_t unit, pa_output_fun* out, void* arg) {
  if (unit==1) _pa_fprintf(out, arg, "%12s"," ");
          else pa_print_amount(n,0,out,arg);
}

static void pa_stat_print_ex(const pa_stat_count_t* stat, const char* msg, int64_t unit, pa_output_fun* out, void* arg, const char* notok ) {
  _pa_fprintf(out, arg,"%10s:", msg);
  if (unit != 0) {
    if (unit > 0) {
      pa_print_amount(stat->peak, unit, out, arg);
      pa_print_amount(stat->total, unit, out, arg);
      // pa_print_amount(stat->freed, unit, out, arg);
      pa_print_amount(stat->current, unit, out, arg);
      pa_print_amount(unit, 1, out, arg);
      pa_print_count(stat->total, unit, out, arg);
    }
    else {
      pa_print_amount(stat->peak, -1, out, arg);
      pa_print_amount(stat->total, -1, out, arg);
      // pa_print_amount(stat->freed, -1, out, arg);
      pa_print_amount(stat->current, -1, out, arg);
      if (unit == -1) {
        _pa_fprintf(out, arg, "%24s", "");
      }
      else {
        pa_print_amount(-unit, 1, out, arg);
        pa_print_count((stat->total / -unit), 0, out, arg);
      }
    }
    if (stat->current != 0) {
      _pa_fprintf(out, arg, "  ");
      _pa_fprintf(out, arg, (notok == NULL ? "not all freed" : notok));
      _pa_fprintf(out, arg, "\n");
    }
    else {
      _pa_fprintf(out, arg, "  ok\n");
    }
  }
  else {
    pa_print_amount(stat->peak, 1, out, arg);
    pa_print_amount(stat->total, 1, out, arg);
    _pa_fprintf(out, arg, "%11s", " ");  // no freed
    pa_print_amount(stat->current, 1, out, arg);
    _pa_fprintf(out, arg, "\n");
  }
}

static void pa_stat_print(const pa_stat_count_t* stat, const char* msg, int64_t unit, pa_output_fun* out, void* arg) {
  pa_stat_print_ex(stat, msg, unit, out, arg, NULL);
}

#if PA_STAT>1
static void pa_stat_total_print(const pa_stat_count_t* stat, const char* msg, int64_t unit, pa_output_fun* out, void* arg) {
  _pa_fprintf(out, arg, "%10s:", msg);
  _pa_fprintf(out, arg, "%12s", " ");  // no peak
  pa_print_amount(stat->total, unit, out, arg);
  _pa_fprintf(out, arg, "\n");
}
#endif

static void pa_stat_counter_print(const pa_stat_counter_t* stat, const char* msg, pa_output_fun* out, void* arg ) {
  _pa_fprintf(out, arg, "%10s:", msg);
  pa_print_amount(stat->total, -1, out, arg);
  _pa_fprintf(out, arg, "\n");
}


static void pa_stat_average_print(size_t count, size_t total, const char* msg, pa_output_fun* out, void* arg) {
  const int64_t avg_tens = (count == 0 ? 0 : (total*10 / count));
  const long avg_whole = (long)(avg_tens/10);
  const long avg_frac1 = (long)(avg_tens%10);
  _pa_fprintf(out, arg, "%10s: %5ld.%ld avg\n", msg, avg_whole, avg_frac1);
}


static void pa_print_header(pa_output_fun* out, void* arg ) {
  _pa_fprintf(out, arg, "%10s: %11s %11s %11s %11s %11s\n", "heap stats", "peak   ", "total   ", "current   ", "block   ", "total#   ");
}

#if PA_STAT>1
static void pa_stats_print_bins(const pa_stat_count_t* bins, size_t max, const char* fmt, pa_output_fun* out, void* arg) {
  bool found = false;
  char buf[64];
  for (size_t i = 0; i <= max; i++) {
    if (bins[i].total > 0) {
      found = true;
      int64_t unit = _pa_bin_size((uint8_t)i);
      _pa_snprintf(buf, 64, "%s %3lu", fmt, (long)i);
      pa_stat_print(&bins[i], buf, unit, out, arg);
    }
  }
  if (found) {
    _pa_fprintf(out, arg, "\n");
    pa_print_header(out, arg);
  }
}
#endif



//------------------------------------------------------------
// Use an output wrapper for line-buffered output
// (which is nice when using loggers etc.)
//------------------------------------------------------------
typedef struct buffered_s {
  pa_output_fun* out;   // original output function
  void*          arg;   // and state
  char*          buf;   // local buffer of at least size `count+1`
  size_t         used;  // currently used chars `used <= count`
  size_t         count; // total chars available for output
} buffered_t;

static void pa_buffered_flush(buffered_t* buf) {
  buf->buf[buf->used] = 0;
  _pa_fputs(buf->out, buf->arg, NULL, buf->buf);
  buf->used = 0;
}

static void pa_cdecl pa_buffered_out(const char* msg, void* arg) {
  buffered_t* buf = (buffered_t*)arg;
  if (msg==NULL || buf==NULL) return;
  for (const char* src = msg; *src != 0; src++) {
    char c = *src;
    if (buf->used >= buf->count) pa_buffered_flush(buf);
    pa_assert_internal(buf->used < buf->count);
    buf->buf[buf->used++] = c;
    if (c == '\n') pa_buffered_flush(buf);
  }
}

//------------------------------------------------------------
// Print statistics
//------------------------------------------------------------

static void _pa_stats_print(pa_stats_t* stats, pa_output_fun* out0, void* arg0) pa_attr_noexcept {
  // wrap the output function to be line buffered
  char buf[256];
  buffered_t buffer = { out0, arg0, NULL, 0, 255 };
  buffer.buf = buf;
  pa_output_fun* out = &pa_buffered_out;
  void* arg = &buffer;

  // and print using that
  pa_print_header(out,arg);
  #if PA_STAT>1
  pa_stats_print_bins(stats->malloc_bins, PA_BIN_HUGE, "bin",out,arg);
  #endif
  #if PA_STAT
  pa_stat_print(&stats->malloc_normal, "binned", (stats->malloc_normal_count.total == 0 ? 1 : -1), out, arg);
  // pa_stat_print(&stats->malloc_large, "large", (stats->malloc_large_count.total == 0 ? 1 : -1), out, arg);
  pa_stat_print(&stats->malloc_huge, "huge", (stats->malloc_huge_count.total == 0 ? 1 : -1), out, arg);
  pa_stat_count_t total = { 0,0,0 };
  pa_stat_count_add_mt(&total, &stats->malloc_normal);
  // pa_stat_count_add(&total, &stats->malloc_large);
  pa_stat_count_add_mt(&total, &stats->malloc_huge);
  pa_stat_print_ex(&total, "total", 1, out, arg, "");
  #endif
  #if PA_STAT>1
  pa_stat_total_print(&stats->malloc_requested, "malloc req", 1, out, arg);
  _pa_fprintf(out, arg, "\n");
  #endif
  pa_stat_print_ex(&stats->reserved, "reserved", 1, out, arg, "");
  pa_stat_print_ex(&stats->committed, "committed", 1, out, arg, "");
  pa_stat_counter_print(&stats->reset, "reset", out, arg );
  pa_stat_counter_print(&stats->purged, "purged", out, arg );
  pa_stat_print_ex(&stats->page_committed, "touched", 1, out, arg, "");
  pa_stat_print(&stats->segments, "segments", -1, out, arg);
  pa_stat_print(&stats->segments_abandoned, "-abandoned", -1, out, arg);
  pa_stat_print(&stats->segments_cache, "-cached", -1, out, arg);
  pa_stat_print(&stats->pages, "pages", -1, out, arg);
  pa_stat_print(&stats->pages_abandoned, "-abandoned", -1, out, arg);
  pa_stat_counter_print(&stats->pages_extended, "-extended", out, arg);
  pa_stat_counter_print(&stats->pages_retire, "-retire", out, arg);
  pa_stat_counter_print(&stats->arena_count, "arenas", out, arg);
  // pa_stat_counter_print(&stats->arena_crossover_count, "-crossover", out, arg);
  pa_stat_counter_print(&stats->arena_rollback_count, "-rollback", out, arg);
  pa_stat_counter_print(&stats->mmap_calls, "mmaps", out, arg);
  pa_stat_counter_print(&stats->commit_calls, "commits", out, arg);
  pa_stat_counter_print(&stats->reset_calls, "resets", out, arg);
  pa_stat_counter_print(&stats->purge_calls, "purges", out, arg);
  pa_stat_counter_print(&stats->malloc_guarded_count, "guarded", out, arg);
  pa_stat_print(&stats->threads, "threads", -1, out, arg);
  pa_stat_average_print(stats->page_searches_count.total, stats->page_searches.total, "searches", out, arg);
  _pa_fprintf(out, arg, "%10s: %5i\n", "numa nodes", _pa_os_numa_node_count());

  size_t elapsed;
  size_t user_time;
  size_t sys_time;
  size_t current_rss;
  size_t peak_rss;
  size_t current_commit;
  size_t peak_commit;
  size_t page_faults;
  pa_process_info(&elapsed, &user_time, &sys_time, &current_rss, &peak_rss, &current_commit, &peak_commit, &page_faults);
  _pa_fprintf(out, arg, "%10s: %5zu.%03zu s\n", "elapsed", elapsed/1000, elapsed%1000);
  _pa_fprintf(out, arg, "%10s: user: %zu.%03zu s, system: %zu.%03zu s, faults: %zu, peak rss: ", "process",
              user_time/1000, user_time%1000, sys_time/1000, sys_time%1000, page_faults );
  pa_printf_amount((int64_t)peak_rss, 1, out, arg, "%s");
  if (peak_commit > 0) {
    _pa_fprintf(out, arg, ", peak commit: ");
    pa_printf_amount((int64_t)peak_commit, 1, out, arg, "%s");
  }
  _pa_fprintf(out, arg, "\n");
}

static pa_msecs_t pa_process_start; // = 0

static pa_stats_t* pa_stats_get_default(void) {
  pa_heap_t* heap = pa_heap_get_default();
  return &heap->tld->stats;
}

static void pa_stats_merge_from(pa_stats_t* stats) {
  if (stats != &_pa_stats_main) {
    pa_stats_add(&_pa_stats_main, stats);
    memset(stats, 0, sizeof(pa_stats_t));
  }
}

void pa_stats_reset(void) pa_attr_noexcept {
  pa_stats_t* stats = pa_stats_get_default();
  if (stats != &_pa_stats_main) { memset(stats, 0, sizeof(pa_stats_t)); }
  memset(&_pa_stats_main, 0, sizeof(pa_stats_t));
  if (pa_process_start == 0) { pa_process_start = _pa_clock_start(); };
}

void pa_stats_merge(void) pa_attr_noexcept {
  pa_stats_merge_from( pa_stats_get_default() );
}

void _pa_stats_merge_thread(pa_tld_t* tld) {
  pa_stats_merge_from( &tld->stats );
}

void _pa_stats_done(pa_stats_t* stats) {  // called from `pa_thread_done`
  pa_stats_merge_from(stats);
}

void pa_stats_print_out(pa_output_fun* out, void* arg) pa_attr_noexcept {
  pa_stats_merge_from(pa_stats_get_default());
  _pa_stats_print(&_pa_stats_main, out, arg);
}

void pa_stats_print(void* out) pa_attr_noexcept {
  // for compatibility there is an `out` parameter (which can be `stdout` or `stderr`)
  pa_stats_print_out((pa_output_fun*)out, NULL);
}

void pa_thread_stats_print_out(pa_output_fun* out, void* arg) pa_attr_noexcept {
  _pa_stats_print(pa_stats_get_default(), out, arg);
}


// ----------------------------------------------------------------
// Basic timer for convenience; use milli-seconds to avoid doubles
// ----------------------------------------------------------------

static pa_msecs_t pa_clock_diff;

pa_msecs_t _pa_clock_now(void) {
  return _pa_prim_clock_now();
}

pa_msecs_t _pa_clock_start(void) {
  if (pa_clock_diff == 0.0) {
    pa_msecs_t t0 = _pa_clock_now();
    pa_clock_diff = _pa_clock_now() - t0;
  }
  return _pa_clock_now();
}

pa_msecs_t _pa_clock_end(pa_msecs_t start) {
  pa_msecs_t end = _pa_clock_now();
  return (end - start - pa_clock_diff);
}


// --------------------------------------------------------
// Basic process statistics
// --------------------------------------------------------

pa_decl_export void pa_process_info(size_t* elapsed_msecs, size_t* user_msecs, size_t* system_msecs, size_t* current_rss, size_t* peak_rss, size_t* current_commit, size_t* peak_commit, size_t* page_faults) pa_attr_noexcept
{
  pa_process_info_t pinfo;
  _pa_memzero_var(pinfo);
  pinfo.elapsed        = _pa_clock_end(pa_process_start);
  pinfo.current_commit = (size_t)(pa_atomic_loadi64_relaxed((_Atomic(int64_t)*)&_pa_stats_main.committed.current));
  pinfo.peak_commit    = (size_t)(pa_atomic_loadi64_relaxed((_Atomic(int64_t)*)&_pa_stats_main.committed.peak));
  pinfo.current_rss    = pinfo.current_commit;
  pinfo.peak_rss       = pinfo.peak_commit;
  pinfo.utime          = 0;
  pinfo.stime          = 0;
  pinfo.page_faults    = 0;

  _pa_prim_process_info(&pinfo);

  if (elapsed_msecs!=NULL)  *elapsed_msecs  = (pinfo.elapsed < 0 ? 0 : (pinfo.elapsed < (pa_msecs_t)PTRDIFF_MAX ? (size_t)pinfo.elapsed : PTRDIFF_MAX));
  if (user_msecs!=NULL)     *user_msecs     = (pinfo.utime < 0 ? 0 : (pinfo.utime < (pa_msecs_t)PTRDIFF_MAX ? (size_t)pinfo.utime : PTRDIFF_MAX));
  if (system_msecs!=NULL)   *system_msecs   = (pinfo.stime < 0 ? 0 : (pinfo.stime < (pa_msecs_t)PTRDIFF_MAX ? (size_t)pinfo.stime : PTRDIFF_MAX));
  if (current_rss!=NULL)    *current_rss    = pinfo.current_rss;
  if (peak_rss!=NULL)       *peak_rss       = pinfo.peak_rss;
  if (current_commit!=NULL) *current_commit = pinfo.current_commit;
  if (peak_commit!=NULL)    *peak_commit    = pinfo.peak_commit;
  if (page_faults!=NULL)    *page_faults    = pinfo.page_faults;
}


// --------------------------------------------------------
// Return statistics
// --------------------------------------------------------

bool pa_stats_get(pa_stats_t* stats) pa_attr_noexcept {
  if (stats == NULL || stats->size != sizeof(pa_stats_t) || stats->version != PA_STAT_VERSION) return false;
  _pa_memzero(stats,stats->size);
  _pa_memcpy(stats, &_pa_stats_main, sizeof(pa_stats_t));
  return true;
}


// --------------------------------------------------------
// Statics in json format
// --------------------------------------------------------

typedef struct pa_heap_buf_s {
  char*   buf;
  size_t  size;
  size_t  used;
  bool    can_realloc;
} pa_heap_buf_t;

static bool pa_heap_buf_expand(pa_heap_buf_t* hbuf) {
  if (hbuf==NULL) return false;
  if (hbuf->buf != NULL && hbuf->size>0) {
    hbuf->buf[hbuf->size-1] = 0;
  }
  if (hbuf->size > SIZE_MAX/2 || !hbuf->can_realloc) return false;
  const size_t newsize = (hbuf->size == 0 ? pa_good_size(12*PA_KiB) : 2*hbuf->size);
  char* const  newbuf  = (char*)pa_rezalloc(hbuf->buf, newsize);
  if (newbuf == NULL) return false;
  hbuf->buf = newbuf;
  hbuf->size = newsize;
  return true;
}

static void pa_heap_buf_print(pa_heap_buf_t* hbuf, const char* msg) {
  if (msg==NULL || hbuf==NULL) return;
  if (hbuf->used + 1 >= hbuf->size && !hbuf->can_realloc) return;
  for (const char* src = msg; *src != 0; src++) {
    char c = *src;
    if (hbuf->used + 1 >= hbuf->size) {
      if (!pa_heap_buf_expand(hbuf)) return;
    }
    pa_assert_internal(hbuf->used < hbuf->size);
    hbuf->buf[hbuf->used++] = c;
  }
  pa_assert_internal(hbuf->used < hbuf->size);
  hbuf->buf[hbuf->used] = 0;
}

static void pa_heap_buf_print_count_bin(pa_heap_buf_t* hbuf, const char* prefix, pa_stat_count_t* stat, size_t bin, bool add_comma) {
  const size_t binsize = _pa_bin_size(bin);
  const size_t pagesize = (binsize <= PA_SMALL_OBJ_SIZE_MAX ? PA_SMALL_PAGE_SIZE :
                            (binsize <= PA_MEDIUM_OBJ_SIZE_MAX ? PA_MEDIUM_PAGE_SIZE :
                              #if PA_LARGE_PAGE_SIZE
                              (binsize <= PA_LARGE_OBJ_SIZE_MAX ? PA_LARGE_PAGE_SIZE : 0)
                              #else
                              0
                              #endif
                              ));
  char buf[128];
  _pa_snprintf(buf, 128, "%s{ \"total\": %lld, \"peak\": %lld, \"current\": %lld, \"block_size\": %zu, \"page_size\": %zu }%s\n", prefix, stat->total, stat->peak, stat->current, binsize, pagesize, (add_comma ? "," : ""));
  buf[127] = 0;
  pa_heap_buf_print(hbuf, buf);
}

static void pa_heap_buf_print_count(pa_heap_buf_t* hbuf, const char* prefix, pa_stat_count_t* stat, bool add_comma) {
  char buf[128];
  _pa_snprintf(buf, 128, "%s{ \"total\": %lld, \"peak\": %lld, \"current\": %lld }%s\n", prefix, stat->total, stat->peak, stat->current, (add_comma ? "," : ""));
  buf[127] = 0;
  pa_heap_buf_print(hbuf, buf);
}

static void pa_heap_buf_print_count_value(pa_heap_buf_t* hbuf, const char* name, pa_stat_count_t* stat) {
  char buf[128];
  _pa_snprintf(buf, 128, "  \"%s\": ", name);
  buf[127] = 0;
  pa_heap_buf_print(hbuf, buf);
  pa_heap_buf_print_count(hbuf, "", stat, true);
}

static void pa_heap_buf_print_value(pa_heap_buf_t* hbuf, const char* name, int64_t val) {
  char buf[128];
  _pa_snprintf(buf, 128, "  \"%s\": %lld,\n", name, val);
  buf[127] = 0;
  pa_heap_buf_print(hbuf, buf);
}

static void pa_heap_buf_print_size(pa_heap_buf_t* hbuf, const char* name, size_t val, bool add_comma) {
  char buf[128];
  _pa_snprintf(buf, 128, "    \"%s\": %zu%s\n", name, val, (add_comma ? "," : ""));
  buf[127] = 0;
  pa_heap_buf_print(hbuf, buf);
}

static void pa_heap_buf_print_counter_value(pa_heap_buf_t* hbuf, const char* name, pa_stat_counter_t* stat) {
  pa_heap_buf_print_value(hbuf, name, stat->total);
}

#define PA_STAT_COUNT(stat)    pa_heap_buf_print_count_value(&hbuf, #stat, &stats->stat);
#define PA_STAT_COUNTER(stat)  pa_heap_buf_print_counter_value(&hbuf, #stat, &stats->stat);

char* pa_stats_get_json(size_t output_size, char* output_buf) pa_attr_noexcept {
  pa_heap_buf_t hbuf = { NULL, 0, 0, true };
  if (output_size > 0 && output_buf != NULL) {
    _pa_memzero(output_buf, output_size);
    hbuf.buf = output_buf;
    hbuf.size = output_size;
    hbuf.can_realloc = false;
  }
  else {
    if (!pa_heap_buf_expand(&hbuf)) return NULL;
  }
  pa_heap_buf_print(&hbuf, "{\n");
  pa_heap_buf_print_value(&hbuf, "stat_version", PA_STAT_VERSION);
  pa_heap_buf_print_value(&hbuf, "palloc_version", PA_MALLOC_VERSION);

  // process info
  pa_heap_buf_print(&hbuf, "  \"process\": {\n");
  size_t elapsed;
  size_t user_time;
  size_t sys_time;
  size_t current_rss;
  size_t peak_rss;
  size_t current_commit;
  size_t peak_commit;
  size_t page_faults;
  pa_process_info(&elapsed, &user_time, &sys_time, &current_rss, &peak_rss, &current_commit, &peak_commit, &page_faults);
  pa_heap_buf_print_size(&hbuf, "elapsed_msecs", elapsed, true);
  pa_heap_buf_print_size(&hbuf, "user_msecs", user_time, true);
  pa_heap_buf_print_size(&hbuf, "system_msecs", sys_time, true);
  pa_heap_buf_print_size(&hbuf, "page_faults", page_faults, true);
  pa_heap_buf_print_size(&hbuf, "rss_current", current_rss, true);
  pa_heap_buf_print_size(&hbuf, "rss_peak", peak_rss, true);
  pa_heap_buf_print_size(&hbuf, "commit_current", current_commit, true);
  pa_heap_buf_print_size(&hbuf, "commit_peak", peak_commit, false);
  pa_heap_buf_print(&hbuf, "  },\n");

  // statistics
  pa_stats_t* stats = &_pa_stats_main;
  PA_STAT_FIELDS()

  // size bins
  pa_heap_buf_print(&hbuf, "  \"malloc_bins\": [\n");
  for (size_t i = 0; i <= PA_BIN_HUGE; i++) {
    pa_heap_buf_print_count_bin(&hbuf, "    ", &stats->malloc_bins[i], i, i!=PA_BIN_HUGE);
  }
  pa_heap_buf_print(&hbuf, "  ],\n");
  pa_heap_buf_print(&hbuf, "  \"page_bins\": [\n");
  for (size_t i = 0; i <= PA_BIN_HUGE; i++) {
    pa_heap_buf_print_count_bin(&hbuf, "    ", &stats->page_bins[i], i, i!=PA_BIN_HUGE);
  }
  pa_heap_buf_print(&hbuf, "  ]\n");
  pa_heap_buf_print(&hbuf, "}\n");
  if (hbuf.used >= hbuf.size) {
    // failed
    if (hbuf.can_realloc) { pa_free(hbuf.buf); }
    return NULL;
  }
  else {
    return hbuf.buf;
  }
}
