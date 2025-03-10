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

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_cond_wait=test_pthread_cond_wait \
	-c "$ROOT_DIR/src/routine.c" -o "$TMP_DIR/worker_wait_routine.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/worker_wait_failure.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/monitor.c" \
	"$ROOT_DIR/src/run.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/worker_wait_routine.o" \
	-o "$TMP_DIR/worker_wait_failure"
run_timeout 5 "$TMP_DIR/worker_wait_failure.out" \
	"$TMP_DIR/worker_wait_failure" \
	|| fail 'worker condition wait failure was not propagated'
grep -q 'worker wait failure: ok' "$TMP_DIR/worker_wait_failure.out" \
	|| fail 'worker condition wait failure test did not finish'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_mutex_unlock=test_mutex_unlock \
	-c "$ROOT_DIR/src/monitor.c" -o "$TMP_DIR/terminal_monitor.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/terminal_state.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/terminal_monitor.o" -o "$TMP_DIR/terminal_state"
"$TMP_DIR/terminal_state" >"$TMP_DIR/terminal_state.out" \
	|| fail 'terminal state was not committed atomically'
grep -q 'died' "$TMP_DIR/terminal_state.out" && fail 'stale death was printed'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dphilo_sleep_ms=test_philo_sleep_ms \
	-c "$ROOT_DIR/src/routine.c" -o "$TMP_DIR/interrupted_routine.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/interrupted_meal.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/interrupted_routine.o" -o "$TMP_DIR/interrupted_meal"
"$TMP_DIR/interrupted_meal" >"$TMP_DIR/interrupted_meal.out" \
	|| fail 'interrupted meal changed completion counters'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_create=test_pthread_create \
	-Dpthread_join=test_pthread_join \
	-c "$ROOT_DIR/src/run.c" -o "$TMP_DIR/lifecycle_run.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_mutex_destroy=test_mutex_destroy \
	-c "$ROOT_DIR/src/init.c" -o "$TMP_DIR/lifecycle_init.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/lifecycle_failure.c" \
	"$ROOT_DIR/src/monitor.c" \
	"$ROOT_DIR/src/routine.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/lifecycle_run.o" \
	"$TMP_DIR/lifecycle_init.o" -o "$TMP_DIR/lifecycle_failure"
run_timeout 8 "$TMP_DIR/lifecycle_failure.out" "$TMP_DIR/lifecycle_failure" \
	|| fail 'thread lifecycle failure was not propagated safely'

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dphilo_parse_args=test_parse_args \
	-Dphilo_table_init=test_table_init \
	-Dphilo_run=test_run \
	-Dphilo_table_destroy=test_destroy \
	-c "$ROOT_DIR/src/main.c" -o "$TMP_DIR/main_unsafe_main.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/main_unsafe.c" "$TMP_DIR/main_unsafe_main.o" \
	-o "$TMP_DIR/main_unsafe"
main_unsafe_out="$TMP_DIR/main_unsafe.out"
if "$TMP_DIR/main_unsafe" 1 2 3 4 >"$main_unsafe_out" 2>&1; then
	fail 'unsafe join failure returned success'
fi
grep -q 'worker thread could not be joined' "$main_unsafe_out" \
	|| fail 'join failure did not reach main'
if grep -q 'unsafe destroy called' "$main_unsafe_out"; then
	fail 'main destroyed resources after join failure'
fi
if grep -q 'normal exit hook\|buffered stdio marker' "$main_unsafe_out"; then
	fail 'unsafe join failure used the normal stdio exit path'
fi

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
