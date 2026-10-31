<div align="center">

# 九八 · Kyuhachi

### PC-98 BOOTLOADER

**PC-98, one click away.**

**Double-click. Pick a game. Play.**
No config files. No mapper menus. No BIOS hunting at 2 a.m.

![version](https://img.shields.io/badge/version-v0.5.0--t52a-ff4d6d)
![platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4)
![engine](https://img.shields.io/badge/engine-DOSBox--X%20core-6c5ce7)
![license](https://img.shields.io/badge/license-GPL--2.0-2ea44f)
![made with](https://img.shields.io/badge/code-100%25%20AI--written-black)

</div>

---

## Why Kyuhachi?

Classic PC-98 emulation means juggling config files, disk swaps, keyboard maps and font ROMs before you even see a title screen.
Kyuhachi takes the battle-tested DOSBox-X core, cuts it down to just what PC-98 games need, and wraps it in something that feels like a console.

| | |
|---|---|
| 🎮 **Per-game profiles** | Boot method, keys and autotype tuned for each title — it just starts. |
| 🖥️ **Pixel-perfect** | Integer scaling with no blur. 4K fullscreen to a cozy window, switched in place. |
| 🕹️ **Gamepad first** | DualSense tested. Japanese-style confirm / cancel out of the box, fully remappable. |
| 🈳 **Hangul-ready** | Korean fan translations render with a built-in font derived from the open Neodgm. |
| ✏️ **Your words** | Every on-screen string lives in `strings.txt`. Edit in Notepad, restart, done. |
| 🌗 **Feels native** | Title bar follows your Windows dark / light theme. Dark by default. |
| 🧳 **Portable** | One folder. Unzip anywhere — Korean paths included. Writes nothing to the registry. |

## Auto-launch — tuned on a full PC-98 library

Add a disk image and Kyuhachi works out how to boot it. No per-game config for the common case.

How it decides (rules R1–R7, built and tuned against thousands of real PC-98 disk images):

| Step | What it does |
|---|---|
| Read the disk | D88 is flattened to a raw floppy, then the FAT12 file system is read directly |
| Find the launcher | Picks the real game EXE / BAT; skips tools (BASIC, N88, mouse drivers, DEBUG, XREF …) |
| Prefer the small launcher | When several BATs exist, the short launcher BAT wins over long scripts |
| Step into folders | If the root has no launcher and exactly one sub-folder does, it goes there |
| Self-extracting disks | Unpacks SFX archives across all disks in order before booting |
| Installers last | SETUP / INST-style names are used only when nothing better exists |
| Nothing found | Shows a clear "no launch candidate" notice instead of crashing |

Where it stands today (internal test runs):

- **Regression set** — 24 titles boot identically on every build: 0 crashes, 0 errors.
- **Crash sweep** — 61 images that used to crash the loader now all start cleanly.
- **Auto-run sample** — 14 of 26 reach the game screen with zero setup; the rest wait for input, need a driver, or are installers.
- **Self-extracting sets** — 32 of 34 unpack and boot.

Titles that don't boot by themselves aren't dead ends: set the boot order and per-disk steps once in the UI, and Kyuhachi remembers.

## Quick start

```
1. Unzip Kyuhachi-v0.5.0-t52a.zip
2. Put your disk images in   games/
3. (optional) Fonts in        res/fonts/
4. Run Kyuhachi.exe
```

## Package layout

```
Kyuhachi/
  Kyuhachi.exe     main program
  strings.txt      on-screen text (UTF-8, key=value)
  README.txt       quick guide
  LICENSE.txt      GPL-2.0 + upstream notices
  res/
    fonts/         optional fonts (built-in Neodgm used if empty)
  games/           your own disk images
  saves/           save data (created on first run)
  src/             where to get the source
```

## Bring your own

Kyuhachi ships **no** game images, NEC BIOS or font ROMs, and no font without a redistribution license.
Dump what you own and drop it in. A built-in font is used when `res/fonts/` is empty.

## 100% AI-coded

Every line of code in this repository was written by AI agents. No human typed the source.
A human (the project owner) set the direction and made every call.

| Role | Model / tool |
|---|---|
| Lead / review (planning, verdicts) | Anthropic Claude Opus 5.5 — Claude Code |
| Builder (host, UI, build scripts) | Anthropic Claude Sonnet 5.5 — Claude Code |

> Model list is filled from session records; earlier sessions may have used other Claude versions.

## Build from source

Full source is in this repository (GPL-2.0). See [`docs/BUILD.md`](docs/BUILD.md).

## Credits

Built on [DOSBox-X](https://github.com/joncampbell123/dosbox-x) — huge thanks to its developers.
Third-party notices: [`NOTICE.md`](NOTICE.md).

---

<div align="center">

## 한국어

**큐하치 — 더블클릭 한 번이면 PC-98.**

PC-98 BOOTLOADER · 코드 100% AI 작성

</div>

설정 파일, 매퍼 메뉴, 바이오스 찾아 헤매기 없이 게임을 고르고 바로 시작합니다.
검증된 DOSBox-X 코어에서 PC-98 게임에 필요한 것만 남겨 콘솔처럼 다듬었습니다.

- **작품별 프로필** — 부팅 방식·키·자동 입력을 작품마다 맞춰 두어 그냥 켜집니다.
- **픽셀 그대로** — 정수 배율, 번짐 없음. 전체화면과 창을 그 자리에서 전환합니다.
- **패드 우선** — 듀얼센스 확인. 일본식 결정·취소 배치가 기본이고 자유롭게 바꿀 수 있습니다.
- **한글 패치 대응** — 오픈 글꼴 Neodgm 동봉.
- **문구는 내 마음대로** — 화면 문구가 전부 `strings.txt` 에. 메모장으로 고치고 다시 켜면 끝.
- **윈도우와 한 몸** — 제목표시줄이 다크·라이트 테마를 따라갑니다. 기본은 다크.
- **포터블** — 폴더 하나. 한글 경로 포함 어디에 풀어도 됩니다.
- **자동 실행법** — 게임 이미지를 넣으면 실행 파일을 알아서 고릅니다(D88 변환 · FAT12 직접 읽기 · 도구류 제외 · 하위 폴더 진입 · 압축 디스크 풀기). 수천 개 PC-98 이미지로 다듬었고, 회귀 24작품 무결 · 크래시 61건 해소. 저절로 안 뜨는 작품은 화면에서 한 번 정해 두면 됩니다.

게임 이미지·NEC 바이오스·폰트 롬은 들어 있지 않습니다. 가진 게임 이미지를 `games/` 에 넣으세요. 글꼴은 내장 글꼴이 있어 `res/fonts/` 가 비어도 됩니다.

### 100% AI 코딩
이 저장소의 코드는 한 줄도 사람이 직접 치지 않았습니다. 전부 AI 에이전트(Anthropic Claude Opus 5.5 · Sonnet 5.5 — Claude Code)가 썼고, 사람은 방향을 정하고 판단했습니다.
