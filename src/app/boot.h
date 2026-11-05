/* app/boot.h -- S9a: image attach, disk reset, direct DOS kernel boot. Moved unchanged from gensei_host.cpp lines 1193-1399 (as of t54s). Included in place; single translation unit. */
/* ------------------------------------------------------------------ images + direct boot */
static FILE * gh_open_rw(const std::string & path, uint64_t * size, bool rw = true) {
    FILE * f = rw ? fopen(path.c_str(), "rb+") : NULL;
    if (!f) f = fopen(path.c_str(), "rb");
    if (!f) return NULL;
    fseeko64(f, 0, SEEK_END); *size = (uint64_t)ftello64(f); fseeko64(f, 0, SEEK_SET);
    return f;
}
/* t43h (#333): 원본 이미지는 절대 바뀌지 않는다 — 첫 실행 때 작품 저장폴더(저장폴더\<id>\disk\)에 작업 사본을 만들고 읽기·쓰기는 사본에만. 이후 실행은 사본 사용. 사본을 못 만들면 원본을 읽기 전용으로 연다. */
static bool gh_copy_file(const std::string & from, const std::string & to) {
    FILE * a = fopen(from.c_str(), "rb"); if (!a) return false; FILE * b = fopen((to + ".tmp").c_str(), "wb"); if (!b) { fclose(a); return false; }
    std::vector<char> buf(1 << 20); bool ok = true; for (;;) { size_t n = fread(&buf[0], 1, buf.size(), a); if (!n) break; if (fwrite(&buf[0], 1, n, b) != n) { ok = false; break; } }
    fclose(a); if (fclose(b) != 0) ok = false; if (!ok) { plat::remove_file((to + ".tmp").c_str()); return false; }
    plat::remove_file(to.c_str()); return plat::move_file(to + ".tmp", to);
}
static std::string gh_disk_dir(void) { return gApp.savedir + "\\" + std::string(gApp.prof_.id ? gApp.prof_.id : "x") + "\\disk"; }
static std::string gh_workpath(const std::string & name) {
    std::string orig = gApp.imgdir + "\\" + name, dd = gh_disk_dir(); std::string base = name; { size_t p = base.find_last_of("\\/"); if (p != std::string::npos) base = base.substr(p + 1); }
    std::string wp = dd + "\\" + base;
    plat::FileInfo ao, aw; bool hasorig = plat::stat_file(orig, ao), hascopy = plat::stat_file(wp, aw);
    if (hascopy) {   /* t601 (#544·#557): 사본 우선 — 원본이 없어도·바뀌어도 사본 그대로 구동하고 자동 재복사(= 게임 안 세이브 덮어쓰기)는 하지 않는다. 바뀜은 안내만 */
        if (hasorig && (aw.size_hi != ao.size_hi || aw.size_lo != ao.size_lo)) { gh_log("origin size differs from work copy (kept copy): %s", orig.c_str()); gApp.note = phr_("원본 이미지가 바뀌었습니다 (사본으로 계속합니다)"); gApp.note_until = SDL_GetTicks() + 6000u; }
        return wp;
    }
    if (!hasorig) return orig;   /* 사본도 원본도 없음 → 여는 쪽이 「원본 이미지가 없습니다」 안내 */
    plat::mkdir(gApp.savedir.c_str()); plat::mkdir((gApp.savedir + "\\" + std::string(gApp.prof_.id ? gApp.prof_.id : "x")).c_str()); plat::mkdir(dd.c_str());
    if (!gh_copy_file(orig, wp)) { gh_log("work copy failed -> opening original read-only: %s", orig.c_str()); return orig; }
    gh_log("work copy made: %s", wp.c_str());
    { FILE * oi = fopen((dd + "\\origin.ini").c_str(), "ab"); if (oi) { fprintf(oi, "%s\t%s\t%llu\n", base.c_str(), orig.c_str(), (unsigned long long)(((uint64_t)ao.size_hi << 32) | ao.size_lo)); fclose(oi); } }   /* t601: 이미지별 원본 경로·크기 기록 */
    return wp;
}
static int gh_reset_disks(const std::string & id) {   /* 「원본 디스크로 초기화」: 작품 저장폴더의 디스크 작업 사본을 지운다(다음 실행 때 원본에서 다시 만든다) */
    std::string dd = gApp.savedir + "\\" + id + "\\disk"; int n = 0; std::vector<std::string> names;
    plat::find_a(dd + "\\*", [&](const plat::DirEnt & fd) { if (fd.is_dir) return true; std::string nm = fd.name; if (nm == "origin.ini" || (nm.size() > 4 && nm.compare(nm.size() - 4, 4, ".bak") == 0)) return true; names.push_back(nm); return true; });
    for (auto & nm : names) {   /* t601 (#544): 직전 사본 1개는 .bak 로 남긴다(이전 .bak 는 덮어씀). 옮기지 못하면 그냥 지운다 */
        std::string f = dd + "\\" + nm, b = f + ".bak"; plat::remove_file(b);
        if (plat::move_file(f, b) || plat::remove_file(f)) n++; }
    plat::remove_file(dd + "\\origin.ini"); gh_log("reset disks: %s copies=%d (previous kept as .bak)", id.c_str(), n); return n;
}
static bool gh_attach(int idx, const char * name, bool replace = false) {
    std::string orig_path = gApp.imgdir + "\\" + name, path = gh_workpath(name); bool iscopy = path != orig_path;   /* t43h: 쓰기는 작업 사본에만 */
    uint64_t sz = 0;
    FILE * f = gh_open_rw(path, &sz, iscopy);
    if (!f) { gh_log("ERROR: cannot open image %s", path.c_str());
        if (!replace) gh_human(std::string(phr_("게임 디스크 이미지 파일을 열 수 없습니다.\n파일: ")) + aw::acp_to_u8(std::string(name)) + phr_("\n찾은 곳: ") + aw::acp_to_u8(gApp.imgdir) + phr_("\n파일이 없거나 옮겨졌을 수 있습니다. 작품 폴더 game.ini 의 imgdir(이미지 폴더) 줄과 파일 이름을 확인해 주세요.\n[위치 지정] 단추는 다음 판(롬 루트 연결)에서 제공합니다."));
        else { gh_human(std::string(phr_("교체할 디스크 파일을 열 수 없습니다: ")) + aw::acp_to_u8(std::string(name)) + phr_("\n(찾은 곳: ") + aw::acp_to_u8(gApp.imgdir) + ")"); gh_msgbox(gApp.errmsg.c_str()); }
        return false; }
    imageDisk * img; { prof::ImgInfo pi = prof::probe(path); uint32_t szk = (uint32_t)(sz / 1024ull);   /* r017: 클래스는 확장자가 아니라 파일 머리로 고른다 */
        if (pi.fmt == prof::F_D88) { img = new imageDiskD88(f, name, szk, false); gh_log("image[%d] class=D88 (header)", idx); }
        else if (pi.fmt == prof::F_NFD0 || pi.fmt == prof::F_NFD1) { img = new imageDiskNFD(f, name, szk, false, pi.fmt == prof::F_NFD1 ? 1u : 0u); gh_log("image[%d] class=NFD r%d (header)", idx, pi.fmt == prof::F_NFD1 ? 1 : 0); }
        else img = new imageDisk(f, name, sz, sz > 2880ull * 1024ull); }
    uint32_t h, c, s, ss; img->Get_Geometry(&h, &c, &s, &ss);
    gh_log("image[%d] %s: size=%llu CHS=%u/%u/%u ss=%u", idx, name, (unsigned long long)sz, c, h, s, ss);
    { /* 스테이트 헤더용 디스크 정보: 부팅 직전(수정 전) 내용 CRC32 */
        Pc98StateDisk d; d.name = name; d.size = sz; d.crc32 = 0xFFFFFFFFu;
        static uint32_t tab[256]; static bool ti = false;
        if (!ti) { for (uint32_t i = 0; i < 256; i++) { uint32_t cc = i; for (int k = 0; k < 8; k++) cc = (cc & 1u) ? (0xEDB88320u ^ (cc >> 1)) : (cc >> 1); tab[i] = cc; } ti = true; }
        std::vector<uint8_t> buf(1 << 20);
        FILE * fc = iscopy ? fopen(orig_path.c_str(), "rb") : f; if (!fc) fc = f;   /* 스테이트 헤더 CRC 는 원본 기준(사본이 바뀌어도 같은 값) */
        fseeko64(fc, 0, SEEK_SET);
        for (;;) { size_t n = fread(&buf[0], 1, buf.size(), fc); if (!n) break; for (size_t i = 0; i < n; i++) d.crc32 = tab[(d.crc32 ^ buf[i]) & 0xFFu] ^ (d.crc32 >> 8); }
        d.crc32 ^= 0xFFFFFFFFu; if (fc != f) fclose(fc); fseeko64(f, 0, SEEK_SET);
        if (replace && idx >= 0 && idx < 4 && gApp.disk_ix[idx] >= 0 && gApp.disk_ix[idx] < (int)gApp.disks.size()) gApp.disks[gApp.disk_ix[idx]] = d; else { if (idx >= 0 && idx < 4) gApp.disk_ix[idx] = (int)gApp.disks.size(); gApp.disks.push_back(d); }
        gh_log("image[%d] crc32=%08x", idx, d.crc32);
    }
    imageDiskList[idx] = img; imageDiskChange[idx] = true; img->Addref();
    if (idx <= 1) FDC_AssignINT13Disk((unsigned char)idx);
    else {
        signed char ide_index = -1; bool slave = false;
        IDE_Auto(ide_index, slave);
        IDE_Hard_Disk_Attach(ide_index, slave, (unsigned char)idx);
        updateDPT();
    }
    return true;
}

