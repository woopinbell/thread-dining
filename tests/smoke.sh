#!/bin/sh

# set -e: 어떤 명령이든 실패(0이 아닌 종료 코드)하면 스크립트 전체를 즉시 중단.
# set -u: 정의되지 않은 변수를 참조하면 에러로 처리, 오타로 빈 문자열이 조용히 쓰이는 걸 방지.
# 이 둘을 켜두는 것이 쉘 테스트 스크립트에서는 거의 관례임(안 켜두면 중간 실패를 놓치고 계속 진행해버림)
set -eu

# 스크립트가 어디서 호출되든(상대경로 실행, 다른 디렉토리에서 실행 등) 프로젝트 루트의 절대경로를 안정적으로 구하는 관용구.
# dirname -- "$0" : 이 스크립트 파일이 들어있는 디렉토리, 그 상위(..)가 프로젝트 루트
# CDPATH= : 사용자가 CDPATH를 설정해둔 경우 cd가 예상치 못한 경로를 출력하는 걸 방지
# cd ... && pwd : 그 디렉토리로 실제로 이동해서 pwd로 절대경로를 얻어냄(상대경로/심볼릭 링크 문제를 제거)
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# ${VAR:-default} : PHILO_BIN 환경변수가 이미 설정돼 있으면 그 값을, 없으면 기본 경로를 사용, CI 등에서 테스트할 바이너리를 바꿔 끼울 수 있게 해줌
PHILO_BIN=${PHILO_BIN:-"$ROOT_DIR/build/bin/philo"}
# 이 실행에서만 쓰는 임시 디렉토리, 빌드 산출물/출력 로그를 여기 모아두고 끝나면 통째로 지움
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
	# awk를 로그 검증기로 씀: 첫 번째 패턴(정규식)에 맞는 줄은 next로 넘어가고,
	# 그 패턴에 안 걸린 줄은 두 번째 규칙(패턴 없음 = 모든 줄에 매칭)에서 bad=1이 세팅됨.
	# END에서 bad를 그대로 awk의 종료 코드로 만들어, 잘못된 형식의 줄이 하나라도 있으면 awk 자체가 실패 종료하게 함
	awk '
		/^[0-9]+ [1-9][0-9]* (has taken a fork|is eating|is sleeping|is thinking|died)$/ { next }
		{ bad = 1 }
		END { exit bad }
	' "$1" || fail "bad log format in $1"
}

# POSIX sh에는 GNU coreutils의 timeout(1) 같은 명령이 기본으로 없다는 전제 하에,
# 잡 컨트롤(백그라운드 실행 + 타이머 감시)만으로 "이 명령을 limit초 안에 못 끝내면 죽인다"를 직접 구현한 것
run_timeout()
{
	limit=$1
	outfile=$2
	shift 2
	# "$@"를 백그라운드(&)로 실행하고 $!로 그 프로세스의 PID를 기억
	"$@" >"$outfile" 2>&1 &
	pid=$!
	# 감시용 서브셸도 별도로 백그라운드 실행: limit초 자고 일어나서, 대상이 아직 살아있으면 SIGTERM으로 종료시킴
	(
		sleep "$limit"
		kill -TERM "$pid" 2>/dev/null || true
	) &
	guard=$!
	# wait "$pid"가 0이 아닌 상태로 끝나도(타임아웃으로 죽었거나 프로그램이 실패했거나) 스크립트가 곧바로 중단되지 않도록
	# 이 구간만 set +e로 잠깐 끄고, 상태 코드를 직접 받아서 처리한 뒤 다시 set -e로 복구
	set +e
	wait "$pid"
	status=$?
	set -e
	# 대상이 제시간에 끝났다면 감시용 서브셸은 아직 sleep 중일 수 있으므로 직접 정리(안 하면 좀비처럼 남아있을 수 있음)
	kill "$guard" 2>/dev/null || true
	wait "$guard" 2>/dev/null || true
	return "$status"
}

# 이 스크립트가 정상 종료/Ctrl-C/kill 중 무엇으로 끝나든 TMP_DIR을 반드시 지우게 함, 객체 소멸자 대신 쉘이 쓰는 방식의 자원 정리
trap cleanup EXIT INT TERM

make -C "$ROOT_DIR" >/dev/null

# ============================================================================
# 이 아래로 반복되는 핵심 기법: "링크 시점 함수 치환(seam)"으로 실패나 특정 상황을 강제로 만드는 단위 테스트.
#
# -Dpthread_mutex_init=test_mutex_init : 컴파일러 전처리 단계에서 지정한 소스 파일 안의 모든 `pthread_mutex_init` 토큰을
# `test_mutex_init`으로 그대로 바꿔치기함. 원본 .c는 한 글자도 건드리지 않고, 이 한 번의 컴파일 명령에서만 동작이 바뀜(같은 init.c라도 -D 없이 컴파일하면 평범하게 진짜 함수를 호출함).
# tests/init_failure.c가 정확히 같은 함수 시그니처로 `test_mutex_init`을 정의해두었기 때문에 링크가 그대로 성립함
# 그래서 이렇게 바꿔치기할 함수는 항상 원본과 시그니처(매개변수/리턴 타입)가 동일해야 컴파일이 통과함.
# 이 때문에 init.c는 Makefile이 만드는 평범한 object가 아니라, 이 -D가 적용된 전용 object(아래 *_init.o, *_routine.o 등)로 이 스크립트가 매번 별도로 다시 컴파일함
# 아래 반복되는 각 블록마다 "어떤 함수를 바꿔치기해서 무엇을 확인하는지"만 짚고, 이 -D/재컴파일/링크의 기본 구조 자체는 다시 설명하지 않음.
# ============================================================================

