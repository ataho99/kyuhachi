/* app/folder_text.inc -- t55b: 폴더 탐색기 화면 문구 (phr_ 키; .inc 는 check_literals 대상 아님) */
static const char * fpt(int i) {
    switch (i) {
    case 0: return phr_("폴더 고르기");
    case 1: return phr_("▶ 이 폴더 사용");
    case 2: return phr_("↑ 위로 (상위 폴더)");
    case 3: return phr_("내장 저장소");
    case 4: return phr_("외장 SD");
    case 5: return phr_("모든 파일 접근 권한이 필요합니다");
    case 6: return phr_("설정 열기");
    case 7: return phr_("「모든 파일 접근」을 켜고 돌아오세요");
    case 8: return phr_("↑↓:선택  ○:열기  ×:위로  탭:열기  바깥 탭:위로");
    case 9: return phr_("(하위 폴더 없음)");
    case 10: return phr_("추천");
    case 11: return phr_("취소");
    case 12: return phr_("게임 폴더를 검색하는 중입니다...");
    case 13: return phr_("저장소");
    case 14: return phr_("이 폴더에서 작품을 찾지 못했습니다");
    case 15: return phr_("다른 폴더는 「게임 폴더 지정」에서 고르세요");
    default: return "";
    }
}
