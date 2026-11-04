/* app/dosmount.h -- #451 step 2: floppy images are mounted by internal calls (fatDrive -> DriveManager -> BIOS attach),
 * not by autoexec "imgmount" text. Same steps as IMGMOUNT::MountFat in DOSBox-X dos_programs.cpp for "-t floppy"
 * (no geometry options, read/write). Included in place; single translation unit. */
#include "dos/drives.h"
extern bool AttachToBiosAndIdeByLetter(imageDisk * image, const char drive, const unsigned char ide_index, const bool ide_slave);
extern int32_t swapInDisksSpecificDrive;

static std::string gh_base_u8(const std::string & p) {
    std::string nm = p; size_t b = nm.find_last_of("\\/"); if (b != std::string::npos) nm = nm.substr(b + 1);
    return aw::acp_to_u8(nm);
}

/* paths = host files (first = drive image, the rest = swap list). On failure nothing stays mounted; why = file name for the user. */
static bool gh_mount_floppy(char drive, const std::vector<std::string> & paths, std::string & why) {
    drive = (char)toupper((unsigned char)drive);   /* callers pass autoexec-style lower case */
    if (paths.empty()) { why = "no image"; return false; }
    if (drive < 'A' || drive > 'Z') { why = "bad drive letter"; return false; }
    if (Drives[drive - 'A']) { why = std::string("drive already mounted: ") + drive; gh_log("ERROR: %s", why.c_str()); return false; }
    std::vector<DOS_Drive *> imgDisks; std::vector<std::string> options;
    for (size_t i = 0; i < paths.size(); i++) {
        fatDrive * fd = new fatDrive(paths[i].c_str(), 0u, 0u, 0u, 0u, options);
        if (!fd->created_successfully) {
            why = gh_base_u8(paths[i]); gh_log("ERROR: fatDrive create failed: %s", paths[i].c_str());
            delete fd; for (size_t k = 0; k < imgDisks.size(); k++) delete imgDisks[k];
            return false;
        }
        strcpy(fd->info, "fatDrive "); strncat(fd->info, paths[i].c_str(), sizeof(fd->info) - 10);
        imgDisks.push_back(fd);
    }
    int mediaid = (int)((unsigned char)((fatDrive *)imgDisks[0])->GetMediaByte()); if (mediaid < 0xF0) mediaid = 0xF0;
    for (size_t ct = 0; ct < imgDisks.size(); ct++) DriveManager::AppendDisk(drive - 'A', imgDisks[ct]);
    DriveManager::InitializeDrive(drive - 'A');
    mem_writeb(Real2Phys(dos.tables.mediaid) + ((unsigned int)drive - 'A') * dos.tables.dpb_size, (uint8_t)mediaid);
    RealPt save_dta = dos.dta(); dos.dta(dos.tables.tempdta);
    for (size_t ct = 0; ct < imgDisks.size(); ct++) {
        DriveManager::CycleDisks(drive - 'A', ct == imgDisks.size() - 1);
        char root[7] = { drive, ':', '\\', '*', '.', '*', 0 }; DOS_FindFirst(root, DOS_ATTR_VOLUME);   /* label into the dir cache */
    }
    dos.dta(save_dta);
    unsigned char di = (unsigned char)(drive - 'A');
    bool swap_ok = swapInDisksSpecificDrive == di || swapInDisksSpecificDrive == -1;
    if (imgDisks.size() == 1 || (imgDisks.size() > 1 && di < 2 && swap_ok)) {
        imageDisk * image = ((fatDrive *)imgDisks[0])->loadedDisk;
        if (AttachToBiosAndIdeByLetter(image, drive, (unsigned char)0xFF, false) && swap_ok) {
            for (size_t si = 0; si < MAX_SWAPPABLE_DISKS; si++) if (diskSwap[si] != NULL) { diskSwap[si]->Release(); diskSwap[si] = NULL; }
            swapInDisksSpecificDrive = -1;
            if (paths.size() > 1) {
                diskSwap[0] = image; diskSwap[0]->Addref(); swapPosition = 0; swapInDisksSpecificDrive = di;
                for (size_t si = 1; si < MAX_SWAPPABLE_DISKS && si < paths.size(); si++) {
                    imageDisk * img = ((fatDrive *)imgDisks[si])->loadedDisk; diskSwap[si] = img; diskSwap[si]->Addref();
                }
            }
        }
    }
    if (di <= 1) imageDiskChange[di] = false;
    gh_log("mount (internal): %c: %s%s", drive, paths[0].c_str(), paths.size() > 1 ? " (+swap list)" : "");
    return true;
}
