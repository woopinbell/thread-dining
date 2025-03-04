#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP_DIR=$(mktemp -d)

cleanup()
{
	rm -rf "$TMP_DIR"
}

fail()
{
	printf 'smoke: %s\n' "$1" >&2
	exit 1
}

check_log_format()
{
	awk '
		/^[0-9]+ [1-9][0-9]* (has taken a fork|is eating|is sleeping|is thinking|died)$/ { next }
		{ bad = 1 }
		END { exit bad }
	' "$1" || fail "bad log format in $1"
}

run_timeout()
{
	limit=$1
	outfile=$2
	shift 2
	"$@" >"$outfile" 2>&1 &
	pid=$!
	(
		sleep "$limit"
		kill -TERM "$pid" 2>/dev/null || true
	) &
	guard=$!
	set +e
	wait "$pid"
	status=$?
	set -e
	kill "$guard" 2>/dev/null || true
	wait "$guard" 2>/dev/null || true
	return "$status"
}

trap cleanup EXIT INT TERM

make -C "$ROOT_DIR" >/dev/null

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_mutex_init=test_mutex_init \
	-Dpthread_mutex_destroy=test_mutex_destroy \
	-c "$ROOT_DIR/src/init.c" -o "$TMP_DIR/init_failure_init.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/init_failure.c" "$TMP_DIR/init_failure_init.o" \
	-o "$TMP_DIR/init_failure"
"$TMP_DIR/init_failure" || fail 'partial mutex initialization cleanup failed'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dclock_gettime=test_clock_gettime \
	-c "$ROOT_DIR/src/time.c" -o "$TMP_DIR/monotonic_time.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/monotonic_clock.c" "$TMP_DIR/monotonic_time.o" \
	-o "$TMP_DIR/monotonic_clock"
"$TMP_DIR/monotonic_clock" || fail 'elapsed time did not use a monotonic clock'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_create=test_pthread_create \
	-c "$ROOT_DIR/src/run.c" -o "$TMP_DIR/start_barrier_run.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/start_barrier.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/monitor.c" \
	"$ROOT_DIR/src/routine.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/start_barrier_run.o" \
	-o "$TMP_DIR/start_barrier"
"$TMP_DIR/start_barrier" >"$TMP_DIR/start_barrier.out" \
	|| fail 'workers did not share one release timestamp'

invalid_out="$TMP_DIR/invalid.out"
if "$ROOT_DIR/philo" 0 100 10 10 >"$invalid_out" 2>&1; then
	fail 'invalid philosopher count succeeded'
fi
grep -q 'Usage: ./philo' "$invalid_out" || fail 'invalid args did not print usage'

overflow_out="$TMP_DIR/overflow.out"
if "$ROOT_DIR/philo" 2 999999999999999999999 10 10 >"$overflow_out" 2>&1; then
	fail 'overflow argument succeeded'
fi

single_out="$TMP_DIR/single.out"
run_timeout 2 "$single_out" "$ROOT_DIR/philo" 1 80 40 40 \
	|| fail 'single philosopher did not exit cleanly'
check_log_format "$single_out"
grep -q '1 has taken a fork' "$single_out" || fail 'single philosopher missed fork log'
grep -q '1 died' "$single_out" || fail 'single philosopher missed death log'

finite_out="$TMP_DIR/finite.out"
run_timeout 3 "$finite_out" "$ROOT_DIR/philo" 2 250 50 50 2 \
	|| fail 'finite meal run did not exit cleanly'
check_log_format "$finite_out"
grep -q 'died' "$finite_out" && fail 'finite meal run had a death'
eat_count=$(grep -c 'is eating' "$finite_out" || true)
[ "$eat_count" -ge 4 ] || fail 'finite meal run did not eat enough'

nodeath_out="$TMP_DIR/nodeath.out"
run_timeout 5 "$nodeath_out" "$ROOT_DIR/philo" 5 800 100 100 3 \
	|| fail 'no-death meal run did not exit cleanly'
check_log_format "$nodeath_out"
grep -q 'died' "$nodeath_out" && fail 'no-death meal run had a death'
eat_count=$(grep -c 'is eating' "$nodeath_out" || true)
[ "$eat_count" -ge 15 ] || fail 'no-death meal run did not reach meal count'

printf 'smoke: ok\n'
