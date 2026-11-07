/* app/main_core.h -- S9c: main() steps J-L: config, device init, run loop. Bodies moved unchanged from gensei_host.cpp main (as of t54u). Included in place before main; single translation unit. */
static void gh_main_conf() {
        /* the one section that lives in sdlmain.cpp: only what the subset reads */
        { Section_prop * s = control->AddSection_prop("sdl", &Gh_Null_Init); s->Add_string("output", Property::Changeable::Always, "surface"); }
        DOSBOX_SetupConfigSections();
        {   /* r020 (#248): 기본 한글 패치 글꼴 = res\fonts\kyuhachi_font.bmp (t52g: 옛 이름 pc98player_font.bmp · 옛 위치도 허용) */
            std::string cc = g_conf_common; size_t at = cc.find("@FONT@");
            if (at != std::string::npos) cc.replace(at, 6, gh_default_font_path()); cc = kha_mixer_patch(cc);
            control->ParseConfigString("gensei:common", cc.c_str());
        }
        {   /* 작품별 cycles / 글꼴 / extra (프로필 > 공통) */
            std::string c;
            if (!gApp.prof_.cycles.empty()) { bool num = true; for (char ch : gApp.prof_.cycles) if (ch < '0' || ch > '9') num = false; c += std::string("[cpu]\ncycles=") + (num ? "fixed " : "") + gApp.prof_.cycles + "\n"; }
            else { int ci = gApp.ui_.effective(gApp.cur_game).cpuspd;
                if (ci < 0 || ci > 3) ci = 0;
                static const char * const CP[4] = { "fixed 3000",
                    "fixed 12000", "fixed 24000", "max" };
                c += std::string("[cpu]\ncycles=") + CP[ci] + "\n"; }
            {   /* r020 (#247정정): 글꼴은 이름(kr_shinsun · kr_gulim · kr_dotum · jp_default → res\fonts\<이름>.bmp)으로 지정한다. 옛 파일 경로 지정도 읽는다. 보조 zip 은 더 뒤지지 않는다 */
                auto ex = [](const std::string & p) { return plat::exists(p.c_str()); };
                std::string f0 = gApp.prof_.font; bool soft = false;
                { auto fk = gApp.prof_.kv.find("font_kind");   /* t39 (#290/#291): 항목의 font= > 목록 기본 — 원판(jp)은 jp 글꼴, 한글판은 번들 한글(또는 설정에서 고른 같은 묶음 글꼴) */
                  if (f0.empty() && fk != gApp.prof_.kv.end() && fk->second == "jp") { f0 = gApp.ui_.font_jp.empty() ? std::string("jp_default") : gApp.ui_.font_jp; soft = true; }
                  else if (f0.empty() && fk != gApp.prof_.kv.end() && fk->second == "kr" && !gApp.ui_.font_kr.empty()) { f0 = gApp.ui_.font_kr; soft = true; } }
                if (!f0.empty()) {
                    bool isname = f0.find_first_of("\\/:.") == std::string::npos; std::string fp;
                    if (isname) { std::string p = gh_res_font_path(f0); if (ex(p)) { fp = p; gh_log("글꼴 이름 %s -> %s", f0.c_str(), p.c_str()); } }
                    else {
                        bool abs = f0.size() > 1 && (f0[1] == ':' || f0[0] == '\\');
                        std::vector<std::string> cand; if (abs) cand.push_back(f0); else {
                            cand.push_back(gApp.prof_.imgdir + "\\" + f0); auto sd = gApp.prof_.kv.find("srcdir"); if (sd != gApp.prof_.kv.end() && !sd->second.empty()) cand.push_back(sd->second + "\\" + f0);
                            cand.push_back(gApp.exedir + "\\games\\" + std::string(gApp.prof_.id ? gApp.prof_.id : "") + "\\" + f0); cand.push_back(gApp.exedir + "\\" + f0); cand.push_back(f0); }
                        for (auto & x : cand) if (ex(x)) { fp = x; break; }
                    }
                    if (fp.empty()) {
                        if (soft) { gh_log("글꼴 종류=jp: res\\fonts\\jp_default.bmp 없음 -> 기본(한글 패치) 글꼴로 대신 (경고 없음)"); }
                        else {
                        std::string bf = gh_default_font_path();
                        gh_log("WARN: 글꼴 '%s' 을(를) 찾지 못함 -> 번들 글꼴 사용 (%s)  (이름 글꼴은 run\\res\\fonts\\<이름>.bmp)", f0.c_str(), bf.c_str());
                        fp = bf; gApp.osd = phr_("글꼴 '") + f0 + phr_("' 을(를) 찾지 못해 기본 글꼴로 실행합니다 (res\\fonts)"); gApp.osd_until = SDL_GetTicks() + 6000;
                        }
                    }
                    if (!fp.empty()) c += "[pc98]\npc-98 anex86 font=" + fp + "\n";
                } else gh_log("글꼴 = 기본 (번들 한글 패치 글꼴 %s)", gh_default_font_path().c_str());
            }
            if (!gApp.prof_.extra.empty()) { std::string e = gApp.prof_.extra; for (auto & ch : e) if (ch == '|') ch = '\n'; c += e + "\n"; }
            if (!c.empty()) { gh_log("profile conf:\n%s", c.c_str()); control->ParseConfigString("gensei:profile", c.c_str()); }
        }
        gh_log("config parsed");
        {   /* r020 (#247정정): 리듬 WAV(2608_*.wav) 는 run\res\rhythm\ 한 곳에서만 읽는다(DOSBox-X 가 현재 폴더 기준으로 읽으므로 그 폴더를 현재 폴더로). sound.rom·bios.rom 은 쓰지 않는다 */
            std::string snd = gApp.prof_.kv.count("sound") ? gApp.prof_.kv["sound"] : std::string();
            if (snd.find("rhythm") != std::string::npos || snd.find("2608") != std::string::npos || snd.find("86") != std::string::npos) {
                std::string rd = gApp.exedir + "\\res\\rhythm"; int n = 0; static const char * const rn[6] = { "bd", "sd", "top", "hh", "tom", "rim" };
                for (int i = 0; i < 6; i++) if (plat::exists((rd + "\\2608_" + rn[i] + ".wav").c_str())) n++;
                if (n > 0) { plat::set_cwd(rd); gh_log("리듬 WAV %d/6 개: %s (현재 폴더로 지정)%s", n, rd.c_str(), n < 6 ? " — 일부 없음" : ""); }
                else { gh_log("리듬음 파일 없음 — 넣는 곳: %s (sound=%s)", rd.c_str(), snd.c_str()); if (gApp.osd.empty()) { gApp.osd = phr_("리듬음 파일 없음 — 넣는 곳: run\\res\\rhythm\\"); gApp.osd_until = SDL_GetTicks() + 10000; } }
            }
        }

}

