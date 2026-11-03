/* gensei_harness_t49a.h -- 시험 하니스 전용 코드(제품 동작과 무관): --inject 를 메뉴 루프에서도 돌리기 · --helplog 조작 설명 폭 로그. 호스트 소스에 include 된다. 별도 커밋·표기(#842·#847). */
#pragma once
/* 하니스: --inject 줄을 메뉴 루프에서도 돌린다(시계 = 게임이 이미 부팅됐으면 게임 시계, 아니면 프로세스 시계) */
static void gh_inj_poll_menu(void) {
    if (g_next_inj >= g_inj.size()) return; static unsigned t0 = SDL_GetTicks(); unsigned now = SDL_GetTicks(); unsigned since = g_boot_tick ? now - g_boot_tick + g_inj_base : (g_inj_stamp ? g_inj_base + (now - g_inj_stamp) : now - t0);   /* t51a: 창 안 전환을 건너도 시험 시계가 이어지게(전환 전 g_inj_base=0·g_inj_stamp=0 이면 예전과 같다) */
    while (g_next_inj < g_inj.size() && since >= g_inj[g_next_inj].at) { gh_inj_run(g_inj[g_next_inj], since); g_next_inj++; }
}
static void gh_help_logfn(const std::string & t, int pw, int kw, int cw, bool two) { static std::set<std::string> seen; if (!seen.insert(t).second) return; gh_log("HELPW pad=%d kbd=%d cell=%d two=%d text=%s", pw, kw, cw, two ? 1 : 0, t.c_str()); }
