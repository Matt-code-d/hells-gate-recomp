#pragma once

#include <rex/ppc.h>
#include <rex/runtime.h>
#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <csetjmp>
#include <string_view>
#include <unordered_map>
#include <rex/system/kernel_state.h>
#include <rex/system/xfile.h>
#include <rex/system/xmemory.h>
#include <rex/system/xthread.h>

inline bool g_dlc_trace_enabled = false;
inline std::atomic<uint32_t> g_dlc_trace_events{0};
inline thread_local uint32_t g_dlc_trace_read = 0;

inline bool DlcTraceReadWord(uint32_t address, uint32_t& value) {
  auto* kernel = REX_KERNEL_STATE();
  if (!kernel || (address & 3) || address > UINT32_MAX - 4) return false;
  auto* memory = kernel->memory();
  auto* heap = memory->LookupHeap(address);
  rex::memory::HeapAllocationInfo info{};
  if (!heap || !heap->QueryRegionInfo(address, &info) ||
      !(info.state & rex::memory::kMemoryAllocationCommit) ||
      !(info.protect & rex::memory::kMemoryProtectRead) ||
      uint64_t(address) + 4 > uint64_t(info.base_address) + info.region_size) return false;
  value = std::byteswap(*memory->TranslateVirtual<const uint32_t*>(address));
  return true;
}

inline uint32_t DlcTraceEvent(const char* stage, uint32_t sp, uint32_t lr,
                              uint32_t object) {
  if (!g_dlc_trace_enabled) return 0;
  uint32_t id = g_dlc_trace_events.fetch_add(1, std::memory_order_relaxed) + 1;
  if (id > 128) {
    if (id == 129) REXLOG_INFO("DLC-TRACE: event limit reached (128)");
    return 0;
  }
  uint32_t slot = 0, flags = 0;
  DlcTraceReadWord(0x829B5478, slot);
  DlcTraceReadWord(0x829B557C, flags);
  REXLOG_INFO("DLC-TRACE: id={} stage={} thread={:08X} object={:08X} sp={:08X} "
              "lr={:08X} current_word={:08X} current_flag={}",
              id, stage, rex::system::XThread::GetCurrentThreadId(), object, sp,
              lr, slot, (flags >> 8) & 0xFF);
  uint32_t frame = sp;
  for (uint32_t depth = 0; depth < 16; ++depth) {
    uint32_t parent = 0, saved_lr = 0;
    if (!DlcTraceReadWord(frame, parent) || parent <= frame ||
        uint64_t(parent) - sp > 0x100000 || (parent & 15) ||
        !DlcTraceReadWord(parent - 8, saved_lr)) break;
    REXLOG_INFO("DLC-TRACE: id={} frame={} sp={:08X} parent={:08X} saved_lr={:08X}",
                id, depth, frame, parent, saved_lr);
    frame = parent;
  }
  return id;
}

inline void DlcTraceReadBegin(rex::ppc::Register& r1, rex::ppc::Register& r3,
                              rex::ppc::Register& r8, rex::ppc::Register& r9,
                              rex::ppc::Register& r10) {
  g_dlc_trace_read = 0;
  if (!g_dlc_trace_enabled) return;
  auto* kernel = REX_KERNEL_STATE();
  if (!kernel) return;
  auto file = kernel->object_table()->LookupObject<rex::system::XFile>(r3.u32);
  if (!file || !file->entry()) return;
  std::string name = file->name();
  for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  if (!name.ends_with(".dlm") && !name.ends_with(".lu2")) return;
  g_dlc_trace_read = DlcTraceEvent("module-read", r1.u32, 0x826AB810, r3.u32);
  if (!g_dlc_trace_read) return;
  uint32_t offset_hi = 0, offset_lo = 0;
  DlcTraceReadWord(r10.u32, offset_hi);
  if (r10.u32 <= UINT32_MAX - 4) DlcTraceReadWord(r10.u32 + 4, offset_lo);
  REXLOG_INFO("DLC-TRACE: id={} file={} buffer={:08X} requested={} offset={:08X}{:08X}",
              g_dlc_trace_read, file->path(), r8.u32, r9.u32, offset_hi, offset_lo);
}

inline void DlcTraceReadEnd(rex::ppc::Register& r1, rex::ppc::Register& r3) {
  if (!g_dlc_trace_enabled || !g_dlc_trace_read) return;
  uint32_t status = 0, bytes = 0;
  if (r1.u32 <= UINT32_MAX - 84) {
    DlcTraceReadWord(r1.u32 + 80, status);
    DlcTraceReadWord(r1.u32 + 84, bytes);
  }
  REXLOG_INFO("DLC-TRACE: id={} stage=read-complete result={:08X} iosb={:08X} bytes={}",
              g_dlc_trace_read, r3.u32, status, bytes);
  g_dlc_trace_read = 0;
}

inline std::string DlcTraceText(uint32_t address) {
  std::string text;
  for (uint32_t i = 0; i < 96 && address <= UINT32_MAX - i; ++i) {
    uint32_t word = 0, at = address + i;
    if (!DlcTraceReadWord(at & ~3u, word)) break;
    char c = static_cast<char>((word >> ((3 - (at & 3)) * 8)) & 0xFF);
    if (!c) break;
    text += (c >= 32 && c <= 126) ? c : '.';
  }
  return text;
}

inline void DlcTraceCommand(rex::ppc::Register& r1, rex::ppc::Register& r3,
                            rex::ppc::Register& r12) {
  if (!g_dlc_trace_enabled) return;
  static std::atomic<uint32_t> calls{0};
  if (calls.fetch_add(1, std::memory_order_relaxed) >= 32) return;
  uint32_t id = DlcTraceEvent("command-entry", r1.u32, r12.u32, r3.u32);
  if (id) REXLOG_INFO("DLC-TRACE: id={} command={}", id, DlcTraceText(r3.u32));
}

inline void DlcTraceDispatch(rex::ppc::Register& r1, rex::ppc::Register& r3,
                             rex::ppc::Register& r11, rex::ppc::Register& r28,
                             rex::ppc::Register& r31) {
  if (!g_dlc_trace_enabled) return;
  static std::atomic<uint32_t> calls{0};
  if (calls.fetch_add(1, std::memory_order_relaxed) >= 32) return;
  uint32_t id = DlcTraceEvent("command-dispatch", r1.u32, 0x8266E844, r3.u32);
  if (id) REXLOG_INFO("DLC-TRACE: id={} index={} target={:08X} name={} argument={}",
                      id, r28.u32, r11.u32, DlcTraceText(r31.u32), DlcTraceText(r3.u32));
}

inline void DlcTraceSetCurrent(rex::ppc::Register& r1, rex::ppc::Register& r3,
                               rex::ppc::Register& r12) {
  DlcTraceEvent("set-current", r1.u32, r12.u32, r3.u32);
}

inline void DlcTraceActivationGate(rex::ppc::Register& r1, rex::ppc::Register& r3,
                                   rex::ppc::Register& r12) {
  if (!g_dlc_trace_enabled) return;
  static std::atomic<uint32_t> calls{0};
  if (calls.fetch_add(1, std::memory_order_relaxed) < 32)
    DlcTraceEvent("activation-gate", r1.u32, r12.u32, r3.u32);
}

inline thread_local jmp_buf g_fiber_jmp_buf;
inline thread_local uint32_t g_setjmp_ctx_addr = 0;
inline thread_local uint32_t g_longjmp_return_value = 0;
inline thread_local bool g_fiber_setjmp_active = false;
inline thread_local void* g_fiber_setjmp_sp = nullptr;

inline thread_local uint32_t g_current_sdbm_id = 0;
inline thread_local uint32_t g_current_sdbm_producer = 0;

