/* app/state.h -- S9d: 옛 g_ 전역 선언 전부를 한 구조체로 모음. 멤버 이름 = 옛 이름에서 g_ 뗀 것, 기본값·volatile 은 그대로. 인스턴스는 static 1개(gApp). */
#include "app/state_types.h"
struct AppState {
    /* ---- 옛 위치: app/boot.h ---- */
    int disk_ix[4] = { -1, -1, -1, -1 };   /* 드라이브 -> g_disks 위치 (교체 때 같은 자리를 갱신) */
    /* ---- 옛 위치: app/crash_inwin.h ---- */
    volatile bool eexit_seen = false;
    int traceexc = 0;
    int traceexc_n = 0;   /* t37cd: --traceexc — 잡힌 C++ 예외(bad_alloc 등)의 던진 자리를 로그로 (EMS 가 왜 죽는지 찾는 용도) */
    /* ---- 옛 위치: app/frame.h ---- */
    std::vector<FtThumb> ft_thumbs;
    FILE *ft_csv = NULL;
    int ft_slot = 0;
    unsigned ft_frames = 0;
    unsigned ft_rawdbx = 0;
    unsigned ft_shown = 0;
    unsigned ft_load = 0;
    unsigned ft_first_nb = 0xFFFFFFFFu;
    volatile bool scr_req = false;
    std::vector<unsigned int> scrshots_ms;
    int dg_n[2] = { 0, 0 };
    int dg_m[2] = { 0, 0 };
    /* ---- 옛 위치: app/kbd_event.h ---- */
    unsigned kbsent_total = 0;
    unsigned kbsent_text = 0;   /* t48b (#629 3): kbov 가 코어로 보낸 키 수 · 그중 텍스트 입력 화면(S_OSK) 중에 보낸 수 */
    bool ob_held = false;
    bool ob_long = false;
    unsigned ob_t0 = 0;   /* t48b (#629 2): 패드 ○ 길게 누르기 */
    int kb_prevcur = -1;
    unsigned kb_cons = 0;
    unsigned kb_pass = 0;
    unsigned kbt_show_ms = 0;
    unsigned kbt_hide_ms = 0;
    unsigned kbt_click_ms = 0;
    std::vector<std::string> kbt_seq;
    /* ---- 옛 위치: app/menu_boot.h ---- */
    SDL_AudioDeviceID beep_dev = 0;
    std::vector<std::string> argv_saved;
    bool id_given = false;
    bool nostage = false;
    std::string stage_entry;
    std::string stage_next;
    bool stage_go = false;
    /* ---- 옛 위치: app/opts_log.h ---- */
    FILE *log = NULL;
    unsigned long long log_bytes = 0;
    unsigned int log_t0 = 0;
    std::string errmsg;   /* t48a (#614 ④): 사용자 화면에 보일 사람 말 오류 문구(비면 일반 문구) */
    std::string imgdir = ".";
    std::string outdir = ".";
    bool imgdir_given = false;
    bool headless = false;
    bool gltime = false;   /* t42(#313): --gltime — 렌더 스레드 그리기 시간(glFinish 기준) 3초 평균을 로그 한 줄로 */
    bool hidewin = false;   /* t41b (#308): 시험 전용 — 보이지 않는 GL 창(SDL_WINDOW_HIDDEN)으로 실행, 슨상 화면에 창이 뜨지 않음 */
    unsigned int run_ms = 0;
    std::vector<unsigned int> shots_ms;
    bool vol_forced = false;
    int setvol_b = -1;
    int setvol_s = -1;
    unsigned int rms_from = 0;
    bool rms_started = false;   /* t35 시험용 */
    GProfile prof_;
    unsigned int save_ms = 0;
    unsigned int load_ms = 0;
    std::string state_path;
    std::string note;
    uint32_t note_until = 0;   /* t43h: 메뉴 안내 한 줄(3초) */
    std::string savedir = "saves";   /* r011: 슬롯 저장 폴더(저장/<게임>/slot01.g98s) */
    bool autokeys = false;   /* r011: 시험용 자동 키 입력(--autokeys). 실기 실행에서는 꺼짐 */
    size_t next_bk = 0;   /* t36cd: bootkeys 다음 순번(--autokeys 와 무관하게 항상 동작) */
    bool bk_osd_on = false;   /* --bkosd 1 (시험 실행 △ 이 붙임): 단계 OSD 줄 표시 */
    bool bk_paused = false;
    bool bk_started = false;
    unsigned bk_t = 0;
    unsigned bk_last = 0;
    unsigned bk_done_at = 0;
    unsigned bkp_from = 0;
    unsigned bkp_to = 0;
    bool bkp_a = false;
    bool bkp_b = false;   /* --bkpause 시작ms,끝ms (시험용) */
    std::string bk_osd;   /* 비면 줄 없음. 주 스레드가 쓰고 오버레이 합성(같은 스레드)이 읽는다 */
    bool dosfd = false;   /* t43: FD 가 스스로 부팅 못 함(DOS 시스템 없음) → 내장 DOS 에 A:/B: 장착 */
    bool dosmode = false;   /* t36cd: CD 슬롯 → 내장 DOS 로 부팅(디스크가 없고 cd= 가 있을 때) */
    std::string entry_cli;   /* t36cd: --entry <이름> */
    bool dumpcode = false;
    bool boot400 = false;
    bool bda = false;
    bool mirq12 = false;   /* --mouseirq12: old r020 (MOUSE_IRQ stayed at the AT default 12) */
    std::string flaglog;
    unsigned keyhold = 80;
    bool keytrace = false;   /* t29 (#266): --keyhold <ms> 자동키 누름 길이 · --keytrace BDA 키 버퍼(0x502~)·PIC 마스크 로그 */
    bool int18trace = false;   /* t31 (#267): --int18trace  INT 18h 콜백을 감싸 AX(AH=기능,AL=그룹)·호출 위치를 2초마다 요약 */
    CallBack_Handler orig18 = NULL;
    std::map<unsigned, unsigned> i18_ax;
    std::map<unsigned, unsigned> i18_caller;
    unsigned i18_last = 0;
    unsigned i18_n = 0;
    unsigned i18_non04 = 0;
    unsigned segdump_ms = 0;
    unsigned hist_from = 0;
    unsigned hist_to = 0;   /* t30 (#267): --segdump <ms> CS/DS/ES/SS 64KB 덤프 · --hist <from>,<to> CS:EIP 표본 히스토그램 */
    bool di0 = true;   /* DI=0 (old r020); FFFE hypothesis rejected in t26 */
    bool ext14 = false;   /* --ext14: keep r020 behaviour (BDA 0401h = extended memory 14MB at boot) */
    bool ext0 = false;
    bool trace21 = false;
    int watch_off = -1;   /* r021 진단: --trace21 (IDT/IVT 핸들러 덤프·PIC 마스크 변화·마우스 레지스터 변화 로그), --watch <16진 오프셋> (DS:off 바이트 변화 로그) */
    std::vector<GMouse> amouse;
    size_t next_mouse = 0;
    bool mouse_game = false;
    bool relmouse = false;
    bool mouse_free = false;   /* r020: 시험용 자동 마우스(--automouse) · 이 작품이 마우스를 쓰는가 · SDL 상대 마우스 모드 상태 */
    bool mouseok_cli = false;
    int ems_req = 1;   /* t38cd: EMS 기본 켬(t37cd 시험에서 마도456 60초 정상) · --noems 로 끄기 */
    std::vector<GInj> inj;
    size_t next_inj = 0;
    std::string inject_file;
    SDL_Joystick *vj = NULL;
    int vj_idx = -1;
    std::string vj_map;
    /* ---- 옛 위치: app/res_prof.h ---- */
    std::string audiodev;   /* r011: --audiodev 이름(부분 일치) */
    bool listaudio = false;
    bool save_done = false;
    bool load_done = false;
    unsigned int ev_tick = 0;   /* SDL ticks at save/load event */
    std::vector<Pc98StateDisk> disks;
    FILE * disk_fh[4] = { NULL, NULL, NULL, NULL };   /* 1b: 드라이브별 열린 작업 사본 핸들 */
    std::string disk_wp[4], disk_nm[4];               /* 1b: 작업 사본 경로 · 이미지 이름 (세이브가 사본 압축본을 함께 담는다) */
    unsigned int boot_tick = 0;   /* SDL ticks at guest boot, 0 = not yet */
    bool quit = false;
    bool winsize_user = false;
    std::string uitest_dir;
    std::string logoshot;   /* t52d: --logoshot <png> 로고 한 장만 그리고 끝 */
    unsigned swap_ms = 0;
    int swap_drv = 0;
    int swap_idx = 0;
    bool swap_done = false;
    bool axistest = false;   /* 시험: --swaptest 초,드라이브,번호 / --axistest */
    unsigned bt_at = 0;
    unsigned bt_len = 0;
    int bt_x = 0;
    unsigned caps_at = 0;
    unsigned caps_len = 0;
    bool loading = false;
    bool noload = false;   /* 「Now Loading...」 덮개: 켠 때부터 첫 게임 화면까지 */
    unsigned pon_tick = 0;
    Bitu pon_pic = 0;   /* 전원 투입 시각(SDL ms · PIC ms) */
    unsigned dbx_raw_frames = 0;
    unsigned frames_seen = 0;
    unsigned load_end_wall = 0;
    unsigned load_end_game = 0;
    std::string ft_dir;
    unsigned ft_ms = 2000;   /* --frametest DIR : 처음 2초 프레임별 기록 */
    bool ft_done = false;
    int bsound_override = -1;   /* --bsound N 시험용 */
    std::string nkdb_path;
    std::string gamefolder_acp;   /* NK DB(gamedb_nk.tsv) · 게임 폴더(시스템 문자열) */
    nk::Db nk_;
    aw::ShaCache shacache;
    std::string addwork_cli;
    std::string gamefolder_cli;
    std::string report_dir;
    bool scanonly = false;
    ui::Ui ui_;
    int cur_game = 0;   /* g_ui.games 인덱스 */
    std::string osd;
    unsigned int osd_until = 0;
    bool builtin_res = false;   /* r019: 셰이더를 내장본으로 대신했는가 */
    bool builtin_res_font = false;
    bool boost_on = false;
    bool bosd_on = true;
    bool fps_on = false;   /* FPS / speed display (settings fpsdisp) */
    int boost_xv = 2;   /* 배속 상태 · 「배속 표시」 설정 · 표시 배율 */
    unsigned play_ms = 0;
    unsigned play_last = 0;   /* 플레이 시간(메뉴 시간 제외) */
    volatile float scanv = 7.0f;
    volatile float maskv = 0.15f;
    volatile float bleedv = 0.0f;
    volatile float glowv = 0.0f;
    volatile float nearv = 0.0f;   /* t48c: 빛 번짐 · 밑 그림 nearest(1) *//* crt 셰이더 uniform(렌더 스레드가 읽음) */
    std::string exedir;
    std::string cfgpath;
    std::string pst_id;
    std::deque<std::string> pst_pool;   /* 프로필마다 문자열을 새로 보관(포인터가 다음 호출에 덮이지 않게) */
    std::vector<prof::DbGame> db;
    bool db_loaded = false;
    bool noboot = false;
    bool noB = false;
    bool demo = false;
    int inwin_req = 0;   /* 0 없음 · 1 다음 단계 · 2 메인 메뉴로 */
    std::string inwin_target;   /* 1 일 때 다음 단계 항목 이름 */
    bool inwin_enabled = true;
    bool inwin_timeout = false;
    bool inwin_wait = false;
    bool inw_menu_pick = false;
    unsigned inwin_t0 = 0;
    unsigned inj_base = 0;
    unsigned inj_stamp = 0;
    int inwin_n = 0;
    int boot_n = 1;
    int inw_kind = 0;
    std::string inwin_testfail;   /* 시험용 --inwintest attach|noboot|throw (제품 동작 무관) */
    std::string pid;   /* 지금 작품 id(프로필 다시 읽기용) */
    std::string inw_sel_id;   /* 메뉴에서 고른 작품 id(폴백 재실행용) */
    bool menu_open = false;
    bool menu_req = false;
    bool menu43 = false;   /* t52f (#132 2항): 큐하치 자체 화면(시작 메뉴)이 열려 있는 동안 화면비는 항상 4:3. 작품별 화면비는 게임이 시작될 때만 적용 */
    int resume_slot = 0;   /* 1..8 = 게임 시작 뒤 이 슬롯을 불러옴 */
    /* ---- 옛 위치: app/state_io.h ---- */
    unsigned int next_key = 0;
    unsigned int key_release_at = 0;
    KBD_KEYS key_down = KBD_NONE;
    bool boost_hold = false;
    bool boost_tog = false;
    /* ---- 옛 위치: app/ui_env.h ---- */
    std::map<std::string, std::vector<std::string>> fdfiles;   /* 작품 id -> 디스크 교체 후보 파일(UiGame::fds 와 같은 순서) */
    std::map<std::string, std::string> vlabel;   /* 변형 폴더 id -> 목록에 보일 이름 */
    std::string uikeys_file;   /* t39: --uikeys <파일> (시험 전용) */
    bool titletest = false;
    bool titlelegacy = false;   /* t39: --titletest / --titlelegacy */
    bool helplog = false;   /* t49a: --helplog (조작 설명 폭 실측 로그) */
    int tb_last = -1;   /* -1 아직 안 함 · 0 밝게 · 1 어둡게 · -3 이 윈도우에서는 지원 안 됨 */
    unsigned tb_tick = 0;
    bool aw_dry = false;
    bool batest = false;
    /* ---- 옛 위치: app/uitest.h ---- */
    int cli_lang = -1;   /* t52g 시험 전용(--lang N): 설정의 언어 대신 0 한국어 · 1 English · 2 にほんご */
    bool menushot = false;   /* t52f 시험 전용(--menushot): 메뉴 창을 실제로 찍어 4:3 고정을 눈으로 본다 */
    /* ---- 옛 위치: app/video_sink.h ---- */
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    std::vector<uint32_t> fb;
    int fbw = 0;
    int fbh = 0;
    int cury = 0;
    unsigned long long frames = 0;
    unsigned long long starts = 0;
    unsigned long long aborts = 0;
    Bitu pic0 = 0;
    double fps = 0;
    volatile int fit = FIT_43;
    volatile int shader = SH_SHARP;
    volatile float curve = 0.0f;
#if defined(__ANDROID__)
    bool windowed = false;   /* t137: 안드로이드는 전체화면 기본 */
#else
    bool windowed = true;
#endif
    bool use_gl = false;
    int winw = 1280;
    int winh = 800;
    unsigned wm_expect_until = 0;
    int wm_ew = 0;
    int wm_eh = 0;   /* 우리가 정한 크기가 되돌아오는 이벤트 무시용 */
    bool wm_dirty = false;
    unsigned wm_last = 0;
    std::string shaderdir;
    SDL_GLContext glctx = NULL;
    SDL_Thread *glthr = NULL;
    volatile bool gl_stop = false;
    SDL_mutex *fb_mtx = NULL;
    std::vector<uint32_t> pub;
    std::vector<uint32_t> rbuf;
    int pub_w = 0;
    int pub_h = 0;
    bool pub_new = false;
    volatile int kbshader = 0;
    int cli_kbsh = -1;   /* --kbshader 0|1 이 설정보다 우선(시험용) */
    std::vector<uint32_t> ovl;
    std::vector<uint32_t> ovl_gl;
    std::vector<uint32_t> ovt_b;
    std::vector<uint32_t> ovt_w;
    int ovl_w = 0;
    int ovl_h = 0;
    bool ovl_has = false;
    bool ovl_new = false;
    volatile int shot_req = 0;
    char shot_path[1024];
    volatile int shot_crop = 0;
    int shot_w = 0;
    int shot_h = 0;   /* t41: 화면 설정(셰이더·곡면·비율) 적용된 스크린샷 — 화면 영역만 잘라 저장 */
    int scr_pend = 0;
    bool vsync = false;
    unsigned int glmx_ms = 0;
    bool glmx_done = false;
    GProg prog[SH_N];
    GProg prog_ov;
    unsigned int ov_tex = 0;
    unsigned int vbo = 0;
    unsigned int tex_gl = 0;
};
static AppState gApp;
#define GH_QUITSET() (gh_log("QUIT-SET %s:%d", __FILE__, __LINE__), gApp.quit = true)   /* t139: quit 세우는 곳 사유 로그(항상) */
