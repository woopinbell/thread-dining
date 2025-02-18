# thread-dining

`thread-dining`은 식사하는 철학자 문제를 C와 POSIX thread로 구현하는
프로젝트다. 각 철학자는 독립된 worker로 동작하고 공유 포크의 소유권은
동기화 객체로 표현한다.

## 목표

- 정해진 CLI 입력을 검증해 실행 설정으로 변환한다.
- 철학자의 식사·수면·사고 흐름을 서로 독립된 worker로 실행한다.
- 사망 또는 선택적인 식사 횟수 완료 조건에서 전체 실행을 종료한다.
- 생성한 thread와 동기화 자원의 수명을 명시적으로 관리한다.

## 예정 실행 계약

```text
./philo number_of_philosophers time_to_die time_to_eat time_to_sleep [number_of_times_each_philosopher_must_eat]
```

시간 단위는 밀리초다. 실행 파일 이름은 `philo`로 고정하고 bonus target은
제공하지 않는다.

## 개발 규약

- 언어는 C를 사용하고 POSIX thread API에 의존한다.
- 기본 compiler 경고는 `-Wall -Wextra -Werror`를 적용한다.
- thread 지원을 위해 compile과 link에 `-pthread`를 사용한다.
- public 선언은 `include/`, 구현은 `src/`, 검증 코드는 `tests/`에 둔다.
- 생성된 executable과 object는 Git에 포함하지 않는다.
- 변경은 하나의 책임 단위로 나누고 각 단계가 독립적으로 build되게 한다.

## 검증 원칙

초기에는 compiler 경고 없는 clean build와 대표 CLI 입력을 확인한다. 동시성
상태가 추가되면 정상 종료, 실패 rollback, 로그 형식과 경쟁 조건을 별도의
검증 경로로 확장한다.