static void gh_bda_dump(const char * tag) {
    if (!gApp.bda) return;
    char p[1024]; snprintf(p, sizeof(p), "%s\\bda_%s.bin", gApp.outdir.c_str(), tag);
    FILE * f = fopen(p, "wb"); if (!f) { gh_log("bda dump: cannot open %s", p); return; }
    for (unsigned a = 0; a < 0x600; a++) { unsigned char b = mem_readb((PhysPt)a); fwrite(&b, 1, 1, f); }
    fclose(f); gh_log("bda dump: %s (0x000-0x5FF)", p);
}

/* t36cd: 내장 DOS 부팅 — sdlmain.cpp 의 VM_Boot_DOSBox_Kernel() 중 "dos_kernel_disabled" 분기를 그대로 옮긴 것(마우스·메뉴 항목 제외) */
static bool gh_boot_dos_kernel() {
    void DOS_Startup(Section *); void DRIVES_Startup(Section *); void DOS_InitClock(); void DOS_KeyboardLayout_Startup(Section *); void XMS_Startup(Section *); void EMS_Startup(Section *);
    void SHELL_MessagesInit(); void CONFIGSHELL_Init(); void CONFIGSHELL_Run(); void SHELL_Init(); void AUTOEXEC_Startup(Section *); void MSCDEX_Startup(Section *); void SHELL_Run();
    void update_pc98_function_row(unsigned char setting, bool force_redraw);
    gh_log("DOS boot: 내장 DOS 커널 시작 (cd=%s)", gApp.prof_.cd.c_str());
    { extern uint8_t MOUSE_IRQ; MOUSE_IRQ = gApp.mirq12 ? 12 : 13; }
    Init_MemHandles();
    DispatchVMEvent(VM_EVENT_DOS_BOOT);
    dos_kernel_disabled = false;
    DOS_Startup(NULL);
    DRIVES_Startup(NULL);
    if (IS_PC98_ARCH) update_pc98_function_row(1, false);
    DispatchVMEvent(VM_EVENT_DOS_INIT_KERNEL_READY);
    DOS_InitClock();
    DOS_KeyboardLayout_Startup(NULL);
    XMS_Startup(NULL);
    EMS_Startup(NULL);
    SHELL_MessagesInit();
    CONFIGSHELL_Init();
    CONFIGSHELL_Run();
    DispatchVMEvent(VM_EVENT_DOS_INIT_CONFIG_SYS_DONE);
    SHELL_Init();
    DispatchVMEvent(VM_EVENT_DOS_INIT_SHELL_READY);
    AUTOEXEC_Startup(NULL);
    MSCDEX_Startup(NULL);
    DispatchVMEvent(VM_EVENT_DOS_INIT_AUTOEXEC_BAT_DONE);
    DispatchVMEvent(VM_EVENT_DOS_INIT_AT_PROMPT);
    gApp.boot_tick = SDL_GetTicks(); if (!gApp.boot_tick) gApp.boot_tick = 1; gApp.pic0 = PIC_Ticks;
    if (!gApp.headless) { int cg_ = gApp.cur_game; std::string o_; if (cg_ >= 0 && cg_ < (int)gApp.ui_.games.size()) for (auto & kv_ : gApp.ui_.games[cg_].ov) o_ += kv_.first + "=" + kv_.second + " "; gh_log("APPLYDIAG id=%s has_t=%d ov[%s] g.scan=%d glow=%d curve=%d", (cg_ >= 0 && cg_ < (int)gApp.ui_.games.size()) ? gApp.ui_.games[cg_].id.c_str() : "-", (int)(cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && gApp.ui_.has_t[cg_]), o_.c_str(), gApp.ui_.g.scan, gApp.ui_.g.glow, gApp.ui_.g.curve); } gh_apply_settings(gApp.ui_.effective(gApp.cur_game), true);
    gh_log("DOS boot: 프롬프트 도달 — 셸 실행");
    SHELL_Run();
    gh_log("DOS boot: 셸 종료");
    GH_QUITSET();
    return true;
}

