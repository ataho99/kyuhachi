#!/bin/bash
P=/mnt/h/RETRO_SET/PC98_PLAYER; T=$P/시험_t52i_061057
sed -n '30,80p' "$T/f1/out_keys0/settings_uikeys.ini"
grep -a -n "scope\|kitan" "$T/f1/out_keys0/kyuhachi_log.txt" | head -5
