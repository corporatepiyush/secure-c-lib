#!/bin/bash
cd /Users/piyush/ai/secure-c-libs
echo "=== Parallel test run ==="
tmpdir="/tmp/scl_results_$$"
mkdir -p "$tmpdir"

count=0; pass=0; fail=0; skip=0
shopt -s nullglob

for t in build/./tests/test_*_bin; do
    name=$(basename "$t" _bin)
    case "$name" in
        test_scl_concurrent_bst_debug|test_scl_concurrent_alloc_arena)
            skip=$((skip+1))
            echo "SKIP $name"
            continue ;;
    esac
    count=$((count+1))
    ( "$t" > "$tmpdir/$name.log" 2>&1 && echo "PASS $name" || echo "FAIL $name" ) &

    # Limit parallel jobs to ~20 at a time
    if [ $((count % 20)) -eq 0 ]; then wait; fi
done

wait

echo ""
for f in "$tmpdir"/*.log; do
    [ -f "$f" ] || continue
    name=$(basename "$f" .log)
    last=$(tail -1 "$f")
    if echo "$last" | grep -q "passed, 0 failed"; then
        pass=$((pass+1))
    else
        fail=$((fail+1))
        echo "--- $name ---"
        tail -5 "$f"
    fi
done

rm -rf "$tmpdir"
echo "=== $count tests: $pass passed, $fail failed ($skip skipped) ==="