inline int FiberSetjmp(uint32_t ctx_addr) {
  g_setjmp_ctx_addr = ctx_addr;
  g_fiber_setjmp_active = true;
  int ret = setjmp(g_fiber_jmp_buf);
  if (ret != 0) {
    REXLOG_INFO("FIBER: setjmp returning from longjmp (ret={})", ret);
    g_fiber_setjmp_active = false;
  } else {
    int dummy;
    g_fiber_setjmp_sp = &dummy;
  }
  return ret;
}

inline void FiberLongjmp(uint32_t return_value) {
  g_longjmp_return_value = return_value;
  int current_sp;
  void* current_sp_ptr = &current_sp;
  bool sp_valid = g_fiber_setjmp_active &&
                  g_fiber_setjmp_sp != nullptr &&
                  current_sp_ptr <= g_fiber_setjmp_sp;
  REXLOG_INFO("FIBER: longjmp called with return_value={} active={} sp_valid={} cur_sp={} saved_sp={}",
              return_value, g_fiber_setjmp_active, sp_valid, current_sp_ptr, g_fiber_setjmp_sp);
  if (!sp_valid) {
    REXLOG_WARN("FIBER: longjmp with invalid/expired setjmp - skipping (DLC/content path)");
    g_fiber_setjmp_active = false;
    return;
  }
  longjmp(g_fiber_jmp_buf, 1);
}

inline void FiberRestoreContext(PPCContext& ctx, uint8_t* base) {
  if (g_setjmp_ctx_addr == 0) return;
  uint32_t addr = g_setjmp_ctx_addr;
  uint32_t phys_offset = (addr >= 0xE0000000u) ? 0x1000u : 0u;
  uint8_t* ptr = base + addr + phys_offset;
  ctx.r1.u64 = __builtin_bswap64(*reinterpret_cast<uint64_t*>(ptr + 144));
  ctx.r31.u64 = __builtin_bswap64(*reinterpret_cast<uint64_t*>(ptr + 296));
  ctx.r3.u32 = g_longjmp_return_value;
  REXLOG_INFO("FIBER: restored r1=0x{:08X} r31=0x{:08X} r3={}",
              ctx.r1.u32, ctx.r31.u32, ctx.r3.s32);
}

inline float g_ultrawide_target_aspect = 0.0f;
inline uint32_t g_ultrawide_hook_call_count = 0;
inline uint32_t g_ultrawide_xscale_hook_call_count = 0;

constexpr double kNativeAspect = 1.7777778;

inline void UltrawideAspectHook(rex::ppc::Register& f29) {
  if (g_ultrawide_target_aspect > 0.0f &&
      f29.f64 > 0.1 && f29.f64 < 1000.0) {
    uint32_t count = ++g_ultrawide_hook_call_count;
    if (count == 1 || (count % 300) == 0) {
      REXLOG_INFO("ULTRAWIDE: y-scale hook #{}, keeping aspect at {:.4f} (was {:.4f})",
                  count, kNativeAspect, f29.f64);
    }
    f29.f64 = kNativeAspect;
  }
}

inline void UltrawideXScaleHook(rex::ppc::Register& f12) {
  if (g_ultrawide_target_aspect > 0.0f) {
    double scale = kNativeAspect / static_cast<double>(g_ultrawide_target_aspect);
    uint32_t count = ++g_ultrawide_xscale_hook_call_count;
    if (count == 1 || (count % 300) == 0) {
      REXLOG_INFO("ULTRAWIDE: x-scale hook #{}, scaling m[0] by {:.4f} (f12={:.4f} -> {:.4f})",
                  count, scale, f12.f64, f12.f64 * scale);
    }
    f12.f64 = f12.f64 * scale;
  }
}

struct SdbmMessage {
  uint32_t hash;
  const char* name;
};

constexpr uint32_t SdbmHashCaseInsensitive(std::string_view s) {
  uint32_t h = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c >= 'A' && c <= 'Z') c += 32;
    h = (h * 0x1003F) + c;
  }
  return h;
}

constexpr std::array<SdbmMessage, 35> kSdbmMessages = {{
    
    {0x8F79C139, "RunningPreTick"},
    {0x89040464, "RunningTick"},
    {0xC6D366A4, "RunningPostTick"},
    {0xFBD9C4C9, "HavokManagerTick"},
    {0xE9404184, "SystemTick"},
    {0xC2DB6E5F, "PostSimulation"},
    {0xDFC458AD, "BeforeRender"},
    {0x3CC7DCA1, "PreRender"},
    {0x13CFE0B9, "DoRender"},
    {0x5F9B83E5, "PreShowRaster"},
    
    {SdbmHashCaseInsensitive("iMsgPausedPreTick"), "PausedPreTick"},
    {SdbmHashCaseInsensitive("iMsgPausedTick"), "PausedTick"},
    {SdbmHashCaseInsensitive("iMsgPausedPostTick"), "PausedPostTick"},
    {SdbmHashCaseInsensitive("iMsgSetPausedMode"), "SetPausedMode"},
    {SdbmHashCaseInsensitive("iMsgSetFrozenMode"), "SetFrozenMode"},
    {SdbmHashCaseInsensitive("iMsgSetRunningMode"), "SetRunningMode"},
    
    {SdbmHashCaseInsensitive("iMsgMenuIntro"), "MenuIntro"},
    {SdbmHashCaseInsensitive("iMsgMenuOutro"), "MenuOutro"},
    {SdbmHashCaseInsensitive("iMsgIGCShotStart"), "IGCShotStart"},
    {SdbmHashCaseInsensitive("iMsgIGCDisableActor"), "IGCDisableActor"},
    {SdbmHashCaseInsensitive("iMsgIGCEnableActor"), "IGCEnableActor"},
    {SdbmHashCaseInsensitive("iMsgIGCDisablePlayerControl"), "IGCDisablePlayerControl"},
    {SdbmHashCaseInsensitive("iMsgIGCEnablePlayerControl"), "IGCEnablePlayerControl"},
    
    {SdbmHashCaseInsensitive("iMsgSequenceInstanceCreate"), "SequenceInstanceCreate"},
    {SdbmHashCaseInsensitive("iMsgSequenceInstanceReady"), "SequenceInstanceReady"},
    {SdbmHashCaseInsensitive("iMsgVideoStart"), "VideoStart"},
    {SdbmHashCaseInsensitive("iMsgVideoStop"), "VideoStop"},
    {SdbmHashCaseInsensitive("iMsgMovieStarted"), "MovieStarted"},
    {SdbmHashCaseInsensitive("iMsgMovieEnded"), "MovieEnded"},
    {SdbmHashCaseInsensitive("iMsgMoviePlayerEnded"), "MoviePlayerEnded"},
    {SdbmHashCaseInsensitive("iMsgUIMinigameAbsolveIconCaught"), "AbsolveIconCaught"},
    {SdbmHashCaseInsensitive("iMsgUIMinigameAbsolveIconLost"), "AbsolveIconLost"},
    {SdbmHashCaseInsensitive("iMsgUIMinigameAbsolveSoulCounterStart"), "AbsolveCounterStart"},
    {SdbmHashCaseInsensitive("iMsgUIMinigameAbsolveSoulCounterRoll"), "AbsolveCounterRoll"},
    {SdbmHashCaseInsensitive("iMsgUIMinigameAbsolveSoulCounterStop"), "AbsolveCounterStop"},
}};

