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
	printf 'concurrency: %s\n' "$1" >&2
	exit 1
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
		END { exit bad }
	' "$1" || fail "invalid or decreasing log in $1"
}

check_progress()
{
	file=$1
	count=$2
	target=$3
	awk -v count="$count" -v target="$target" '
		$3 == "is" && $4 == "eating" { meals[$2]++ }
		END {
			for (id = 1; id <= count; id++)
				if (meals[id] < target) exit 1
		}
	' "$file" || fail "a philosopher did not reach the meal target"
}

check_terminal_line()
{
	awk '
		{
			if (terminal) after = 1
			if ($3 == "died") { deaths++; terminal = 1 }
		}
		END { exit after || deaths != 1 }
	' "$1" || fail "death was not the only terminal log"
}

trap cleanup EXIT INT TERM

make -C "$ROOT_DIR" >/dev/null

for count in 2 5 17; do
	output="$TMP_DIR/finite-$count.out"
	run_timeout 6 "$output" "$ROOT_DIR/philo" "$count" 2000 5 5 4 \
		|| fail "finite run for $count philosophers did not finish"
	check_log "$output"
	grep -q 'died' "$output" && fail "finite run reported a death"
	check_progress "$output" "$count" 4
done

iteration=0
while [ "$iteration" -lt 8 ]; do
	output="$TMP_DIR/repeat-$iteration.out"
	run_timeout 4 "$output" "$ROOT_DIR/philo" 7 1000 4 4 3 \
		|| fail "repeated schedule $iteration did not finish"
	check_log "$output"
	grep -q 'died' "$output" && fail "repeated schedule reported a death"
	check_progress "$output" 7 3
	iteration=$((iteration + 1))
done

iteration=0
while [ "$iteration" -lt 10 ]; do
	output="$TMP_DIR/death-$iteration.out"
	run_timeout 3 "$output" "$ROOT_DIR/philo" 5 60 80 10 \
		|| fail "death schedule $iteration did not finish"
	check_log "$output"
	check_terminal_line "$output"
	iteration=$((iteration + 1))
done

cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/log_terminal_race.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" -o "$TMP_DIR/log_terminal_race"
race_out="$TMP_DIR/log-terminal-race.out"
run_timeout 5 "$race_out" "$TMP_DIR/log_terminal_race" \
	|| fail 'terminal logging race did not finish'
check_log "$race_out"
check_terminal_line "$race_out"

printf 'concurrency: ok\n'
