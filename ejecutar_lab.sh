#!/usr/bin/env bash
# ejecutar_lab.sh - corre todo el laboratorio U10 Post 1 y guarda salidas en resultados/
# Uso: bash ejecutar_lab.sh   (desde la carpeta del repo, dentro de Linux o WSL)
set -u
REPO="$(cd "$(dirname "$0")" && pwd)"
OUT="$REPO/resultados"; mkdir -p "$OUT"
P1=~/u10post1; P2=~/u10post2; mkdir -p $P1 $P2
cp "$REPO/cache_bench.c" $P1/; cp "$REPO"/{mymaps,pagefault_demo,mmap_fault_cost}.c $P2/

echo "== Parte 1 ==" 
cd $P1
{ echo '$ for i in 0 1 2 3; do echo -n "Cache level $i: "; cat /sys/devices/system/cpu/cpu0/cache/index${i}/size 2>/dev/null || echo "N/A"; done'
  for i in 0 1 2 3; do echo -n "Cache level $i: "; cat /sys/devices/system/cpu/cpu0/cache/index${i}/size 2>/dev/null || echo "N/A"; done; } > $OUT/cp1_cache.txt
{ echo '$ cd /sys/devices/system/cpu/cpu0/cache && grep . index*/{level,type,ways_of_associativity,coherency_line_size,number_of_sets}'
  (cd /sys/devices/system/cpu/cpu0/cache && grep . index*/{level,type,ways_of_associativity,coherency_line_size,number_of_sets})
  echo '$ lscpu | grep -E "Model name|cache"'; lscpu | grep -E "Model name|cache"; } > $OUT/cp1_geometria.txt
{ echo '$ gcc -O0 -Wall -o cache_bench cache_bench.c && ./cache_bench'; gcc -O0 -Wall -o cache_bench cache_bench.c 2>&1 && ./cache_bench; } > $OUT/parte1_cache_bench.txt 2>&1
{ for f in -fprefetch-loop-arrays -fno-prefetch-loop-arrays; do echo "\$ gcc -O2 $f -S -o - cache_bench.c | grep -c prefetch"; gcc -O2 $f -S -o - cache_bench.c | grep -c prefetch; done
  for f in -fprefetch-loop-arrays -fno-prefetch-loop-arrays; do echo "\$ gcc -O2 $f -o cache_bench_O2 cache_bench.c && ./cache_bench_O2"; gcc -O2 $f -o cache_bench_O2 cache_bench.c && ./cache_bench_O2; echo; done; } > $OUT/parte1_prefetch_flags.txt 2>&1
{ echo '$ gcc -O2 -fprefetch-loop-arrays -S -o - cache_bench.c | awk "/^[a-z_]+:/{f=\$1} /prefetch/{print f, \$0}"'
  gcc -O2 -fprefetch-loop-arrays -S -o - cache_bench.c | awk '/^[a-z_]+:/{f=$1} /prefetch/{print f, $0}'; } > $OUT/parte1_prefetch_asm.txt

echo "== Parte 2 =="
cd $P2
for f in mymaps pagefault_demo mmap_fault_cost; do gcc -Wall -o $f $f.c; done
gcc -Wall -DUSE_POPULATE -o mmap_populate mmap_fault_cost.c
( sleep 15 | ./mymaps > mymaps_out.txt ) &
sleep 1; PID=$(pgrep -n -x mymaps)
{ echo '$ gcc -o mymaps mymaps.c && ./mymaps &'; cat mymaps_out.txt; echo '$ PID=$!'; echo '$ cat /proc/$PID/maps'; cat /proc/$PID/maps; } > $OUT/p2_cp1_maps.txt
{ echo '$ pmap -x $PID'; pmap -x $PID; } > $OUT/p2_cp2_pmap_mymaps.txt
{ echo "\$ awk '/^[0-9a-f]+-/{s=/heap|stack|r-xp.*(libc|musl)/} s && /^[0-9a-f]+-|^(Size|Rss|Pss|Shared_Clean|Shared_Dirty|Private_Clean|Private_Dirty|Referenced|Anonymous):/' /proc/\$PID/smaps"
  awk '/^[0-9a-f]+-/{s=/heap|stack|r-xp.*(libc|musl)/} s && /^[0-9a-f]+-|^(Size|Rss|Pss|Shared_Clean|Shared_Dirty|Private_Clean|Private_Dirty|Referenced|Anonymous):/' /proc/$PID/smaps
  echo '$ kill $PID'; } > $OUT/p2_cp2_smaps.txt
kill $PID 2>/dev/null
bash -c '{ echo "\$ cat /proc/\$\$/smaps | head -60"; head -60 /proc/$$/smaps; echo "\$ pmap -x \$\$"; pmap -x $$; }' > $OUT/p2_cp2_shell.txt
{ echo '$ vmstat -s | head -15'; vmstat -s | head -15; echo '$ free -h'; free -h; } > $OUT/p2_cp2_vmstat_free.txt
{ echo '$ gcc -o pagefault_demo pagefault_demo.c && ./pagefault_demo'; ./pagefault_demo; } > $OUT/p2_cp3_pagefault.txt 2>&1
{ echo '$ /usr/bin/time -v ./pagefault_demo'; /usr/bin/time -v ./pagefault_demo; } > $OUT/p2_cp3_time_v.txt 2>&1
{ echo '$ gcc -o mmap_fault_cost mmap_fault_cost.c && ./mmap_fault_cost'; for k in 1 2 3; do ./mmap_fault_cost; echo; done; } > $OUT/p2_cp4_mmap.txt 2>&1
{ echo '$ gcc -DUSE_POPULATE -o mmap_populate mmap_fault_cost.c && ./mmap_populate'; for k in 1 2 3; do ./mmap_populate; echo; done; } > $OUT/p2_cp4_mmap_populate.txt 2>&1
bash -c 'P=$$
echo "\$ wc -l /proc/\$\$/maps /proc/\$\$/smaps"; wc -l /proc/$P/maps /proc/$P/smaps | sed "s#/proc/$P#/proc/\$\$#"
for f in maps smaps smaps_rollup; do echo "\$ time (for i in \$(seq 500); do cat /proc/\$\$/$f > /dev/null; done)"; { time (for i in $(seq 500); do cat /proc/$P/$f > /dev/null; done); } 2>&1 | grep real; done
echo "\$ time (for i in \$(seq 500); do pmap -x \$\$ > /dev/null; done)"; { time (for i in $(seq 500); do pmap -x $P > /dev/null; done); } 2>&1 | grep real
echo "\$ strace -f -e trace=open,openat pmap -x \$\$ 2>&1 | grep /proc"; (strace -f -e trace=open,openat pmap -x $P 2>&1 | grep "/proc/$P" | sed "s#/proc/$P#/proc/\$\$#") || echo "(strace no disponible)"' > $OUT/p2_cp5_costo_smaps_pmap.txt 2>&1
echo "LISTO, salidas en $OUT"