static_assert(kSdbmMessages[10].hash == SdbmHashCaseInsensitive("iMsgPausedPreTick"));
static_assert(kSdbmMessages[11].hash == SdbmHashCaseInsensitive("iMsgPausedTick"));
static_assert(kSdbmMessages[12].hash == SdbmHashCaseInsensitive("iMsgPausedPostTick"));
static_assert(kSdbmMessages[13].hash == SdbmHashCaseInsensitive("iMsgSetPausedMode"));
static_assert(kSdbmMessages[14].hash == SdbmHashCaseInsensitive("iMsgSetFrozenMode"));
static_assert(kSdbmMessages[15].hash == SdbmHashCaseInsensitive("iMsgSetRunningMode"));
static_assert(kSdbmMessages[16].hash == SdbmHashCaseInsensitive("iMsgMenuIntro"));
static_assert(kSdbmMessages[17].hash == SdbmHashCaseInsensitive("iMsgMenuOutro"));
static_assert(kSdbmMessages[18].hash == SdbmHashCaseInsensitive("iMsgIGCShotStart"));
static_assert(kSdbmMessages[19].hash == SdbmHashCaseInsensitive("iMsgIGCDisableActor"));
static_assert(kSdbmMessages[20].hash == SdbmHashCaseInsensitive("iMsgIGCEnableActor"));
static_assert(kSdbmMessages[21].hash == SdbmHashCaseInsensitive("iMsgIGCDisablePlayerControl"));
static_assert(kSdbmMessages[22].hash == SdbmHashCaseInsensitive("iMsgIGCEnablePlayerControl"));
static_assert(kSdbmMessages[23].hash == SdbmHashCaseInsensitive("iMsgSequenceInstanceCreate"));
static_assert(kSdbmMessages[24].hash == SdbmHashCaseInsensitive("iMsgSequenceInstanceReady"));
static_assert(kSdbmMessages[25].hash == SdbmHashCaseInsensitive("iMsgVideoStart"));
static_assert(kSdbmMessages[26].hash == SdbmHashCaseInsensitive("iMsgVideoStop"));
static_assert(kSdbmMessages[27].hash == SdbmHashCaseInsensitive("iMsgMovieStarted"));
static_assert(kSdbmMessages[28].hash == SdbmHashCaseInsensitive("iMsgMovieEnded"));
static_assert(kSdbmMessages[29].hash == SdbmHashCaseInsensitive("iMsgMoviePlayerEnded"));

constexpr std::array<std::string_view, 19> kSdbmTrackedHandlerNames = {
    "RunningPreTick", "RunningTick", "RunningPostTick",
    "PausedPreTick",  "PausedTick",  "PausedPostTick",
    "SystemTick",
    "IGCShotStart",          "IGCDisableActor",          "IGCEnableActor",
    "IGCDisablePlayerControl", "IGCEnablePlayerControl",
    "SequenceInstanceCreate",  "SequenceInstanceReady",
    "AbsolveIconCaught", "AbsolveIconLost", "AbsolveCounterStart",
    "AbsolveCounterRoll", "AbsolveCounterStop"};

constexpr bool IsSdbmHandlerTracked(uint32_t id) {
  for (size_t i = 0; i < kSdbmMessages.size(); ++i) {
    if (kSdbmMessages[i].hash == id) {
      for (std::string_view name : kSdbmTrackedHandlerNames) {
        if (name == kSdbmMessages[i].name) {
          return true;
        }
      }
      return false;
    }
  }
  return false;
}

constexpr size_t kSdbmHandlerTableSize = 128;

struct DiagSnapshot {
  uint64_t vblank_callback_count = 0;
  uint32_t vblank_callback_source = 0;
  uint32_t vblank_callback_arg = 0;
  uint64_t vblank_event_processor_count = 0;
  uint64_t callback_4170_count = 0;
  uint32_t callback_4170_target = 0;
  uint64_t callback_416c_value_count = 0;
  uint64_t callback_416c_call_count = 0;
  uint32_t callback_416c_target = 0;
  uint64_t f324_entry_count = 0;
  uint32_t value_5504 = 0;
  uint32_t delta_f3bc = 0;
  uint32_t delta_f3c8 = 0;
  uint32_t final_delta = 0;
  uint64_t hb_reader_entry_count = 0;
  uint64_t hb_published_count = 0;
  uint32_t hb_published_value = 0;
  uint64_t hb_consumed_count = 0;
  uint32_t hb_consumed_value = 0;
  uint64_t hb_call_boundary_count = 0;
  uint64_t cb_sched_entry_count = 0;
  uint64_t cb_slot_iter_entry_count = 0;
  uint64_t cb_dispatch_entry_count = 0;
  uint32_t cb_dispatch_slot = 0;
  uint32_t cb_dispatch_object = 0;
  uint32_t cb_dispatch_arg = 0;
  uint64_t engine_ui_dispatch_count = 0;
  uint64_t vdswap_count = 0;
  uint64_t render_loop_entry_count = 0;
  uint64_t render_loop_loopback_count = 0;
  std::array<uint32_t, 32> scheduler_message_ids{};
  std::array<uint64_t, 32> scheduler_message_counts{};
  uint64_t scheduler_message_overflow_count = 0;
  uint32_t scheduler_message_pointer = 0;
  uint64_t scheduler_message_f1 = 0;
  uint64_t scheduler_message_f2 = 0;
  uint64_t scheduler_message_f3 = 0;
  uint32_t sdbm_latest_id = 0;
  uint32_t sdbm_latest_payload = 0;
  uint32_t sdbm_latest_context = 0;
  std::array<uint64_t, kSdbmMessages.size()> sdbm_message_counts{};
  uint64_t sdbm_message_unknown_count = 0;
  uint64_t presentation_loop_count = 0;
  uint32_t presentation_async_flag = 0;
  float base_hz_original = 0.0f;
  float base_hz_applied = 0.0f;
  uint64_t base_hz_writer_hook_count = 0;
  float base_hz_reader_value = 0.0f;
  uint64_t base_hz_reader_hook_count = 0;
  
  uint32_t mode_byte_value = 0;
  uint32_t mode_byte_object = 0;
  uint64_t mode_byte_transitions = 0;
  
  uint64_t onethirty_a_count = 0;
  float onethirty_a_value = 0.0f;
  uint32_t onethirty_a_object = 0;
  uint64_t onethirty_b_count = 0;
  float onethirty_b_value = 0.0f;
  uint32_t onethirty_b_object = 0;
  
  std::array<uint32_t, 32> sdbm_unknown_ids{};
  std::array<uint64_t, 32> sdbm_unknown_counts{};

  std::array<uint32_t, kSdbmHandlerTableSize> sdbm_handler_ids{};
  std::array<uint32_t, kSdbmHandlerTableSize> sdbm_handler_producers{};
  std::array<uint32_t, kSdbmHandlerTableSize> sdbm_handler_targets{};
  std::array<uint32_t, kSdbmHandlerTableSize> sdbm_handler_objs{};
  std::array<uint64_t, kSdbmHandlerTableSize> sdbm_handler_counts{};
  uint64_t sdbm_handler_overflow_count = 0;

  std::array<uint64_t, 8> ui_fix_site_counts{};
  std::array<float, 8> ui_fix_site_original{};
  std::array<float, 8> ui_fix_site_applied{};

  uint64_t catching_souls_hit_count = 0;
  float catching_souls_original_increment = 0.0f;
  float catching_souls_scaled_increment = 0.0f;
  float catching_souls_factor = 1.0f;

  uint64_t absolve_timer_increment_calls = 0;
  float absolve_timer_last_original = 0.0f;
  float absolve_timer_last_scaled = 0.0f;
  uint64_t absolve_update_vtable_calls = 0;
  uint64_t absolve_prompt_timer_calls = 0;
  float absolve_prompt_timer_last_value = 0.0f;
  float absolve_prompt_timer_last_threshold = 0.0f;
  uint32_t absolve_prompt_timer_last_object = 0;
  uint64_t absolve_prompt_timer_increment_calls = 0;
  float absolve_prompt_timer_last_original = 0.0f;
  float absolve_prompt_timer_last_scaled = 0.0f;
  uint64_t absolve_roll_step_calls = 0;
  float absolve_roll_step_last_original = 0.0f;
  float absolve_roll_step_last_scaled = 0.0f;
};

