#pragma once
/* screens/start.h -- S8a: S_START screen. Code moved unchanged from kh_ui.h (members of struct Ui, defined out of line). */
inline void Ui::k_start(UKey k, Act & a) {
    do {
            if (k == UK_OK) start_ok(a);
            else if (k == UK_CANCEL) go(start_ret, 0);
            break;
    } while (0);
}
