#!/bin/bash
# T11/R-4: 한글 리터럴이 T("…") 밖에 있는 줄 수. 사용: tools/check_literals.sh <src 폴더>  출력: literals outside_T=N log_calls=N (log_calls = 그중 로그 호출 줄)
D=${1:-$(dirname "$0")/../src}
perl -CSD -e '
 my($tot,$log)=(0,0);
 for my $f (@ARGV){ next if $f=~/embed|\.inc$/; open(my $h,"<",$f) or next;
  while(<$h>){ my $l=$_; $l=~s/\r?\n$//; next unless $l=~/[\x{AC00}-\x{D7A3}]/; next if $l=~m{^\s*(//|\*|/\*)};
   $l=~s{//.*$}{}; $l=~s{/\*.*?\*/}{}g; $l=~s{/\*.*$}{}; $l=~s/\bT\(\s*"(?:[^"\\]|\\.)*"\s*\)//g; next unless $l=~/[\x{AC00}-\x{D7A3}]/;
   $tot++; $log++ if $l=~/\b(gh_log|GH_LOG|LOG|log_|fprintf|printf|puts)\b/; } }
 print "literals outside_T=$tot log_calls=$log\n";' $(ls $D/*.h $D/*.cpp $D/*/*.h $D/*/*.cpp $D/*/*/*.h $D/*/*/*.cpp 2>/dev/null)