# [1] pthread_mutex_init을 실패시켜, philo_table_init 중간에 mutex 하나가 초기화 실패했을 때 이미 만들어진 자원만 정확히 롤백(destroy)하는지 확인
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dpthread_mutex_init=test_mutex_init \
	-Dpthread_mutex_destroy=test_mutex_destroy \
	-c "$ROOT_DIR/src/init.c" -o "$TMP_DIR/init_failure_init.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/init_failure.c" "$TMP_DIR/init_failure_init.o" \
	-o "$TMP_DIR/init_failure"
"$TMP_DIR/init_failure" || fail 'partial mutex initialization cleanup failed'

# [2] clock_gettime을 바꿔치기, pthread 함수가 아니라 표준 라이브러리 함수도 같은 방식으로 치환할 수 있음을 보여줌.
# philo_now_ms가 항상 CLOCK_MONOTONIC으로 호출하는지, clock_gettime 자체가 실패하면 프로세스를 즉시 종료시키는지 확인
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dclock_gettime=test_clock_gettime \
	-c "$ROOT_DIR/src/time.c" -o "$TMP_DIR/monotonic_time.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/monotonic_clock.c" "$TMP_DIR/monotonic_time.o" \
	-o "$TMP_DIR/monotonic_clock"
"$TMP_DIR/monotonic_clock" || fail 'elapsed time did not use a monotonic clock'

# [3] pthread_create를 바꿔치기, 이번엔 "실패를 주입"하는 게 아니라 "실제 생성은 그대로 하되 타이밍을 조작"하는 용도로 씀
# (지연 스레드 하나를 끼워넣어, 모든 철학자가 barrier에서 정말 동시에 풀려나는지 검증). 같은 -D 기법이 실패 주입과
# 훅(관찰/조작) 두 가지 다른 목적으로 쓰일 수 있다는 걸 보여주는 예.
# run.c만 이 -D로 다시 컴파일하고, 나머지(init/monitor/routine/state/time)는 원본 그대로 링크에 포함시킴
# 이 테스트가 건드리려는 지점이 run.c 하나뿐이기 때문에, 나머지 모듈은 손댈 필요가 없음
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

# [4] pthread_cond_wait을 바꿔치기해 조건변수 대기 중 실패를 한 번 주입
# 워커 스레드가 barrier에서 깨어나길 기다리다가 실패하면 run_error를 세우고 안전하게 종료로 이어지는지 확인.
# 이 테스트는 실제로 스레드가 블로킹된 채 실행되므로, 버그가 있으면 영원히 멈출 수 있어 run_timeout으로 감싸서 
# 스크립트 자체가 함께 멈추는 걸 방지함(다른 블록들과 달리 "$TMP_DIR/xxx" 를 직접 실행하지 않고 run_timeout을 거치는 이유)
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
# 종료 코드가 0인 것만으로는 "끝까지 정상 흐름을 탔다"는 걸 완전히 보장하지 못하므로,
# 테스트 프로그램이 성공 시 마지막에 찍는 고유한 문자열을 grep으로 다시 한번 확인하는 관용구(이 스크립트 전체에서 자주 반복됨)
grep -q 'worker wait failure: ok' "$TMP_DIR/worker_wait_failure.out" \
	|| fail 'worker condition wait failure test did not finish'

# [5] pthread_mutex_unlock을 바꿔치기, [3]과 같은 "훅" 방식이지만 훨씬 더 미세한 지점(락이 풀리는 그 순간)에 걸어서,
# "상태를 다 갱신하고 락을 놓은 뒤에" 다른 스레드가 끼어드는 경쟁 상황을 인위적으로 만들어 재현함
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

# [6] philo_sleep_ms를 바꿔치기, 지금까지는 pthread/libc 함수를 바꿨는데, 여기서는 이 프로젝트 "자기 자신의" 함수를 치환함.
# 호출하는 쪽(routine.c)이 이름으로만 함수를 부르고 있으면 프로젝트 내부 함수도 똑같이 -D로 갈아치울 수 있다는 걸 보여줌.
# "먹는 도중 자던 게 중간에 실패로 끝나면 식사 카운트가 올라가지 않아야 한다"를 확인
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

