#!/bin/sh

# smoke.sh는 `set -eu`였지만 여기는 `set -u`만 켬(-e는 없음)
# 이 스크립트는 probe 빌드 실패, probe 실행 실패, 프로젝트 빌드 실패 등 여러 단계에서 "실패했을 때 스크립트를 곧바로 죽일지,
# skip으로 처리할지"를 그때그때 스스로 판단해야 하기 때문. 
# set -e가 켜져 있으면 그 판단을 하기도 전에 쉘이 먼저 스크립트를 중단시켜버려서 아래 skip()/fail() 분기 로직 자체가 무의미해짐. 
# set -u(정의되지 않은 변수 참조 금지)만은 그대로 유지
set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP_DIR=$(mktemp -d)
# smoke.sh의 PHILO_BIN과 같은 "환경변수로 오버라이드, 없으면 기본값" 패턴.
# TSAN_CC: ThreadSanitizer(-fsanitize=thread)를 지원하는 컴파일러를 가리키게 할 수 있음(플랫폼에 따라 기본 cc가 TSan을 못 지원할 수 있어서,
# CI 등에서 clang 같은 다른 컴파일러를 지정할 여지를 열어둠)
TSAN_CC=${TSAN_CC:-cc}
# TSAN_REQUIRED: 이 환경에서 TSan이 꼭 동작해야 하는지(1=필수, 실패 시 test 자체를 fail)
# 아니면 안 되면 그냥 건너뛰어도 되는지(0=기본값)를 호출하는 쪽(CI vs 로컬)이 선택할 수 있게 해줌
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

# skip: "이 테스트를 실패로 칠 수 없는 사정(이 환경에 TSan이 없음/못 씀)이라 그냥 건너뛴다"는 뜻을 종료 코드 77로 표현함
# 이건 임의로 정한 숫자가 아니라 Automake 계열 테스트 스위트에서 널리 쓰는 관례로, 77을 받은 CI 러너는 이 테스트를 "실패"가 아니라 "스킵됨"으로 따로 집계함.
# 다만 TSAN_REQUIRED=1이면("이 환경에서는 TSan이 반드시 돼야 한다") 그 관용적 스킵을 거부하고 그냥 실패(exit 1) 처리함
skip()
{
	printf 'tsan: skipped (%s)\n' "$1" >&2
	if [ "$TSAN_REQUIRED" -eq 1 ]; then
		exit 1
	fi
	exit 77
}

# smoke.sh의 run_timeout과 기본 아이디어(백그라운드 실행 + 감시용 서브셸로 타임아웃 강제)는 동일함.
# 차이점만 짚으면: 여기서는 stdout/stderr를 따로 분리해서 받고(TSan은 레이스 리포트를 stderr에 씀),
# 스크립트 전체에 set -e가 없어서 set +e/set -e로 감쌀 필요도 없이 그냥 wait의 종료 코드를 받으면 됨
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

# ThreadSanitizer는 레이스를 감지하면 그 내용을 stderr에 "ThreadSanitizer"라는 문자열로 시작하는 리포트로 남김
# 이건 TSan 런타임 자체가 하는 일이라, 프로그램이 정상 종료(exit 0)해도 이 리포트만 남아있을 수 있음
# 그래서 종료 코드만 보고 통과시키면 안 되고, stderr에 이 표식이 있는지 항상 따로 확인해야 함
check_tsan_stderr()
{
	if grep -q 'ThreadSanitizer' "$1"; then
		cat "$1" >&2
		return 1
	fi
	return 0
}

# 실제 워크로드 하나를 "TSan 리포트가 있는지"까지 포함해서 통과/실패로 판정하는 래퍼.
# TSAN_OPTIONS는 이 스크립트가 아니라 TSan 런타임(컴파일된 바이너리 안에 들어있는 계측 코드)이 직접 읽는 환경변수
# halt_on_error=1은 레이스를 한 번이라도 감지하면 그 즉시 프로그램을 중단시키고(여러 개를 모아서 보여주지 않고 첫 번째에서 바로 끝냄),
# exitcode=66은 그 중단의 종료 코드를 66으로 고정함(0/1이나 타임아웃에 의한 강제종료와 헷갈리지 않게 구분해두는 값)
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
	# 종료 코드가 0이어도(halt_on_error에 안 걸렸어도) stderr에 리포트가 남아있을 수 있으니 마지막에 한 번 더 확인
	check_tsan_stderr "$stderr_file"
}

# smoke.sh의 check_log_format과 같은 "awk 스크립트의 종료 코드를 그대로 테스트 결과로 쓰는" 방식을 그대로 쓰되,
# 여기서는 형식 검사에 더해 "타임스탬프가 이전 줄보다 거꾸로 가지 않는지"까지 함께 검사함:
# previous에 이전 줄의 타임스탬프를 기억해두고, 그보다 작은 값이 나오면 bad=1. seen은 "로그가 한 줄이라도 있었는지"를 기억해서,
# 파일이 통째로 비어 있는 경우까지 실패로 잡아냄(`exit bad || !seen`)
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

# awk의 연관배열(meals[$2])을 써서 "철학자 id별로 is eating이 몇 번 찍혔는지"를 센 뒤,
# count(전체 철학자 수)만큼 id를 1부터 돌면서 target(기대 식사 횟수)에 못 미친 사람이 하나라도 있으면 실패.
# -v로 쉘 변수를 awk 변수로 넘기는 것도 여기서 처음 보이는 관용구(count="$2" target="$3")
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

