#include "philo.h"

#include <pthread.h>
#include <stdio.h>

#define LOGGER_COUNT 12
#define LOGS_PER_LOGGER 200

// 지금까지 smoke.sh의 모든 테스트는 -D로 함수를 바꿔치기해서 "특정 시점에 정확히 이런 일이 일어난다"를 인위적으로 강제하는 방식
// 이 파일은 그런 seam이 하나도 없음(빌드 명령에도 -D가 없음, 아래 참고)
// init.c/state.c/time.c를 원본 그대로 링크해서, 진짜 여러 스레드가 동시에 같은 자원(philo_log 안의 print_mutex, 그리고 한 철학자의 state)
// 에 부딪히게 만들어 데이터 레이스가 "실제로" 드러나길 기다리는 방식임.
// 즉 정답을 미리 정해두고 그 경로를 강제로 타게 하는 게 아니라, 확률적으로만 재현되는 버그를 스케줄러가 최대한 자주 그 상황에 놓이도록 부하를 몰아서 잡아내려는 시도

static t_table			g_table;
// 이 두 개는 pthread_create/join과 무관하게, 이 테스트 파일이 스스로 만든 "출발 신호" 용도의 동기화 도구
// routine.c/run.c에 있던 wait_for_start/release_start와 개념은 똑같은 barrier 패턴이지만, seam 없이 순수 pthread 호출만으로 이 파일 안에서 새로 하나 만든 것
// 목적은 오직 하나: LOGGER_COUNT개의 스레드가 최대한 같은 순간에 philo_log를 두드리게 만들어서 락 경쟁을 가장 세게 만드는 것
static pthread_mutex_t	g_gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	g_gate_cond = PTHREAD_COND_INITIALIZER;
static int				g_ready;
static int				g_go;

static void	*log_states(void *arg)
{
	t_philo	*philo;
	int		i;

	philo = (t_philo *)arg;
	pthread_mutex_lock(&g_gate_mutex);
	g_ready++;
	pthread_cond_broadcast(&g_gate_cond);
	// g_go가 세워질 때까지 대기
    // main이 LOGGER_COUNT개 스레드가 모두 여기 도착한 걸 확인하고서야 g_go=1을 세우므로,
	// 아래 while문을 빠져나온 시점에는 12개 스레드가 거의 동시에 풀려나 있게 됨
	while (!g_go)
		pthread_cond_wait(&g_gate_cond, &g_gate_mutex);
	pthread_mutex_unlock(&g_gate_mutex);
	i = 0;
	while (i < LOGS_PER_LOGGER)
	{
		// 12개 스레드가 전부 "같은" 철학자 포인터(philo)를 넘겨받았다는 점이 핵심
        // 실제 프로그램에서는 철학자마다 자기 자신의 t_philo를 갖지만, 
        // 이 테스트는 일부러 하나의 t_philo/print_mutex에 최대한 많은 스레드를 몰아붙여서 philo_log 내부 락킹의 약점을 드러내려는 것
        // (실전 코드 스타일이 아니라 테스트만을 위한 인위적인 부하 집중 트릭)
		philo_log(philo, "is thinking");
		i++;
	}
	return (NULL);
}

static void	set_config(t_config *config)
{
	config->number = 1;
	config->time_to_die = 1;
	config->time_to_eat = 1;
	config->time_to_sleep = 1;
	config->must_eat = 0;
	config->has_meal_limit = 0;
}

int	main(void)
{
	pthread_t	threads[LOGGER_COUNT];
	t_config	config;
	int			started;
	int			i;

	set_config(&config);
	if (philo_table_init(&g_table, &config) != PHILO_OK)
		return (1);
	g_table.start_ms = philo_now_ms() - 100;
	g_table.philos[0].last_meal_ms = g_table.start_ms;
	started = 0;
	while (started < LOGGER_COUNT)
	{
		if (pthread_create(&threads[started], NULL, log_states,
				&g_table.philos[0]) != 0)
			break ;
		started++;
	}
	// 여기서부터 main 스레드가 직접 barrier의 "풀어주는 쪽" 역할을 함: 모든 로거가 준비됐다고 알려올 때까지
	// (g_ready == started) 기다린 뒤, g_go를 세우고 broadcast해서 한 번에 전부 깨움
	pthread_mutex_lock(&g_gate_mutex);
	while (g_ready < started)
		pthread_cond_wait(&g_gate_cond, &g_gate_mutex);
	g_go = 1;
	pthread_cond_broadcast(&g_gate_cond);
	pthread_mutex_unlock(&g_gate_mutex);
	// 12개의 로거를 풀어준 바로 그 순간, main 스레드도 똑같은 철학자에 대해 death 로그를 시도함
	// 이게 이 테스트가 실제로 노리는 경쟁 상황: "is thinking"을 계속 찍어대는 여러 스레드와
    // "died"를 한 번 찍으려는 스레드가 동시에 philo_log/philo_try_log_death의 락을 다툼.
	// 여기서 만약 세팅 자체가 잘못돼 death가 조기에 실패하면, 그건 이 테스트의 버그가 아니라 "테스트를 제대로 시작조차 못한" 상황이므로 
    // 실패로 간주하지 않고 philo_finish로 강제 종료만 시켜 나머지 로거들이 빨리 끝나게 함
	if (started != LOGGER_COUNT || !philo_try_log_death(&g_table.philos[0]))
	{
		fprintf(stderr, "terminal log race could not be started\n");
		philo_finish(&g_table);
	}
	i = 0;
	while (i < started)
	{
		pthread_join(threads[i], NULL);
		i++;
	}
	if (started != LOGGER_COUNT || philo_table_destroy(&g_table) != PHILO_OK)
		return (1);
	return (0);
}
// 이 파일 자체는 "died"와 "is thinking" 줄들이 뒤섞여 표준출력으로 쏟아지게 만드는 역할만 하고, 
// 그 출력이 실제로 올바른지(타임스탬프가 거꾸로 가지 않는지, died가 정확히 마지막 한 줄로만 나오는지)는 검증하지 않음
// 그 검증은 이 프로그램을 실행하는 concurrency.sh 쪽에서 awk로 따로 확인함
// (-D 없이 단순히 `cc ... tests/log_terminal_race.c src/init.c src/state.c src/time.c -o ...` 로 빌드해서 그대로 실행)
// 즉 이 .c 파일은 "레이스가 일어나기 쉬운 상황을 만들어내는 부하 생성기" 역할, 옳고 그름을 판정하는 건 쉘 스크립트 쪽 역할