inline struct DiagCounters {
  std::atomic<uint64_t> vblank_callback_count{0};
  std::atomic<uint32_t> vblank_callback_source{0};
  std::atomic<uint32_t> vblank_callback_arg{0};
  std::atomic<uint64_t> vblank_event_processor_count{0};
  std::atomic<uint64_t> callback_4170_count{0};
  std::atomic<uint32_t> callback_4170_target{0};
  std::atomic<uint64_t> callback_416c_value_count{0};
  std::atomic<uint64_t> callback_416c_call_count{0};
  std::atomic<uint32_t> callback_416c_target{0};
  std::atomic<uint64_t> f324_entry_count{0};
  std::atomic<uint32_t> value_5504{0};
  std::atomic<uint32_t> delta_f3bc{0};
  std::atomic<uint32_t> delta_f3c8{0};
  std::atomic<uint32_t> final_delta{0};
  std::atomic<uint64_t> hb_reader_entry_count{0};
  std::atomic<uint64_t> hb_published_count{0};
  std::atomic<uint32_t> hb_published_value{0};
  std::atomic<uint64_t> hb_consumed_count{0};
  std::atomic<uint32_t> hb_consumed_value{0};
  std::atomic<uint64_t> hb_call_boundary_count{0};
  std::atomic<uint64_t> cb_sched_entry_count{0};
  std::atomic<uint64_t> cb_slot_iter_entry_count{0};
  std::atomic<uint64_t> cb_dispatch_entry_count{0};
  std::atomic<uint32_t> cb_dispatch_slot{0};
  std::atomic<uint32_t> cb_dispatch_object{0};
  std::atomic<uint32_t> cb_dispatch_arg{0};
  std::atomic<uint64_t> engine_ui_dispatch_count{0};
  std::atomic<uint64_t> vdswap_count{0};
  std::atomic<uint64_t> render_loop_entry_count{0};
  std::atomic<uint64_t> render_loop_loopback_count{0};
  struct SchedulerMessageSlot {
    std::atomic<uint32_t> id{UINT32_MAX};
    std::atomic<uint64_t> count{0};
  };
  std::array<SchedulerMessageSlot, 32> scheduler_messages{};
  std::atomic<uint64_t> scheduler_message_overflow_count{0};
  std::atomic<uint32_t> scheduler_message_pointer{0};
  std::atomic<uint64_t> scheduler_message_f1{0};
  std::atomic<uint64_t> scheduler_message_f2{0};
  std::atomic<uint64_t> scheduler_message_f3{0};
  std::atomic<uint32_t> sdbm_latest_id{0};
  std::atomic<uint32_t> sdbm_latest_payload{0};
  std::atomic<uint32_t> sdbm_latest_context{0};
  std::array<std::atomic<uint64_t>, kSdbmMessages.size()> sdbm_message_counts{};
  std::atomic<uint64_t> sdbm_message_unknown_count{0};
  std::atomic<uint64_t> presentation_loop_count{0};
  std::atomic<uint32_t> presentation_async_flag{0};
  std::atomic<float> base_hz_original{0.0f};
  std::atomic<float> base_hz_applied{0.0f};
  std::atomic<uint64_t> base_hz_writer_hook_count{0};
  std::atomic<float> base_hz_reader_value{0.0f};
  std::atomic<uint64_t> base_hz_reader_hook_count{0};

  std::atomic<uint32_t> mode_byte_value{0};
  std::atomic<uint32_t> mode_byte_object{0};
  std::atomic<uint64_t> mode_byte_transitions{0};

  std::atomic<uint64_t> onethirty_a_count{0};
  std::atomic<float> onethirty_a_value{0.0f};
  std::atomic<uint32_t> onethirty_a_object{0};
  std::atomic<uint64_t> onethirty_b_count{0};
  std::atomic<float> onethirty_b_value{0.0f};
  std::atomic<uint32_t> onethirty_b_object{0};

  struct SdbmUnknownSlot {
    std::atomic<uint32_t> id{UINT32_MAX};
    std::atomic<uint64_t> count{0};
  };
  std::array<SdbmUnknownSlot, 32> sdbm_unknown_slots{};

  struct SdbmHandlerSlot {
    std::atomic<uint32_t> id{UINT32_MAX};
    std::atomic<uint32_t> producer{0};
    std::atomic<uint32_t> target{0};
    std::atomic<uint32_t> obj{0};
    std::atomic<uint64_t> count{0};
  };
  std::array<SdbmHandlerSlot, kSdbmHandlerTableSize> sdbm_handler_table{};
  std::atomic<uint64_t> sdbm_handler_overflow_count{0};

  std::array<std::atomic<uint64_t>, 8> ui_fix_site_counts{};
  std::array<std::atomic<float>, 8> ui_fix_site_original{};
  std::array<std::atomic<float>, 8> ui_fix_site_applied{};

  std::atomic<uint64_t> catching_souls_hit_count{0};
  std::atomic<float> catching_souls_original_increment{0.0f};
  std::atomic<float> catching_souls_scaled_increment{0.0f};
  std::atomic<float> catching_souls_factor{1.0f};

  std::atomic<uint64_t> absolve_timer_increment_calls{0};
  std::atomic<float> absolve_timer_last_original{0.0f};
  std::atomic<float> absolve_timer_last_scaled{0.0f};
  std::atomic<uint64_t> absolve_update_vtable_calls{0};
  std::atomic<uint64_t> absolve_prompt_timer_calls{0};
  std::atomic<float> absolve_prompt_timer_last_value{0.0f};
  std::atomic<float> absolve_prompt_timer_last_threshold{0.0f};
  std::atomic<uint32_t> absolve_prompt_timer_last_object{0};
  std::atomic<uint64_t> absolve_prompt_timer_increment_calls{0};
  std::atomic<float> absolve_prompt_timer_last_original{0.0f};
  std::atomic<float> absolve_prompt_timer_last_scaled{0.0f};
  std::atomic<uint64_t> absolve_roll_step_calls{0};
  std::atomic<float> absolve_roll_step_last_original{0.0f};
  std::atomic<float> absolve_roll_step_last_scaled{0.0f};

  void Snapshot(DiagSnapshot& out) const {
    out.vblank_callback_count = vblank_callback_count.load(std::memory_order_relaxed);
    out.vblank_callback_source = vblank_callback_source.load(std::memory_order_relaxed);
    out.vblank_callback_arg = vblank_callback_arg.load(std::memory_order_relaxed);
    out.vblank_event_processor_count = vblank_event_processor_count.load(std::memory_order_relaxed);
    out.callback_4170_count = callback_4170_count.load(std::memory_order_relaxed);
    out.callback_4170_target = callback_4170_target.load(std::memory_order_relaxed);
    out.callback_416c_value_count = callback_416c_value_count.load(std::memory_order_relaxed);
    out.callback_416c_call_count = callback_416c_call_count.load(std::memory_order_relaxed);
    out.callback_416c_target = callback_416c_target.load(std::memory_order_relaxed);
    out.f324_entry_count = f324_entry_count.load(std::memory_order_relaxed);
    out.value_5504 = value_5504.load(std::memory_order_relaxed);
    out.delta_f3bc = delta_f3bc.load(std::memory_order_relaxed);
    out.delta_f3c8 = delta_f3c8.load(std::memory_order_relaxed);
    out.final_delta = final_delta.load(std::memory_order_relaxed);
    out.hb_reader_entry_count = hb_reader_entry_count.load(std::memory_order_relaxed);
    out.hb_published_count = hb_published_count.load(std::memory_order_relaxed);
    out.hb_published_value = hb_published_value.load(std::memory_order_relaxed);
    out.hb_consumed_count = hb_consumed_count.load(std::memory_order_relaxed);
    out.hb_consumed_value = hb_consumed_value.load(std::memory_order_relaxed);
    out.hb_call_boundary_count = hb_call_boundary_count.load(std::memory_order_relaxed);
    out.cb_sched_entry_count = cb_sched_entry_count.load(std::memory_order_relaxed);
    out.cb_slot_iter_entry_count = cb_slot_iter_entry_count.load(std::memory_order_relaxed);
    out.cb_dispatch_entry_count = cb_dispatch_entry_count.load(std::memory_order_relaxed);
    out.cb_dispatch_slot = cb_dispatch_slot.load(std::memory_order_relaxed);
    out.cb_dispatch_object = cb_dispatch_object.load(std::memory_order_relaxed);
    out.cb_dispatch_arg = cb_dispatch_arg.load(std::memory_order_relaxed);
    out.engine_ui_dispatch_count = engine_ui_dispatch_count.load(std::memory_order_relaxed);
    out.vdswap_count = vdswap_count.load(std::memory_order_relaxed);
    out.render_loop_entry_count = render_loop_entry_count.load(std::memory_order_relaxed);
    out.render_loop_loopback_count = render_loop_loopback_count.load(std::memory_order_relaxed);
    for (size_t i = 0; i < scheduler_messages.size(); ++i) {
      out.scheduler_message_ids[i] = scheduler_messages[i].id.load(std::memory_order_relaxed);
      out.scheduler_message_counts[i] = scheduler_messages[i].count.load(std::memory_order_relaxed);
    }
    out.scheduler_message_overflow_count = scheduler_message_overflow_count.load(std::memory_order_relaxed);
    out.scheduler_message_pointer = scheduler_message_pointer.load(std::memory_order_relaxed);
    out.scheduler_message_f1 = scheduler_message_f1.load(std::memory_order_relaxed);
    out.scheduler_message_f2 = scheduler_message_f2.load(std::memory_order_relaxed);
    out.scheduler_message_f3 = scheduler_message_f3.load(std::memory_order_relaxed);
    out.sdbm_latest_id = sdbm_latest_id.load(std::memory_order_relaxed);
    out.sdbm_latest_payload = sdbm_latest_payload.load(std::memory_order_relaxed);
    out.sdbm_latest_context = sdbm_latest_context.load(std::memory_order_relaxed);
    for (size_t i = 0; i < sdbm_message_counts.size(); ++i) {
      out.sdbm_message_counts[i] = sdbm_message_counts[i].load(std::memory_order_relaxed);
    }
    out.sdbm_message_unknown_count = sdbm_message_unknown_count.load(std::memory_order_relaxed);
    out.presentation_loop_count = presentation_loop_count.load(std::memory_order_relaxed);
    out.presentation_async_flag = presentation_async_flag.load(std::memory_order_relaxed);
    out.base_hz_original = base_hz_original.load(std::memory_order_relaxed);
    out.base_hz_applied = base_hz_applied.load(std::memory_order_relaxed);
    out.base_hz_writer_hook_count = base_hz_writer_hook_count.load(std::memory_order_relaxed);
    out.base_hz_reader_value = base_hz_reader_value.load(std::memory_order_relaxed);
    out.base_hz_reader_hook_count = base_hz_reader_hook_count.load(std::memory_order_relaxed);
    out.mode_byte_value = mode_byte_value.load(std::memory_order_relaxed);
    out.mode_byte_object = mode_byte_object.load(std::memory_order_relaxed);
    out.mode_byte_transitions = mode_byte_transitions.load(std::memory_order_relaxed);
    out.onethirty_a_count = onethirty_a_count.load(std::memory_order_relaxed);
    out.onethirty_a_value = onethirty_a_value.load(std::memory_order_relaxed);
    out.onethirty_a_object = onethirty_a_object.load(std::memory_order_relaxed);
    out.onethirty_b_count = onethirty_b_count.load(std::memory_order_relaxed);
    out.onethirty_b_value = onethirty_b_value.load(std::memory_order_relaxed);
    out.onethirty_b_object = onethirty_b_object.load(std::memory_order_relaxed);
    for (size_t i = 0; i < sdbm_unknown_slots.size(); ++i) {
      out.sdbm_unknown_ids[i] = sdbm_unknown_slots[i].id.load(std::memory_order_relaxed);
      out.sdbm_unknown_counts[i] = sdbm_unknown_slots[i].count.load(std::memory_order_relaxed);
    }
    for (size_t i = 0; i < sdbm_handler_table.size(); ++i) {
      out.sdbm_handler_ids[i] = sdbm_handler_table[i].id.load(std::memory_order_relaxed);
      out.sdbm_handler_producers[i] = sdbm_handler_table[i].producer.load(std::memory_order_relaxed);
      out.sdbm_handler_targets[i] = sdbm_handler_table[i].target.load(std::memory_order_relaxed);
      out.sdbm_handler_objs[i] = sdbm_handler_table[i].obj.load(std::memory_order_relaxed);
      out.sdbm_handler_counts[i] = sdbm_handler_table[i].count.load(std::memory_order_relaxed);
    }
    out.sdbm_handler_overflow_count = sdbm_handler_overflow_count.load(std::memory_order_relaxed);
    for (size_t i = 0; i < ui_fix_site_counts.size(); ++i) {
      out.ui_fix_site_counts[i] = ui_fix_site_counts[i].load(std::memory_order_relaxed);
      out.ui_fix_site_original[i] = ui_fix_site_original[i].load(std::memory_order_relaxed);
      out.ui_fix_site_applied[i] = ui_fix_site_applied[i].load(std::memory_order_relaxed);
    }
    out.catching_souls_hit_count = catching_souls_hit_count.load(std::memory_order_relaxed);
    out.catching_souls_original_increment =
        catching_souls_original_increment.load(std::memory_order_relaxed);
    out.catching_souls_scaled_increment =
        catching_souls_scaled_increment.load(std::memory_order_relaxed);
    out.catching_souls_factor = catching_souls_factor.load(std::memory_order_relaxed);
    out.absolve_timer_increment_calls =
        absolve_timer_increment_calls.load(std::memory_order_relaxed);
    out.absolve_timer_last_original =
        absolve_timer_last_original.load(std::memory_order_relaxed);
    out.absolve_timer_last_scaled =
        absolve_timer_last_scaled.load(std::memory_order_relaxed);
    out.absolve_update_vtable_calls =
        absolve_update_vtable_calls.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_calls =
        absolve_prompt_timer_calls.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_last_value =
        absolve_prompt_timer_last_value.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_last_threshold =
        absolve_prompt_timer_last_threshold.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_last_object =
        absolve_prompt_timer_last_object.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_increment_calls =
        absolve_prompt_timer_increment_calls.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_last_original =
        absolve_prompt_timer_last_original.load(std::memory_order_relaxed);
    out.absolve_prompt_timer_last_scaled =
        absolve_prompt_timer_last_scaled.load(std::memory_order_relaxed);
    out.absolve_roll_step_calls =
        absolve_roll_step_calls.load(std::memory_order_relaxed);
    out.absolve_roll_step_last_original =
        absolve_roll_step_last_original.load(std::memory_order_relaxed);
    out.absolve_roll_step_last_scaled =
        absolve_roll_step_last_scaled.load(std::memory_order_relaxed);
  }
} g_diag_counters;