static void gh_main_devinit() {
        DOSBOX_InitTickLoop();
        DOSBOX_RealInit();
        gh_log("RealInit done, machine=%d", (int)machine);

#define GH_INIT(f) do { gh_log("init " #f); f; } while (0)
        { /* sdlmain.cpp 가 만들던 메뉴항목(get_item 으로 찾기만 하는 것)을 미리 할당 — 목록은 빌드 때 소스에서 뽑는다 */
            static const char * const pre_names[] = {
#include "gensei_menu_names.inc"
                0 };
            for (int i = 0; pre_names[i]; i++) if (!mainMenu.item_exists(pre_names[i])) mainMenu.alloc_item(DOSBoxMenu::item_type_id, pre_names[i]);
#ifdef GH_DOSKERNEL
            { /* t36cd: DOS_EnableDriveMenu 가 조합해서 찾는 드라이브 메뉴 이름(Drive<X>, drive_<X>_*) 미리 할당 */
                static const char * const dsfx[] = { "_mountauto","_mounthd","_mountcd","_mountfd","_mountfro","_mountarc","_mountimg","_mountimgs","_mountiro","_unmount","_swap","_rescan","_info","_boot","_bootimg","_saveimg", 0 };
                for (char dc = 'A'; dc <= 'Z'; dc++) {
                    std::string nm = std::string("Drive") + dc;
                    if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    for (int k = 0; dsfx[k]; k++) {
                        nm = std::string("drive_") + dc + dsfx[k];
                        if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    }
                }
                for (int ic = 1; ic <= 8; ic++) for (int ms = 0; ms < 2; ms++) { /* ide.cpp DOS_EnableDriveIDEMenu: IDEDrive<n><m|s> */
                    char bn[32]; snprintf(bn, sizeof bn, "IDEDrive%d%c", ic, ms ? 's' : 'm');
                    std::string nm = bn;
                    if (!mainMenu.item_exists(nm)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, nm);
                    for (int k = 0; dsfx[k]; k++) {
                        std::string n2 = nm + dsfx[k];
                        if (!mainMenu.item_exists(n2)) mainMenu.alloc_item(DOSBoxMenu::item_type_id, n2);
                    }
                }
            }
#endif
        }
        GH_INIT(IO_Init());
        GH_INIT(HARDWARE_Init());
        GH_INIT(CPU_PreInit());
        GH_INIT(Init_AddressLimitAndGateMask());
        GH_INIT(Init_MemoryAccessArray());
        GH_INIT(Init_A20_Gate());
        GH_INIT(Init_PS2_Port_92h());
        GH_INIT(Init_RAM());
        GH_INIT(Init_DMA());
        GH_INIT(Init_PIC());
        GH_INIT(TIMER_Init());
        GH_INIT(PCIBUS_Init());
        GH_INIT(PAGING_Init());
        GH_INIT(CMOS_Init());
        GH_INIT(ROMBIOS_Init());
        GH_INIT(CALLBACK_Init());
        GH_INIT(Init_VGABIOS());
        GH_INIT(PROGRAMS_Init());
        GH_INIT(PCSPEAKER_Init());
        GH_INIT(MIXER_Init());
        GH_INIT(CPU_Init());
        GH_INIT(Weitek_Init());
        GH_INIT(FPU_Init());
        GH_INIT(VGA_Init());
        GH_INIT(ISAPNP_Cfg_Init());
        GH_INIT(FDC_Primary_Init());
        GH_INIT(KEYBOARD_Init());
        GH_INIT(PS1SOUND_Init());
        GH_INIT(DISNEY_Init());
        GH_INIT(GUS_Init());
        GH_INIT(IDE_Init());
        GH_INIT(IMFC_Init());
        GH_INIT(INNOVA_Init());
        GH_INIT(WSS_Init());
        GH_INIT(BIOS_Init());
        GH_INIT(INT10_Init());
        GH_INIT(SERIAL_Init());
        GH_INIT(DONGLE_Init());
        GH_INIT(PARALLEL_Init());
        GH_INIT(NE2K_Init());
        GH_INIT(MEM_A20_Enable(true));
        GH_INIT(Init_MemHandles());
        GH_INIT(AllocCallback2());
        GH_INIT(MSG_Init());
        if (gApp.dosmode) {   /* t36cd: 내장 DOS — sdlmain 의 초기화 목록 중 DOS·CD 몫 (마우스는 호스트 몫이라 제외) */
            void DOS_Init(); void XMS_Init(); void EMS_Init(); void DOS_KeyboardLayout_Init(); void CDROM_Image_Init(); void MSCDEX_Init(); void DRIVES_Init(); void AUTOEXEC_Init();
            control->ParseConfigString("gensei:dos0", gApp.ems_req ? "[dos]\nems=true\n" : "[dos]\nems=false\n");   /* PC-98 에서는 EMS 불필요 — bad_alloc 회피 */
            GH_INIT(DOS_Init()); GH_INIT(XMS_Init()); GH_INIT(EMS_Init()); GH_INIT(DOS_KeyboardLayout_Init()); GH_INIT(CDROM_Image_Init()); GH_INIT(MSCDEX_Init()); GH_INIT(DRIVES_Init()); GH_INIT(AUTOEXEC_Init());
            std::string ae = "[autoexec]\n";
            if (!gApp.prof_.cd.empty()) ae += "imgmount d \"" + gApp.prof_.cd + "\" -t iso\n";
            if (gApp.dosfd) {   /* t43: A: = 첫 디스크(+교체 목록), B: = 둘째 디스크. 실행 파일이 하나면 자동 실행, 여럿/없음이면 목록만 */
                auto qp = [&](const std::string & n) { return std::string("\"") + gh_workpath(n) + "\""; };   /* t43h: 내장 DOS 의 FD 장착도 작업 사본 */
                std::string d0 = gApp.prof_.disk[0] ? gApp.prof_.disk[0] : gApp.prof_.disk[1], d1 = (gApp.prof_.disk[0] && gApp.prof_.disk[1]) ? gApp.prof_.disk[1] : std::string();
                bool hddf = gApp.prof_.kv.count("doshdd") && gApp.prof_.kv["doshdd"] == "1"; const char L1 = hddf ? 'b' : 'a', L2 = hddf ? 'c' : 'b';   /* t43d: 설치 흐름 = HDD 가 A:, FD 는 B:·C: */
                std::string a = std::string("imgmount ") + L1 + " " + qp(d0); for (auto & sw : gApp.prof_.swap) if (sw != d0 && sw != d1) a += " " + qp(sw); ae += a + " -t floppy\n";
                if (!d1.empty()) ae += std::string("imgmount ") + L2 + " " + qp(d1) + " -t floppy\n";
                std::string dv = gApp.prof_.kv.count("dosdrv") ? gApp.prof_.kv["dosdrv"] : std::string("a"); if (dv != "a" && dv != "b") dv = "a";
                if (hddf) {   /* t43c: C: = 작품별 호스트 폴더 (savedir\<id>\hdd). 비어 있으면 설치기 실행, 채워졌으면 실행 파일 탐지 */
                    std::string sd1 = gApp.savedir, sd2 = gApp.savedir + "\\" + std::string(gApp.prof_.id ? gApp.prof_.id : "x"), hd = sd2 + "\\hdd";
                    plat::mkdir(sd1.c_str()); plat::mkdir(sd2.c_str()); plat::mkdir(hd.c_str());
                    ae += "mount a \"" + hd + "\"\n";
                    std::string run, dirp; int nrun = gh_scan_hdd(hd, run, dirp);
                    std::string idv = gApp.prof_.kv.count("dosinstdrv") ? gApp.prof_.kv["dosinstdrv"] : dv, inst = gApp.prof_.kv.count("dosinst") ? gApp.prof_.kv["dosinst"] : std::string(); if (!inst.empty() && gApp.prof_.kv.count("dosinstargs") && !gApp.prof_.kv["dosinstargs"].empty()) inst += " " + gApp.prof_.kv["dosinstargs"];
                    if (nrun == 0 && !inst.empty()) { std::string fl = idv == "b" ? "c" : "b"; size_t pp; while ((pp = inst.find("@F")) != std::string::npos) inst.replace(pp, 2, fl); while ((pp = inst.find("@H")) != std::string::npos) inst.replace(pp, 2, "a"); ae += fl + ":\n" + inst + "\na:\ndir /w\n"; gh_log("HDD 설치 흐름: A: = %s (비어 있음, HDD) → %s: %s 실행", hd.c_str(), fl.c_str(), inst.c_str()); }
                    else if (nrun == 0 && gApp.prof_.kv.count("dossfx") && !gApp.prof_.kv["dossfx"].empty()) {   /* t43p (#344-3): LHA 자기풀림 — A: = 빈 호스트 폴더에서 FD 의 SFX 를 실행해 푼다. 다음 실행부터는 푼 폴더에서 실행 파일 탐지 */
                        ae += "a:\n"; { bool ub_done = false; std::string cur, rs = gApp.prof_.kv["dossfx"]; for (char ch : rs + ",") { if (ch == ',') { if (!cur.empty()) { if (cur[0] == '#') { size_t bar = cur.find('|'); int ix = atoi(cur.c_str() + 1); std::string nm = bar == std::string::npos ? std::string() : cur.substr(bar + 1); if (!nm.empty() && ix >= 2 && (size_t)(ix - 2) < gApp.prof_.swap.size()) { if (!ub_done) { ae += "imgmount -u b\n"; ub_done = true; } ae += "imgmount -u c\n"; ae += std::string("imgmount c ") + qp(gApp.prof_.swap[ix - 2]) + " -t floppy\n"; ae += "c:\\" + nm + "\n"; ae += "a:\n"; } } else ae += cur + "\n"; } cur.clear(); } else cur += ch; } } ae += "dir /w\n";
                        gh_log("HDD 풀기 흐름(LHA 자기풀림): A: = %s (비어 있음, HDD) → %s", hd.c_str(), gApp.prof_.kv["dossfx"].c_str()); }
                    else { gApp.prof_.bootkeys.clear(); ae += "a:\n"; if (!dirp.empty()) ae += "cd \\" + dirp + "\n"; ae += (nrun == 1 ? run : std::string("dir /w")) + "\n"; gh_log("HDD 설치 끝난 폴더: 실행 파일 %d개 → %s", nrun, nrun == 1 ? run.c_str() : "목록"); }
                } else {
                std::string uexe = gApp.prof_.kv.count("dosexe") ? gApp.prof_.kv["dosexe"] : std::string(), uargs = gApp.prof_.kv.count("dosargs") ? gApp.prof_.kv["dosargs"] : std::string(); const std::string ea = uargs.empty() ? std::string() : " " + uargs;   /* 2b */
                if (!uexe.empty()) {   /* 2b: 사용자가 고른 실행 파일 (실행 방법의 「실행 파일」) — "*" = DOS 프롬프트 */
                    if (uexe == "*") { ae += dv + ":\n"; gh_log("exe: DOS prompt (user choice)"); }
                    else if (uexe.size() >= 3 && uexe[1] == (char)58) {
                        char L = (char)tolower((unsigned char)uexe[0]); std::string rel = uexe.substr(2), dn = (L == L1) ? d0 : d1, dp2 = dn.empty() ? std::string() : gh_workpath(dn); size_t sl = rel.find_last_of((char)92); std::string fdir = sl == std::string::npos ? std::string() : rel.substr(0, sl), fnm = sl == std::string::npos ? rel : rel.substr(sl + 1);
                        if (!dp2.empty() && prof::dos_exists(dp2, rel)) { ae += std::string(1, L) + ":\n"; if (!fdir.empty()) ae += "cd \\" + fdir + "\n"; ae += fnm + ea + "\n"; gh_log("exe: user choice %s", uexe.c_str()); }
                        else { ae += std::string(1, L) + ":\n"; ae += "echo Executable not found - pick one in the run method\n"; gApp.osd = phr_("실행 파일을 못 찾음 — 실행 방법에서 고르세요"); gApp.osd_until = SDL_GetTicks() + 8000u; gh_log("exe: user choice not found: %s", uexe.c_str()); }
                    }
                } else {
                ae += dv + ":\n"; if (gApp.prof_.kv.count("dossub") && !gApp.prof_.kv["dossub"].empty()) ae += "cd \\" + gApp.prof_.kv["dossub"] + "\n";   /* t43s R2 */
                std::vector<std::string> rl; { std::string cur, rs = gApp.prof_.kv.count("dosrun") ? gApp.prof_.kv["dosrun"] : std::string(); for (char ch : rs + ",") { if (ch == ',') { if (!cur.empty()) rl.push_back(cur); cur.clear(); } else cur += ch; } }
                { std::string pk = gApp.prof_.kv.count("dospick") ? gApp.prof_.kv["dospick"] : std::string(); gh_log("DOS 루트 실행 후보(%d): %s | 선택: %s", (int)rl.size(), gApp.prof_.kv.count("dosrunsz") ? gApp.prof_.kv["dosrunsz"].c_str() : "-", pk.empty() ? (rl.size() == 1 ? rl[0].c_str() : "(목록)") : pk.c_str());   /* t43n (#343) */
                  if (rl.size() == 1) ae += rl[0] + ea + "\n"; else if (!pk.empty()) ae += pk + ea + "\n"; else { ae += "dir /w\n"; if (gApp.prof_.kv.count("dosdata")) ae += "echo No executable on this disk (data disk? the main disk is needed)\n"; if (rl.empty()) { gApp.osd = phr_("실행 파일을 못 찾음 — 실행 방법에서 고르세요"); gApp.osd_until = SDL_GetTicks() + 8000u; } } }   /* t43s R5 */
                }
                }
            }
            { std::string ln; for (char ch : gApp.prof_.autoexec + "|") { if (ch == '|') { if (!ln.empty()) ae += ln + "\n"; ln.clear(); } else ln += ch; } }
            gh_log("autoexec(내장 DOS):\n%s", ae.c_str());
            control->ParseConfigString("gensei:dos", ae.c_str());
        }
        gh_log("devices initialised");

}