# "died"가 정확히 한 번만 나왔는지, 그리고 그 died 줄 이후로 다른 로그가 더 찍히지 않았는지
# (terminal 이후엔 after=1이 세팅되고, 그 뒤로도 줄이 더 있으면 그 자체로 실패 조건)를 확인
# state.c에서 "사망 판정과 출력을 하나의 원자적 구간으로 묶어둔" 설계가 실제로 지켜지는지를 로그만 보고 검증하는 셈
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

# case 문을 "여러 분기 처리"가 아니라 단순 유효성 검사로 쓴 예: TSAN_REQUIRED가 0도 1도 아니면(오타 등) 즉시 fail
case "$TSAN_REQUIRED" in
	0|1)
		;;
	*)
		fail 'TSAN_REQUIRED must be 0 or 1'
		;;
esac

# <<'PROBE' ... PROBE : 쉘 heredoc, 이 사이의 모든 줄을 그대로(변수 치환 없이) 앞의 명령(cat > 파일)의 표준입력으로 넘겨서 파일로 씀.
# 구분자를 'PROBE'처럼 따옴표로 감싸면 안의 $나 백틱을 쉘이 해석하지 않고 글자 그대로 취급함
# 그래서 C 소스 코드를 별도 파일 없이 스크립트 안에 직접 박아넣을 수 있음.
# 이 probe.c는 정말 최소한의 "스레드 하나 만들고 join하고 값 하나 공유하는" 코드로, 이 프로젝트 코드와는 무관함
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

# "probe 먼저, 본 빌드는 나중" 원칙: 이 프로젝트를 통째로 -fsanitize=thread로 빌드하기 전에,
# 위의 아주 단순하고 이미 정답이 알려진 프로그램을 똑같은 플래그로 먼저 빌드/실행해봄.
# 이 probe조차 빌드나 실행이 안 된다면, 그건 이 프로젝트 코드의 문제가 아니라 지금 이 환경(툴체인, 플랫폼, TSan 런타임 라이브러리 부재 등)이
# ThreadSanitizer 자체를 못 돌리는 상황이라는 뜻, 그래서 fail이 아니라 skip으로 처리함
if ! "$TSAN_CC" -Wall -Wextra -Werror -pthread -fsanitize=thread -g \
	"$TMP_DIR/probe.c" -o "$TMP_DIR/tsan-probe" \
	>"$TMP_DIR/probe-build.out" 2>"$TMP_DIR/probe-build.err"; then
	cat "$TMP_DIR/probe-build.err" >&2
	skip "$TSAN_CC cannot build a ThreadSanitizer probe"
fi
# 컴파일러가 "성공했다"고 했는데도 실행 파일이 실제로 없거나 실행 권한이 없는 경우까지 한 번 더 확인(방어적 체크)
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

# probe까지 통과했으니 이제 진짜 프로젝트를 빌드
# 여기엔 -D로 바꿔치기하는 함수가 전혀 없음(smoke.sh와 정반대).
# src/*.c를 손대지 않고 그대로 전부 링크해서, 실제 프로그램 그 자체를 TSan 계측만 추가한 채로 만듦.
# 여기서부터 실패하면 그건 probe가 아니라 이 프로젝트 코드/빌드 자체의 문제이므로 skip이 아니라 fail
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

# 이 아래 세 워크로드는 모두 seam 없이 실제 바이너리를 진짜 인자로 돌리는 end-to-end 실행이고,
# TSan이 그 실행 전체(모든 메모리 접근)를 감시하는 동안 프로그램이 정상적으로 동작하는지를 각기 다른 각도로 확인함

# finite: 정해진 식사 횟수(4끼)를 다 채우고 아무도 죽지 않은 채 끝나야 하는 시나리오
run_case finite 7 1000 5 5 4 \
	|| fail 'finite schedule reported a race or runtime error'
check_log "$TMP_DIR/finite.out" finite
if grep -q 'died' "$TMP_DIR/finite.out"; then
	fail 'finite workload reported a death'
fi
check_progress "$TMP_DIR/finite.out" 7 4 finite

# death: time_to_die를 일부러 빡빡하게 잡아서(60ms) 누군가 반드시 죽도록 만든 시나리오
# 정확히 한 명만, 그리고 그 죽음 이후로는 로그가 더 안 찍혀야 함(check_death)
run_case death 5 60 80 10 \
	|| fail 'terminal schedule reported a race or runtime error'
check_log "$TMP_DIR/death.out" death
check_death "$TMP_DIR/death.out"

# contention: 철학자 수를 늘리고(17명) 시간 여유는 촉박하게 둬서 포크를 둘러싼 경쟁을 최대화한 시나리오
# 이만큼 많은 스레드가 몰려도 죽음 없이 목표 식사 횟수(3끼)까지 도달하는지 확인
run_case contention 17 2000 5 5 3 \
	|| fail 'contention schedule reported a race or runtime error'
check_log "$TMP_DIR/contention.out" contention
if grep -q 'died' "$TMP_DIR/contention.out"; then
	fail 'contention workload reported a death'
fi
check_progress "$TMP_DIR/contention.out" 17 3 contention

printf 'tsan: ok\n'