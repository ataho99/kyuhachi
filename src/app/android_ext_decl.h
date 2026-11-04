#pragma once
/* android_ext_decl.h -- t137 (지시 #490): 안드로이드 전용 확장의 선언. 윈도에서는 전부 빈 함수 (동작 불변). */
#include <string>
#if defined(__ANDROID__)
static volatile unsigned g_kha_hold = 0; static volatile int g_kha_glidle = 0;   /* t137: gl 스레드 쉼 (복귀 암전 수정) */
static void kha_cfg_load(void);
static void kha_cfg_save(void);
static void kha_lib_init(void);
static void kha_perm_ask_once(void);
static int  kha_scan_extra(const char * why);
static void kha_lib_add(const std::string & d);
static void kha_lib_clear(void);
static std::string kha_font_lookup(const std::string & name);
static std::string kha_mixer_conf(void);
static std::string kha_mixer_patch(std::string cc);
static void kha_ld_diag(const char * tag);
static void kha_ld_tick(unsigned now);
static void kha_ev_watch(const SDL_Event * e);
#else
static const unsigned g_kha_hold = 0; static int g_kha_glidle = 0;
static inline void kha_cfg_load(void) {}
static inline void kha_cfg_save(void) {}
static inline void kha_lib_init(void) {}
static inline void kha_perm_ask_once(void) {}
static inline int  kha_scan_extra(const char *) { return 0; }
static inline void kha_lib_add(const std::string &) {}
static inline void kha_lib_clear(void) {}
static inline std::string kha_font_lookup(const std::string &) { return std::string(); }
static inline std::string kha_mixer_conf(void) { return std::string(); }
static inline std::string kha_mixer_patch(std::string cc) { return cc; }
static inline void kha_ld_diag(const char *) {}
static inline void kha_ld_tick(unsigned) {}
template <class E> static inline void kha_ev_watch(const E *) {}
#endif
#ifdef __ANDROID__
static std::string kha_krf(const std::string & f) { return f.empty() ? std::string("kr_shinsun") : f; }
#else
static std::string kha_krf(const std::string & f) { return f; }
#endif
static volatile int g_kha_rl = 0; static volatile int g_kha_was = 0;