static void gh_main_run() {
        gh_stage_init();   /* t49a (#845·#847) */
        if (gApp.demo) { std::string dn = gApp.prof_.kv.count("demo") ? gApp.prof_.kv["demo"] : std::string(); if (dn.empty() || !gh_attach(0, dn.c_str())) { gh_finish(2); } gApp.prof_.bootletter = 'A'; gApp.autokeys = false; gh_log("DEMO boot: %s", dn.c_str()); }
        else if (!gApp.dosfd) for (int i = 0; i < 4; i++) if (gApp.prof_.disk[i] && !(gApp.noB && i == 1) && !gh_attach(i, gApp.prof_.disk[i])) { gh_finish(2); }

        if (!VM_PowerOn_gensei()) { gh_log("ERROR: power on failed"); gh_finish(3); }
        gh_log("powered on, running machine");
        gApp.pon_tick = SDL_GetTicks() | 1u; gApp.pon_pic = PIC_Ticks; gApp.loading = true;
        if (!gApp.ft_dir.empty() && !ov::g_font_ok) ov::font_load(gApp.exedir);
        for (;;) {   /* t51a: 창 안 전환이 요청되면 정리 · 디스크 교체 · 리셋 뒤 같은 창에서 RunMachine 재진입. 요청이 없으면 예전과 같이 한 번 돌고 끝 */
            try {
                DOSBOX_RunMachine();
            } catch (int x) {
                gh_log("RunMachine exit signal %d", x);
            }
            if (!gApp.inwin_req && !gApp.inwin_timeout) break;
            if (!gh_inwin_cycle()) break;
        }
        gh_finish(0);
}