/* BOOT (PC-98 branch) re-done without the DOS kernel; called from cb_bios_boot__func */
bool VM_Boot_DOSBox_Kernel() {
    if (gApp.dosmode) return gh_boot_dos_kernel();
    int drive = gApp.prof_.bootletter;
    int di = drive - 'A';
    gh_log("direct boot: drive %c", drive);
    gh_bda_dump(gApp.boot_n > 1 ? "pre2" : "pre");
    /* PC-98 bus mouse tick IRQ: core sets MOUSE_IRQ=13 in MOUSE_OnReset(), which this host never runs -> it stayed 12 (FM board) */
    { extern uint8_t MOUSE_IRQ; MOUSE_IRQ = gApp.mirq12 ? 12 : 13; }
    /* t32: INT 18h 감싸기는 BIOS 리셋(포트 F0h, 약 2초) 뒤에 한다 — t31 은 리셋이 콜백 영역을 다시 써서 감싼 것이 사라졌다 */
    /* official DOSBox-X boots from inside its DOS kernel; the XMS driver has already taken all extended memory (BDA 0401h = 0), so games see no extended RAM */
    if (gApp.ext0) mem_writeb(0x401, 0x00);   /* t28: no longer needed - root cause was MOUSE_IRQ (t27); kept only as --ext0 test option */
    if (di < 0 || di >= MAX_DISK_IMAGES || !imageDiskList[di]) { gh_log("ERROR: no image on boot drive"); gh_human(phr_("부팅할 디스크가 연결되어 있지 않습니다.\n작품 폴더 game.ini 의 disk_a / boot 줄을 확인해 주세요.")); gh_msgbox(gApp.errmsg.c_str()); GH_QUITSET(); return true; }
    bootSector bootarea;
    memset(&bootarea, 0, sizeof(bootarea));
    imageDisk * img = imageDiskList[di];
    unsigned int bootsize = img->getSectSize();
    bool has_read = false, sect128 = false;
    imageDiskChange[di] = false;
    if (drive < 'C') {
        if (img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea, 128) == Int13Status::NoError && img->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 128, 128) == Int13Status::NoError &&
            img->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 256, 128) == Int13Status::NoError && img->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 384, 128) == Int13Status::NoError) {
            gh_log("boot: 128 byte/sector track0"); has_read = true; bootsize = 512; sect128 = true;
        }
        if (!has_read && img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea, 256) == Int13Status::NoError && img->Read_Sector(0, 0, 2, (uint8_t *)&bootarea + 256, 256) == Int13Status::NoError &&
            img->Read_Sector(0, 0, 3, (uint8_t *)&bootarea + 512, 256) == Int13Status::NoError && img->Read_Sector(0, 0, 4, (uint8_t *)&bootarea + 768, 256) == Int13Status::NoError) {
            gh_log("boot: 256 byte/sector track0"); has_read = true; bootsize = 1024; sect128 = true;
        }
    }
    unsigned int max_seg = std::min((unsigned int)(MEM_ConventionalPages() * (4096u / 16u)), 0xC000u);
    unsigned int load_seg = std::min(max_seg, 0x2000u) - (bootsize / 16u);
    Bitu stack_seg = 0x0030;
    if ((stack_seg + 0x20) > max_seg) stack_seg = max_seg - 0x20;
    if (!has_read) {
        if (img->Read_Sector(0, 0, 1, (uint8_t *)&bootarea) != Int13Status::NoError) { gh_log("ERROR: boot sector read failed"); gh_human(phr_("첫 디스크의 부트 섹터를 읽지 못했습니다.\n이미지 파일이 손상됐거나 지원하지 않는 형식일 수 있습니다.")); gh_msgbox(gApp.errmsg.c_str()); GH_QUITSET(); return true; }
    }
    if (bootarea.rawdata[0] == 0 && bootarea.rawdata[1] == 0) { gh_log("ERROR: boot sector empty"); gh_human(phr_("첫 디스크의 부트 영역이 비어 있습니다.\n부팅 디스크가 아닌 이미지(데이터 디스크)를 첫 칸에 넣었을 수 있습니다.")); gh_msgbox(gApp.errmsg.c_str()); GH_QUITSET(); return true; }

    if (drive == 'A' || drive == 'B') FDC_AssignINT13Disk((unsigned char)(drive - 'A'));
    GetDMAChannel(2)->tcount = true;
    GetDMAChannel(3)->tcount = true;

    MEM_BlockWrite((PhysPt)(load_seg << 4u), bootarea.rawdata, bootsize);
    gh_log("boot: loaded %u bytes at %04x:0000 (stack_seg=%04x)", bootsize, load_seg, (unsigned)stack_seg);
    DispatchVMEvent(VM_EVENT_GUEST_OS_BOOT);

    SegSet16(cs, (uint16_t)load_seg);
    SegSet16(ds, 0x0000);
    SegSet16(es, (uint16_t)load_seg);
    reg_ip = 0; reg_ebx = 0x200; reg_esp = 0xD8;
    SegSet16(ss, (uint16_t)stack_seg);
    reg_esi = (uint32_t)load_seg; reg_ecx = 0x200; reg_ebp = 0; reg_eax = 0x30; reg_edx = 0x1;
    reg_edi = gApp.di0 ? 0x0000u : 0xFFFEu;   /* official log: first IPL INT 1Bh shows DI=FFFE */

    if (gApp.boot400) { reg_eax = 0x4200; reg_ecx = 0xC000; CALLBACK_RunRealInt(0x18); }   /* 640x400 (old r020 behaviour, --boot400) */
    else { reg_eax = 0x4200; reg_ecx = 0x8000; CALLBACK_RunRealInt(0x18); }   /* 640x200 = official DOSBox-X default (pc98_640x200) */
    reg_eax = 0x4100; CALLBACK_RunRealInt(0x18);                    /* hide graphics */
    CPU_Push16((uint16_t)(BIOS_bootfail_code_offset >> 4));
    CPU_Push16(BIOS_bootfail_code_offset & 0xF);
    for (unsigned int i = 0; i < 80 * 25 * 2; i += 2) { mem_writew((PhysPt)(0xA0000 + i), 0x0000); mem_writew((PhysPt)(0xA2000 + i), 0x00E1); }
    PC98_show_cursor(false);

    uint32_t heads, cyls, sects, ssize;
    img->Get_Geometry(&heads, &cyls, &sects, &ssize);
    uint8_t F2HD_MODE = 0; uint16_t disk_equip = 0, disk_equip_144 = 0; uint8_t scsi_equip = 0;
    for (int i = 0; i < 2; i++) if (imageDiskList[i]) { disk_equip |= (0x0111u << i); disk_equip_144 |= (1u << i); F2HD_MODE |= (0x11u << i); }
    for (int i = 0; i < 2; i++) if (imageDiskList[i + 2]) {
        scsi_equip |= (1u << i);
        uint16_t m = 0x460u + ((uint16_t)i * 4u);
        mem_writeb(m + 0u, sects); mem_writeb(m + 1u, heads);
        mem_writew(m + 2u, (cyls & 0xFFFu) + (ssize == 512u ? 0x1000u : 0u) + (ssize == 1024u ? 0x2000u : 0) + 0x8000u);
    }
    mem_writew(0x55C, disk_equip); mem_writew(0x5AE, disk_equip_144); mem_writeb(0x482, scsi_equip);
    mem_writeb(0x488, 0); mem_writeb(0x493, F2HD_MODE); mem_writeb(0x5CA, 0);
    if (drive >= 'C') mem_writeb(0x584, 0xA0 + (drive - 'C'));
    else if ((ssize == 1024 && heads == 2 && cyls == 77 && sects == 8) || sect128) mem_writeb(0x584, 0x90 + (drive - 65));
    else if (ssize == 512 && heads == 2 && cyls == 80 && sects == 18) mem_writeb(0x584, 0x30 + (drive - 65));
    else { gh_log("boot: unknown floppy type, guessing"); mem_writeb(0x584, (ssize < 1024 ? 0x30 : 0x90) + (drive - 65)); }

    bootguest = false;
    bootdrive = drive - 65;
    if (!(gApp.boot_n > 1 && gApp.inwin_testfail == "noboot")) {   /* t51a: 시험 주입 noboot = 두 번째 부팅의 「도달 신호」 를 일부러 내지 않는다 */
        gApp.boot_tick = SDL_GetTicks();
        if (!gApp.boot_tick) gApp.boot_tick = 1;
    }
    gApp.pic0 = PIC_Ticks;
    if (gApp.boot_n > 1) gh_log("INWIN 부팅 신호 n=%d (전환 뒤 %u ms)", gApp.boot_n, SDL_GetTicks() - gApp.inwin_t0);
    if (!gApp.headless) { int cg_ = gApp.cur_game; std::string o_; if (cg_ >= 0 && cg_ < (int)gApp.ui_.games.size()) for (auto & kv_ : gApp.ui_.games[cg_].ov) o_ += kv_.first + "=" + kv_.second + " "; gh_log("APPLYDIAG id=%s has_t=%d ov[%s] g.scan=%d glow=%d curve=%d", (cg_ >= 0 && cg_ < (int)gApp.ui_.games.size()) ? gApp.ui_.games[cg_].id.c_str() : "-", (int)(cg_ >= 0 && cg_ < (int)ui::Ui::MAXG && gApp.ui_.has_t[cg_]), o_.c_str(), gApp.ui_.g.scan, gApp.ui_.g.glow, gApp.ui_.g.curve); } gh_apply_settings(gApp.ui_.effective(gApp.cur_game), true);
    gh_log("boot: CS:IP=%04x:%04x SS:SP=%04x:%04x", SegValue(cs), (unsigned)reg_ip, SegValue(ss), (unsigned)reg_sp);
    gh_bda_dump(gApp.boot_n > 1 ? "post2" : "post");
    gh_log("boot: regs AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X boot400=%d", (unsigned)reg_ax, (unsigned)reg_bx, (unsigned)reg_cx, (unsigned)reg_dx, (unsigned)reg_si, (unsigned)reg_di, (unsigned)reg_bp, (int)gApp.boot400);
    return true;
}

