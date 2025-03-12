# Thread dining

![Language](https://img.shields.io/badge/language-C-blue?logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/platform-POSIX-lightgrey)

`thread-dining`은 42 `philo` 과제를 변형한 C 프로젝트입니다. POSIX thread와 mutex를 사용해 식사하는 철학자 문제를 구현하고, thread 수명과 공유 포크의 소유권을 검증합니다.

## 실행

```sh
./build/bin/philo number_of_philosophers time_to_die time_to_eat time_to_sleep [number_of_times_each_philosopher_must_eat]
```

모든 시간 단위는 밀리초입니다.

예시:

```sh
./build/bin/philo 5 800 200 200
./build/bin/philo 5 800 200 200 3
```

프로그램은 각 철학자의 포크 획득, 식사, 수면, 사고와 사망을 표준 출력에 기록합니다. 선택적인 식사 횟수를 지정하면 모든 철학자가 해당 횟수를 완료한 뒤 종료합니다.

## 빌드

저장소 루트에서 실행합니다.

```sh
make
```

실행 파일은 `build/bin/philo`, 오브젝트와 dependency 파일은 `build/obj/`에 생성됩니다. bonus target은 제공하지 않습니다.

## 테스트

기본 테스트는 대표적인 정상 실행, 사망 조건, CLI 경계값, mutex 초기화 실패, monotonic clock, thread 시작 장벽과 terminal log race를 검증합니다.

```sh
make test
```

ThreadSanitizer 검증은 환경에서 지원되는 경우 다음 명령으로 실행합니다.

```sh
make test-tsan
```

TSAN을 반드시 사용할 수 있어야 하는 환경에서는 다음과 같이 실행합니다.

```sh
make test-tsan TSAN_REQUIRED=1
```

## 정리

```sh
make clean  # build/ 및 테스트 캐시 삭제
make fclean # clean과 동일
make re     # fclean 후 전체 재빌드
```

빌드 산출물과 테스트 캐시는 저장소에 포함하지 않습니다.
