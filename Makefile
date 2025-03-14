NAME := build/bin/philo
BIN_DIR := build/bin
OBJ_DIR := build/obj

CC := cc
# -MMD -MP: 컴파일할 때마다 오브젝트 파일과 함께 "이 .c가 어떤 헤더들에 의존하는지" 적은 .d 파일도 만들어달라는 옵션
# -MMD는 시스템 헤더는 빼고 프로젝트 헤더만 추적하고, -MP는 그 헤더가 나중에 지워졌을 때 make가 에러를 내지 않도록 가짜 타겟을 함께 추가
# 이 .d 파일들은 맨 아래 -include $(OBJS:.o=.d)에서 다시 읽혀서, philo.h 하나만 고쳐도 그걸 include하는 모든 .c가 자동으로 재컴파일 대상이 되게 만드는 용도
CFLAGS := -Wall -Wextra -Werror -pthread -Iinclude -MMD -MP
# ?= : TSAN_CC/TSAN_REQUIRED가 이미 환경변수나 `make TSAN_CC=... test-tsan` 같은 커맨드라인 인자로 넘어와 있으면 그 값을 그대로 쓰고, 없을 때만 여기 기본값을 대입
# 쉘의 ${VAR:-default}와 같은 목적으로 아래 test-tsan 타겟에서 이 값을 그대로 tests/tsan.sh의 같은 이름 환경변수로 넘겨줌
TSAN_CC ?= $(CC)
TSAN_REQUIRED ?= 0

# wildcard: src/ 아래 있는 .c 파일들을 셸의 글롭처럼 그때그때 실제로 디스크를 뒤져서 목록으로 만들어줌
# 새 .c 파일을 추가해도 이 Makefile 자체는 고칠 필요가 없는 이유
SRC := $(wildcard src/*.c)
# patsubst: SRC의 각 경로에서 "src/%.c" 패턴의 %에 해당하는 부분만 뽑아 "$(OBJ_DIR)/%.o"로 다시 조립함
# 즉 src/init.c -> build/obj/init.o 처럼, 소스 목록을 그에 대응하는 오브젝트 목록으로 규칙적으로 바꿔줌
OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRC))

# .PHONY: 여기 나열된 이름들은 실제 파일이 아니라 "항상 다시 실행해야 하는 명령의 이름"이라고 make에게 알려주는 선언.
# 이걸 안 해두면, 만약 현재 디렉토리에 우연히 "clean"이라는 이름의 파일이 생기면 make가
# 그걸 이미 만들어진 최신 결과물로 오해해서 clean 타겟 실행을 그냥 건너뛰어버릴 수 있음
.PHONY: all bonus clean fclean re test test-tsan

all: $(NAME)

# 콜론 뒤는 "이 타겟을 만들기 전에 먼저 준비돼 있어야 하는 것들"의 목록.
# | 뒤의 $(BIN_DIR)은 order-only 의존성  "실행 전에 존재만 하면 된다"는 뜻으로, $(BIN_DIR)의 타임스탬프가
# 갱신됐다고 해서 그것 때문에 $(NAME)을 다시 링크하지는 않음(디렉토리는 매번 mkdir -p해도 무해하지만,
# 그걸 이유로 매번 다시 링크하는 건 낭비라서 이렇게 일반 의존성과 구분해둔 것)
$(NAME): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@

# %는 패턴 규칙
# "build/obj/무언가.o를 만들려면 src/그무언가.c가 있어야 한다"를 한 줄로 표현해서 src 아래 모든 .c에 재사용함.
# $< 는 의존성 목록 중 첫 번째 항목(=짝이 되는 그 .c 파일), $@ 는 지금 만들고 있는 타겟(그 .o 파일) 자신을 가리키는 자동 변수라,
# 파일마다 규칙을 따로 안 써도 됨.
# include/philo.h도 의존성에 넣어둬서, 헤더가 바뀌면 이 규칙에 걸리는 모든 오브젝트가 재컴파일 대상이 됨
# (더 세밀한 헤더 의존성은 위 -MMD/-MP가 만드는 .d 파일들이 보강해줌)
$(OBJ_DIR)/%.o: src/%.c include/philo.h | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# 42 계열 프로젝트에서 보너스 파트를 제공하지 않을 때 관례적으로 남겨두는 자리  
make bonus를 실행하면
# 항상 메시지만 찍고 실패(exit 1)로 끝나게 해둔 것. @는 이 명령 자체를 실행하기 전에 그대로 화면에
# 에코하지 않게 하는 접두어(안 붙이면 make가 실행할 명령 문구를 그대로 한 번 더 출력함)
bonus:
	@printf 'bonus target is unavailable\n'
	@exit 1

clean:
	rm -rf build tests/__pycache__ .pytest_cache

# fclean은 지금 clean과 똑같은 일만 하지만 이름을 따로 둔 건 42 관례상 "fclean = clean이 지우는 것 +
# 최종 산출물까지 전부 지운다"는 의미로 구분해서 쓰기 때문 
 여기서는 clean이 이미 build 전체를 지우므로
# fclean에 추가로 더 지울 게 없어서 몸통이 비어 있음
fclean: clean

re: fclean all

# test: all  
smoke.sh/concurrency.sh를 돌리기 전에 먼저 all(바이너리 빌드)이 끝나 있어야 한다는 의존성.
# PHILO_BIN="$(CURDIR)/$(NAME)" 로 방금 빌드한 바이너리의 절대경로(CURDIR = make가 실행된 디렉토리)를
# 두 스크립트의 환경변수로 넘겨줌 
 smoke.sh 안의 ${PHILO_BIN:-...} 기본값 대신 이 값이 쓰이게 됨
test: all
	PHILO_BIN="$(CURDIR)/$(NAME)" ./tests/smoke.sh
	PHILO_BIN="$(CURDIR)/$(NAME)" ./tests/concurrency.sh

# test-tsan은 일부러 all에 의존하지 않음
  tsan.sh가 자기 안에서 -fsanitize=thread를 붙여 소스를 통째로
# 별도 빌드하기 때문에(philo-tsan), all이 만드는 일반 빌드 결과는 여기서 재사용되지 않음.
# TSAN_CC/TSAN_REQUIRED도 위에서 정의한 make 변수를 그대로 같은 이름의 환경변수로 넘겨서, tsan.sh의
# ${TSAN_CC:-cc} / ${TSAN_REQUIRED:-0} 기본값 대신 이 값이 쓰이게 함
test-tsan:
	TSAN_CC="$(TSAN_CC)" TSAN_REQUIRED="$(TSAN_REQUIRED)" ./tests/tsan.sh

# 각 오브젝트가 실제로 어떤 헤더에 의존하는지 적힌 .d 파일들을 다시 이 Makefile 안으로 읽어들임
 
# 첫 빌드 전에는 .d 파일이 아직 하나도 없어서 make가 이 include 대상을 못 찾는데, 그때 에러를 내지
# 않도록 보통의 include가 아니라 앞에 -를 붙인 -include를 씀(못 찾으면 조용히 무시)
-include $(OBJS:.o=.d)