# [7] [6]과 똑같이 philo_sleep_ms를 바꿔치기하지만 테스트 목적이 다름(식사 횟수가 INT_MAX를 넘어도 안전하게 올라가는지 확인)
# 그래서 -D는 동일해도 결과 object 파일 이름과 링크되는 테스트 .c가 다름. 
# 같은 함수를 여러 테스트가 각자 다른 목적으로 재사용해도 된다는 걸 보여줌(각 테스트는 자기만의 전용 object를 새로 만듦)
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	-Dphilo_sleep_ms=test_philo_sleep_ms \
	-c "$ROOT_DIR/src/routine.c" -o "$TMP_DIR/meal_counter_range_routine.o"
cc -Wall -Wextra -Werror -pthread -I"$ROOT_DIR/include" \
	"$ROOT_DIR/tests/meal_counter_range.c" \
	"$ROOT_DIR/src/init.c" \
	"$ROOT_DIR/src/state.c" \
	"$ROOT_DIR/src/time.c" \
	"$TMP_DIR/meal_counter_range_routine.o" \
	-o "$TMP_DIR/meal_counter_range"
"$TMP_DIR/meal_counter_range" >"$TMP_DIR/meal_counter_range.out" \
	|| fail 'meal counter did not advance beyond INT_MAX'

# [8] -D를 두 개 동시에 걸어서 같은 파일(run.c) 안의 서로 다른 두 함수(create/join)를 한꺼번에 치환하고,
# init.c 쪽도 또 다른 -D(mutex_destroy)를 걸어 별도 전용 object로 컴파일
# 이렇게 나온 두 개의 특수 object를 나머지 평범한 .c들과 함께 링크함.
# 한 테스트가 여러 지점(스레드 생성/조인/자원 파괴)을 동시에 흔들어야 할 때는 이렇게 여러 -D와 여러 전용 object를 조합해서 쓴다는 예
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

# [9] 지금까지는 pthread/프로젝트 함수 "하나"를 가짜로 바꿔서 그 내부 동작만 흔드는 방식이었는데,
# 여기서는 main.c가 호출하는 네 함수(parse_args/table_init/run/table_destroy) 전체를 통째로 스텁으로 갈아치움
# main()의 "이 네 단계를 어떤 순서/조건으로 호출하고, 실패를 어떻게 다루는지" 그 흐름 자체를 검증 대상으로 격리하기 위함.
# 즉 지금까지의 [1]~[8]은 "낮은 층위 함수 하나를 흔들어 그 위 로직을 시험"했고, [9]는 "가장 위 층위(main)의
# 오케스트레이션 로직을 시험"하는 반대 방향의 같은 기법
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

# ============================================================================
# 여기부터는 함수 치환(-D)이 전혀 없음, 실제로 빌드된 바이너리($PHILO_BIN)를 그대로 실행해서 표준출력/표준에러만 보고 검증하는 블랙박스 테스트로 전환됨.
# 지금까지의 "내부 함수를 조작해서 특정 경로를 강제로 타게 만드는" 방식과 달리, 여기서부터는 프로그램을 일반적인 방식으로 실행하고 결과만 관찰함
# ============================================================================
invalid_out="$TMP_DIR/invalid.out"
if "$PHILO_BIN" 0 100 10 10 >"$invalid_out" 2>&1; then
	fail 'invalid philosopher count succeeded'
fi
grep -q 'Usage: ./philo' "$invalid_out" || fail 'invalid args did not print usage'

overflow_out="$TMP_DIR/overflow.out"
if "$PHILO_BIN" 2 999999999999999999999 10 10 >"$overflow_out" 2>&1; then
	fail 'overflow argument succeeded'
fi

single_out="$TMP_DIR/single.out"
run_timeout 2 "$single_out" "$PHILO_BIN" 1 80 40 40 \
	|| fail 'single philosopher did not exit cleanly'
check_log_format "$single_out"
grep -q '1 has taken a fork' "$single_out" || fail 'single philosopher missed fork log'
grep -q '1 died' "$single_out" || fail 'single philosopher missed death log'

finite_out="$TMP_DIR/finite.out"
run_timeout 3 "$finite_out" "$PHILO_BIN" 2 250 50 50 2 \
	|| fail 'finite meal run did not exit cleanly'
check_log_format "$finite_out"
grep -q 'died' "$finite_out" && fail 'finite meal run had a death'
# grep -c는 매칭 개수가 0이어도 종료 코드는 실패(1)로 줌, set -e 아래서 그대로 두면 스크립트가 중단되므로
# `|| true`로 그 실패를 무시하고, 출력된 개수(문자열 "0" 포함)만 변수에 담음
eat_count=$(grep -c 'is eating' "$finite_out" || true)
[ "$eat_count" -ge 4 ] || fail 'finite meal run did not eat enough'

nodeath_out="$TMP_DIR/nodeath.out"
run_timeout 5 "$nodeath_out" "$PHILO_BIN" 5 800 100 100 3 \
	|| fail 'no-death meal run did not exit cleanly'
check_log_format "$nodeath_out"
grep -q 'died' "$nodeath_out" && fail 'no-death meal run had a death'
eat_count=$(grep -c 'is eating' "$nodeath_out" || true)
[ "$eat_count" -ge 15 ] || fail 'no-death meal run did not reach meal count'

printf 'smoke: ok\n'