inline void DiagVblankCallback(rex::ppc::Register& r3, rex::ppc::Register& r4) {
  g_diag_counters.vblank_callback_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.vblank_callback_source.store(r3.u32, std::memory_order_relaxed);
  g_diag_counters.vblank_callback_arg.store(r4.u32, std::memory_order_relaxed);
}

inline void DiagVblankEventProcessor() {
  g_diag_counters.vblank_event_processor_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagCallback4170Call(rex::ppc::Register& r11) {
  g_diag_counters.callback_4170_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.callback_4170_target.store(r11.u32, std::memory_order_relaxed);
}

inline void DiagCallback416cValue(rex::ppc::Register& r9) {
  g_diag_counters.callback_416c_value_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.callback_416c_target.store(r9.u32, std::memory_order_relaxed);
}

inline void DiagCallback416cCall(rex::ppc::Register& r9) {
  g_diag_counters.callback_416c_call_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.callback_416c_target.store(r9.u32, std::memory_order_relaxed);
}

inline void DiagF324Entry() {
  g_diag_counters.f324_entry_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagValue5504(rex::ppc::Register& r11) {
  g_diag_counters.value_5504.store(r11.u32, std::memory_order_relaxed);
}

inline void DiagDeltaF3BC(rex::ppc::Register& r7) {
  g_diag_counters.delta_f3bc.store(r7.u32, std::memory_order_relaxed);
}

inline void DiagDeltaF3C8(rex::ppc::Register& r7) {
  g_diag_counters.delta_f3c8.store(r7.u32, std::memory_order_relaxed);
}

inline void DiagEngineUiDispatch() {
  g_diag_counters.engine_ui_dispatch_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagVdSwap() {
  g_diag_counters.vdswap_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagRenderLoopEntry() {
  g_diag_counters.render_loop_entry_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagRenderLoopLoopback() {
  g_diag_counters.render_loop_loopback_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagFinalDelta(rex::ppc::Register& r7) {
  g_diag_counters.final_delta.store(r7.u32, std::memory_order_relaxed);
}

inline void DiagHeartbeatReaderEntry() {
  g_diag_counters.hb_reader_entry_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagHeartbeatPublished(rex::ppc::Register& r10) {
  g_diag_counters.hb_published_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.hb_published_value.store(r10.u32, std::memory_order_relaxed);
}

inline void DiagHeartbeatConsumed(rex::ppc::Register& r11) {
  g_diag_counters.hb_consumed_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.hb_consumed_value.store(r11.u32, std::memory_order_relaxed);
}

inline void DiagHeartbeatCallBoundary() {
  g_diag_counters.hb_call_boundary_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagCallbackSchedulerEntry() {
  g_diag_counters.cb_sched_entry_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagCallbackSlotIteratorEntry() {
  g_diag_counters.cb_slot_iter_entry_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagCallbackDispatchEntry(rex::ppc::Register& r3,
                                      rex::ppc::Register& r4,
                                      rex::ppc::Register& r5) {
  g_diag_counters.cb_dispatch_entry_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.cb_dispatch_slot.store(r3.u32, std::memory_order_relaxed);
  g_diag_counters.cb_dispatch_object.store(r4.u32, std::memory_order_relaxed);
  g_diag_counters.cb_dispatch_arg.store(r5.u32, std::memory_order_relaxed);
}

inline void DiagSchedulerMessage(rex::ppc::Register& r11,
                                 rex::ppc::Register& r4,
                                 rex::ppc::Register& f1,
                                 rex::ppc::Register& f2,
                                 rex::ppc::Register& f3) {
  const uint32_t id = r11.u32;
  g_diag_counters.scheduler_message_pointer.store(r4.u32, std::memory_order_relaxed);
  g_diag_counters.scheduler_message_f1.store(std::bit_cast<uint64_t>(f1.f64), std::memory_order_relaxed);
  g_diag_counters.scheduler_message_f2.store(std::bit_cast<uint64_t>(f2.f64), std::memory_order_relaxed);
  g_diag_counters.scheduler_message_f3.store(std::bit_cast<uint64_t>(f3.f64), std::memory_order_relaxed);
  for (auto& slot : g_diag_counters.scheduler_messages) {
    uint32_t slot_id = slot.id.load(std::memory_order_relaxed);
    if (slot_id == id) {
      slot.count.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (slot_id == UINT32_MAX) {
      if (slot.id.compare_exchange_strong(slot_id, id, std::memory_order_relaxed,
                                          std::memory_order_relaxed) ||
          slot_id == id) {
        slot.count.fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }
  }
  g_diag_counters.scheduler_message_overflow_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagPresentationLoop(rex::ppc::Register& r31) {
  g_diag_counters.presentation_loop_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.presentation_async_flag.store(r31.u32, std::memory_order_relaxed);
}

inline void DiagSdbmDispatcher(rex::ppc::Register& r11,
                               rex::ppc::Register& r3,
                               rex::ppc::Register& r4,
                               rex::ppc::Register& r12) {
  const uint32_t id = r11.u32;
  
  g_current_sdbm_id = id;
  g_current_sdbm_producer = r12.u32;
  g_diag_counters.sdbm_latest_id.store(id, std::memory_order_relaxed);
  g_diag_counters.sdbm_latest_payload.store(r3.u32, std::memory_order_relaxed);
  g_diag_counters.sdbm_latest_context.store(r4.u32, std::memory_order_relaxed);
  for (size_t i = 0; i < kSdbmMessages.size(); ++i) {
    if (kSdbmMessages[i].hash == id) {
      g_diag_counters.sdbm_message_counts[i].fetch_add(1, std::memory_order_relaxed);
      return;
    }
  }
  
  g_diag_counters.sdbm_message_unknown_count.fetch_add(1, std::memory_order_relaxed);
  for (auto& slot : g_diag_counters.sdbm_unknown_slots) {
    uint32_t slot_id = slot.id.load(std::memory_order_relaxed);
    if (slot_id == id) {
      slot.count.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (slot_id == UINT32_MAX) {
      if (slot.id.compare_exchange_strong(slot_id, id, std::memory_order_relaxed,
                                          std::memory_order_relaxed) ||
          slot.id.load(std::memory_order_relaxed) == id) {
        slot.count.fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }
  }
}

inline void DiagSdbmHandlerCall(rex::ppc::Register& r11,
                                rex::ppc::Register& r3,
                                rex::ppc::Register& r4) {
  
  (void)r4;
  const uint32_t id = g_current_sdbm_id;
  const uint32_t producer = g_current_sdbm_producer;
  const uint32_t target = r11.u32;
  const uint32_t obj = r3.u32;

  if (id == 0xC74083FE || id == 0x088EF246 || id == 0xE83C34E2 ||
      id == 0x4957B47D || id == 0x7A57D382) {
    REXLOG_INFO("DIAG_ABSOLVE_HANDLER id={:08X} producer={:08X} target={:08X} "
                "obj={:08X} msg={:08X}",
                id, producer, target, obj, r4.u32);
  }

  if (!IsSdbmHandlerTracked(id)) {
    return;
  }

  auto& table = g_diag_counters.sdbm_handler_table;
  for (size_t i = 0; i < table.size(); ++i) {
    auto& slot = table[i];
    uint32_t slot_id = slot.id.load(std::memory_order_relaxed);
    if (slot_id == id &&
        slot.producer.load(std::memory_order_relaxed) == producer &&
        slot.target.load(std::memory_order_relaxed) == target &&
        slot.obj.load(std::memory_order_relaxed) == obj) {
      slot.count.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    if (slot_id == UINT32_MAX) {
      uint32_t expected = UINT32_MAX;
      if (slot.id.compare_exchange_strong(expected, id, std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
        slot.producer.store(producer, std::memory_order_relaxed);
        slot.target.store(target, std::memory_order_relaxed);
        slot.obj.store(obj, std::memory_order_relaxed);
        slot.count.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      
      slot_id = slot.id.load(std::memory_order_relaxed);
      if (slot_id == id &&
          slot.producer.load(std::memory_order_relaxed) == producer &&
          slot.target.load(std::memory_order_relaxed) == target &&
          slot.obj.load(std::memory_order_relaxed) == obj) {
        slot.count.fetch_add(1, std::memory_order_relaxed);
        return;
      }
    }
  }
  g_diag_counters.sdbm_handler_overflow_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagBaseHzWriter(rex::ppc::Register& f13) {
  float original = static_cast<float>(f13.f64);
  g_diag_counters.base_hz_original.store(original, std::memory_order_relaxed);
  double override = rex::cvar::Query<double>("engine_base_hz_override");
  float applied = original;
  if (override > 0.0) {
    applied = static_cast<float>(override);
    f13.f64 = applied;
  }
  g_diag_counters.base_hz_applied.store(applied, std::memory_order_relaxed);
  g_diag_counters.base_hz_writer_hook_count.fetch_add(1, std::memory_order_relaxed);
}

inline void DiagBaseHzReader(rex::ppc::Register& f0) {
  g_diag_counters.base_hz_reader_value.store(static_cast<float>(f0.f64),
                                              std::memory_order_relaxed);
  g_diag_counters.base_hz_reader_hook_count.fetch_add(1, std::memory_order_relaxed);
}

inline double GetTimingHz() {
  double hz = rex::cvar::Query<double>("engine_base_hz_override");
  if (std::isfinite(hz) && hz > 0.0) {
    return hz;
  }
  hz = REXCVAR_QUERY(double, target_fps);
  if (std::isfinite(hz) && hz > 0.0) {
    return hz;
  }
  return 60.0;
}

inline double GetCatchingSoulsHz() {
  const double hz = GetTimingHz();
  if (rex::cvar::Query<bool>("catching_souls_timing_fix") &&
      std::isfinite(hz) && hz > 60.0) {
    return 60.0;
  }
  return hz;
}

inline void DiagUiTimingReader(rex::ppc::Register& value, size_t site) {
  const float original = static_cast<float>(value.f64);
  float applied = original;
  if (rex::cvar::Query<bool>("ui_timing_fix") &&
      rex::cvar::Query<double>("engine_base_hz_override") > 0.0) {
    
    applied = static_cast<float>(GetTimingHz());
    value.f64 = applied;
  }
  g_diag_counters.ui_fix_site_original[site].store(original, std::memory_order_relaxed);
  g_diag_counters.ui_fix_site_applied[site].store(applied, std::memory_order_relaxed);
  g_diag_counters.ui_fix_site_counts[site].fetch_add(1, std::memory_order_relaxed);
}

inline void DiagUiHz8243F470(rex::ppc::Register& f0) { DiagUiTimingReader(f0, 0); }
inline void DiagUiHz8245FE10(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 1); }
inline void DiagUiHz82478238(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 2); }
inline void DiagUiHz82496118(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 3); }
inline void DiagUiHz827B55D0(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 4); }
inline void DiagUiHz827B2CD0(rex::ppc::Register& f0) { DiagUiTimingReader(f0, 5); }
inline void DiagUiHz827B2CF0(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 6); }
inline void DiagUiHz827B51EC(rex::ppc::Register& f13) { DiagUiTimingReader(f13, 7); }

inline thread_local double g_catching_souls_paused_tick_accumulator = 0.0;
inline bool CatchingSoulsTimingFix(rex::ppc::Register& r11) {
  constexpr uint32_t kPausedTickHash = 0x177B48A3;
  if (r11.u32 != kPausedTickHash) {
    return false;
  }
  const double override_hz = rex::cvar::Query<double>("engine_base_hz_override");
  double factor = 1.0;
  if (rex::cvar::Query<bool>("catching_souls_timing_fix") &&
      std::isfinite(override_hz) && override_hz > 60.0) {
    factor = 60.0 / std::clamp(override_hz, 60.0, 240.0);
  }
  g_diag_counters.catching_souls_factor.store(static_cast<float>(factor),
                                               std::memory_order_relaxed);
  g_diag_counters.catching_souls_original_increment.store(
      static_cast<float>(override_hz), std::memory_order_relaxed);
  g_diag_counters.catching_souls_hit_count.fetch_add(1, std::memory_order_relaxed);
  if (factor >= 1.0) {
    g_catching_souls_paused_tick_accumulator = 0.0;
    return false;
  }
  g_catching_souls_paused_tick_accumulator += factor;
  if (g_catching_souls_paused_tick_accumulator >= 1.0) {
    g_catching_souls_paused_tick_accumulator -= 1.0;
    g_diag_counters.catching_souls_scaled_increment.store(1.0f,
                                                           std::memory_order_relaxed);
    return false;
  }
  g_diag_counters.catching_souls_scaled_increment.store(0.0f,
                                                         std::memory_order_relaxed);
  return true;
}

inline void DiagModeByteLoad(rex::ppc::Register& r11, rex::ppc::Register& r27) {
  const uint32_t value = r11.u32 & 0xFF;
  const uint32_t prev = g_diag_counters.mode_byte_value.load(std::memory_order_relaxed);
  g_diag_counters.mode_byte_value.store(value, std::memory_order_relaxed);
  g_diag_counters.mode_byte_object.store(r27.u32, std::memory_order_relaxed);
  if (prev != value) {
    g_diag_counters.mode_byte_transitions.fetch_add(1, std::memory_order_relaxed);
  }
}

inline double ScaleTimeStep(double value, double hz, double base_hz = 60.0) {
  if (hz <= 0.0) {
    return value;
  }
  
  if (std::abs(value * hz - 1.0) <= 0.05) {
    return value;
  }
  if (base_hz <= 0.0) {
    if (std::abs(value * 60.0 - 1.0) <= 0.05) {
      base_hz = 60.0;
    } else if (std::abs(value * 30.0 - 1.0) <= 0.05) {
      base_hz = 30.0;
    } else if (std::abs(value * 59.94 - 1.0) <= 0.05) {
      base_hz = 59.94;
    } else {
      base_hz = 60.0;
    }
  }
  if (hz <= base_hz || std::abs(value) >= 0.1) {
    return value;
  }
  return value * (base_hz / hz);
}

inline double ScaleFrameStep(double value, double hz, double base_hz = 30.0) {
  if (hz <= 0.0 || !std::isfinite(hz) || !std::isfinite(value)) {
    return value;
  }
  if (std::abs(value * hz - 1.0) <= 0.05) {
    return value;
  }
  if (base_hz <= 0.0) {
    if (std::abs(value * 60.0 - 1.0) <= 0.05) {
      base_hz = 60.0;
    } else if (std::abs(value * 30.0 - 1.0) <= 0.05) {
      base_hz = 30.0;
    } else if (std::abs(value * 59.94 - 1.0) <= 0.05) {
      base_hz = 59.94;
    } else {
      base_hz = 60.0;
    }
  }
  if (std::abs(value) >= 10.0 || hz <= base_hz) {
    return value;
  }
  return value * (base_hz / hz);
}

inline void DiagOneThirtyLoadA(rex::ppc::Register& f8, rex::ppc::Register& r5) {
  if (rex::cvar::Query<bool>("ui_timing_fix")) {
    f8.f64 = ScaleTimeStep(f8.f64, GetTimingHz(), 30.0);
  }
  g_diag_counters.onethirty_a_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.onethirty_a_value.store(static_cast<float>(f8.f64), std::memory_order_relaxed);
  g_diag_counters.onethirty_a_object.store(r5.u32, std::memory_order_relaxed);
}

inline void DiagOneThirtyLoadB(rex::ppc::Register& f9, rex::ppc::Register& r6) {
  if (rex::cvar::Query<bool>("ui_timing_fix")) {
    f9.f64 = ScaleTimeStep(f9.f64, GetTimingHz(), 30.0);
  }
  g_diag_counters.onethirty_b_count.fetch_add(1, std::memory_order_relaxed);
  g_diag_counters.onethirty_b_value.store(static_cast<float>(f9.f64), std::memory_order_relaxed);
  g_diag_counters.onethirty_b_object.store(r6.u32, std::memory_order_relaxed);
}

inline void DiagUiStep8243F278(rex::ppc::Register& f0) {
  if (rex::cvar::Query<bool>("ui_timing_fix")) {
    f0.f64 = ScaleFrameStep(f0.f64, GetTimingHz(), 0.0);
  }
}

inline void DiagUiStep8243F2C0(rex::ppc::Register& f12) {
  if (rex::cvar::Query<bool>("ui_timing_fix")) {
    f12.f64 = ScaleFrameStep(f12.f64, GetTimingHz(), 0.0);
  }
}

inline std::atomic<uint32_t> g_absolve_object{0};
inline std::atomic<uint32_t> g_absolve_state_object{0};

inline void DiagAbsolveSingleton(rex::ppc::Register& ptr) {
  const uint32_t previous = g_absolve_object.load(std::memory_order_relaxed);
  const uint32_t current = ptr.u32;
  g_absolve_object.store(current, std::memory_order_relaxed);
  if (current != 0 && current != previous) {
    REXLOG_INFO("DIAG_ABSOLVE_SINGLETON pointer={:08X}", current);
  }
}

inline void DiagAbsolveStateWrite(rex::ppc::Register& value,
                                  rex::ppc::Register& object,
                                  uint32_t expected_state) {
  const uint32_t obj = object.u32;
  const uint32_t state = value.u32;
  const uint32_t singleton = g_absolve_object.load(std::memory_order_relaxed);
  REXLOG_INFO("DIAG_ABSOLVE_STATE candidate singleton={:08X} object={:08X} state={} "
              "expected={}",
              singleton, obj, state, expected_state);
  if (obj == singleton) {
    g_absolve_state_object.store(obj, std::memory_order_relaxed);
  }
}

inline void DiagAbsolveStopStateWrite(rex::ppc::Register& r11,
                                      rex::ppc::Register& r28) {
  DiagAbsolveStateWrite(r11, r28, 3);
}

inline void DiagAbsolveRollStateWrite(rex::ppc::Register& r10,
                                      rex::ppc::Register& r31) {
  DiagAbsolveStateWrite(r10, r31, 2);
}

inline void DiagAbsolveTimerIncrement(rex::ppc::Register& f0) {
  const double original = f0.f64;
  const double hz = GetCatchingSoulsHz();
  f0.f64 = ScaleFrameStep(original, hz, 0.0);
  g_diag_counters.absolve_timer_last_original.store(
      static_cast<float>(original), std::memory_order_relaxed);
  g_diag_counters.absolve_timer_last_scaled.store(
      static_cast<float>(f0.f64), std::memory_order_relaxed);
  g_diag_counters.absolve_timer_increment_calls.fetch_add(
      1, std::memory_order_relaxed);
}

inline void AbsolvePromptTimerIncrement(rex::ppc::Register& f0) {
  const double original = f0.f64;
  
  g_diag_counters.absolve_prompt_timer_last_original.store(
      static_cast<float>(original), std::memory_order_relaxed);
  g_diag_counters.absolve_prompt_timer_last_scaled.store(
      static_cast<float>(f0.f64), std::memory_order_relaxed);
  g_diag_counters.absolve_prompt_timer_increment_calls.fetch_add(
      1, std::memory_order_relaxed);
}

inline void AbsolveRollTimeStep(rex::ppc::Register& f0) {
  const double original = f0.f64;
  
  f0.f64 = ScaleFrameStep(original, GetTimingHz(), 0.0);
  g_diag_counters.absolve_roll_step_last_original.store(
      static_cast<float>(original), std::memory_order_relaxed);
  g_diag_counters.absolve_roll_step_last_scaled.store(
      static_cast<float>(f0.f64), std::memory_order_relaxed);
  g_diag_counters.absolve_roll_step_calls.fetch_add(
      1, std::memory_order_relaxed);
}

struct Throttle6eState {
  uint8_t last = 0;
  double acc = 0.0;
};
inline thread_local std::unordered_map<uint32_t, Throttle6eState>
    g_absolve_6e_state;

inline void Absolve6eThrottle(rex::ppc::Register& r11,
                              rex::ppc::Register& r29) {
  const uint32_t obj = r29.u32;
  const uint8_t new6e = static_cast<uint8_t>(r11.u32);
  auto it = g_absolve_6e_state.find(obj);
  if (it == g_absolve_6e_state.end() || new6e < it->second.last) {
    g_absolve_6e_state[obj] = {new6e, 0.0};
    return;
  }
  const int delta = static_cast<int>(new6e) - it->second.last;
  if (delta > 0) {
    it->second.acc += delta * (60.0 / GetTimingHz());
    int advance = static_cast<int>(it->second.acc);
    it->second.acc -= advance;
    uint8_t result = it->second.last + advance;
    if (result > new6e) result = new6e;
    it->second.last = result;
    r11.u32 = result;
  } else {
    it->second.last = new6e;
  }
}

inline thread_local double g_absolve_table_counter = 0.0;

inline void AbsolveTableRamp(rex::ppc::Register& r8) {
  g_absolve_table_counter += 60.0 / GetTimingHz();
  const uint32_t idx = static_cast<uint32_t>(g_absolve_table_counter) & 0xFF;
  r8.u32 = idx << 2;
}

inline thread_local double g_absolve_roll_table_counter = 0.0;

inline void AbsolveRollRamp(rex::ppc::Register& r7) {
  g_absolve_roll_table_counter += 30.0 / GetTimingHz();
  const uint32_t idx = static_cast<uint32_t>(g_absolve_roll_table_counter) & 0xFF;
  r7.u32 = idx << 2;
}

inline void DiagAbsolveUpdateVTable(rex::ppc::Register& r11,
                                    rex::ppc::Register& r3) {
  g_diag_counters.absolve_update_vtable_calls.fetch_add(
      1, std::memory_order_relaxed);
}

inline thread_local uint64_t g_absolve_timer_sample_count = 0;

inline thread_local std::unordered_map<uint32_t, std::pair<double, double>>
    g_absolve_prompt_prev;

inline void DiagAbsolvePromptTimer(rex::ppc::Register& f1,
                                   rex::ppc::Register& f0,
                                   rex::ppc::Register& r3,
                                   rex::ppc::Register& r30) {
  const double original = f1.f64;
  const uint64_t count = ++g_absolve_timer_sample_count;
  g_diag_counters.absolve_prompt_timer_calls.fetch_add(
      1, std::memory_order_relaxed);
  g_diag_counters.absolve_prompt_timer_last_value.store(
      static_cast<float>(f1.f64), std::memory_order_relaxed);
  g_diag_counters.absolve_prompt_timer_last_threshold.store(
      static_cast<float>(f0.f64), std::memory_order_relaxed);
  g_diag_counters.absolve_prompt_timer_last_object.store(
      r3.u32, std::memory_order_relaxed);

  const uint32_t singleton = g_absolve_object.load(std::memory_order_relaxed);
  if (count % 120 == 0) {
    REXLOG_INFO("DIAG_ABSOLVE_TIMER sample={} orig={:.9g} scaled={:.9g} "
                "threshold={:.9g} object={:08X} source={:08X} singleton={:08X}",
                count, original, f1.f64, f0.f64, r3.u32, r30.u32, singleton);
  }
}
