#!/bin/sh

set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP_DIR=$(mktemp -d)
TSAN_CC=${TSAN_CC:-cc}
TSAN_REQUIRED=${TSAN_REQUIRED:-0}

cleanup()
{
	rm -rf "$TMP_DIR"
}

fail()
{
	printf 'tsan: %s\n' "$1" >&2
	exit 1
}

skip()
{
	printf 'tsan: skipped (%s)\n' "$1" >&2
	if [ "$TSAN_REQUIRED" -eq 1 ]; then
		exit 1
	fi
	exit 77
}

run_timeout()
{
	limit=$1
	stdout_file=$2
	stderr_file=$3
	shift 3
	"$@" >"$stdout_file" 2>"$stderr_file" &
	pid=$!
	(
		sleep "$limit"
		kill -TERM "$pid" 2>/dev/null || true
	) &
	guard=$!
	wait "$pid"
	status=$?
	kill "$guard" 2>/dev/null || true
	wait "$guard" 2>/dev/null || true
	return "$status"
}

check_tsan_stderr()
{
	if grep -q 'ThreadSanitizer' "$1"; then
		cat "$1" >&2
		return 1
	fi
	return 0
}

run_case()
{
	name=$1
	shift
	stdout_file="$TMP_DIR/$name.out"
	stderr_file="$TMP_DIR/$name.err"
	TSAN_OPTIONS='halt_on_error=1:exitcode=66' \
		run_timeout 20 "$stdout_file" "$stderr_file" "$TMP_DIR/philo-tsan" "$@"
	status=$?
	if [ "$status" -ne 0 ]; then
		cat "$stderr_file" >&2
		printf 'tsan: %s workload exited with status %d\n' \
			"$name" "$status" >&2
		return 1
	fi
	check_tsan_stderr "$stderr_file"
}

check_log()
{
	awk '
		/^[0-9]+ [1-9][0-9]* (has taken a fork|is eating|is sleeping|is thinking|died)$/ {
			if (seen && $1 < previous) bad = 1
			previous = $1
			seen = 1
			next
		}
		{ bad = 1 }
		END { exit bad || !seen }
	' "$1" || fail "$2 workload produced an invalid log"
}

check_progress()
{
	awk -v count="$2" -v target="$3" '
		$3 == "is" && $4 == "eating" { meals[$2]++ }
		END {
			for (id = 1; id <= count; id++)
				if (meals[id] < target) exit 1
		}
	' "$1" || fail "$4 workload did not reach its meal target"
}

check_death()
{
	awk '
		{
			if (terminal) after = 1
			if ($3 == "died") { deaths++; terminal = 1 }
		}
		END { exit after || deaths != 1 }
	' "$1" || fail 'death workload did not end with exactly one death'
}

trap cleanup EXIT INT TERM

case "$TSAN_REQUIRED" in
	0|1)
		;;
	*)
		fail 'TSAN_REQUIRED must be 0 or 1'
		;;
esac

cat >"$TMP_DIR/probe.c" <<'PROBE'
#include <pthread.h>

static int	g_value;

static void	*set_value(void *arg)
{
	(void)arg;
	g_value = 1;
	return (0);
}

int	main(void)
{
	pthread_t	thread;

	if (pthread_create(&thread, 0, set_value, 0) != 0)
		return (1);
	if (pthread_join(thread, 0) != 0)
		return (1);
	return (g_value != 1);
}
PROBE

if ! "$TSAN_CC" -Wall -Wextra -Werror -pthread -fsanitize=thread -g \
	"$TMP_DIR/probe.c" -o "$TMP_DIR/tsan-probe" \
	>"$TMP_DIR/probe-build.out" 2>"$TMP_DIR/probe-build.err"; then
	cat "$TMP_DIR/probe-build.err" >&2
	skip "$TSAN_CC cannot build a ThreadSanitizer probe"
fi
if [ ! -x "$TMP_DIR/tsan-probe" ]; then
	fail "$TSAN_CC reported probe success without producing an executable"
fi

TSAN_OPTIONS='halt_on_error=1:exitcode=66' \
	run_timeout 10 "$TMP_DIR/probe.out" "$TMP_DIR/probe.err" \
	"$TMP_DIR/tsan-probe"
probe_status=$?
if [ "$probe_status" -ne 0 ]; then
	cat "$TMP_DIR/probe.err" >&2
	skip "ThreadSanitizer probe exited with status $probe_status"
fi
if ! check_tsan_stderr "$TMP_DIR/probe.err"; then
	skip 'ThreadSanitizer probe reported a runtime error'
fi

if ! "$TSAN_CC" -Wall -Wextra -Werror -pthread -fsanitize=thread -g \
	-I"$ROOT_DIR/include" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/main.c" \
	"$ROOT_DIR/src/monitor.c" \
	"$ROOT_DIR/src/parse.c" \
	"$ROOT_DIR/src/routine.c" \
	"$ROOT_DIR/src/run.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" -o "$TMP_DIR/philo-tsan" \
	>"$TMP_DIR/build.out" 2>"$TMP_DIR/build.err"; then
	cat "$TMP_DIR/build.err" >&2
	fail 'project build failed after the ThreadSanitizer probe passed'
fi
if [ ! -x "$TMP_DIR/philo-tsan" ]; then
	fail "$TSAN_CC reported project build success without producing an executable"
fi

run_case finite 7 1000 5 5 4 \
	|| fail 'finite schedule reported a race or runtime error'
check_log "$TMP_DIR/finite.out" finite
if grep -q 'died' "$TMP_DIR/finite.out"; then
	fail 'finite workload reported a death'
fi
check_progress "$TMP_DIR/finite.out" 7 4 finite

run_case death 5 60 80 10 \
	|| fail 'terminal schedule reported a race or runtime error'
check_log "$TMP_DIR/death.out" death
check_death "$TMP_DIR/death.out"

run_case contention 17 2000 5 5 3 \
	|| fail 'contention schedule reported a race or runtime error'
check_log "$TMP_DIR/contention.out" contention
if grep -q 'died' "$TMP_DIR/contention.out"; then
	fail 'contention workload reported a death'
fi
check_progress "$TMP_DIR/contention.out" 17 3 contention

printf 'tsan: ok\n'
