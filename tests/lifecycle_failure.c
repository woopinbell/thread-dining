#include "philo.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int	g_create_calls;
static int	g_fail_create_at;
static int	g_join_calls;
static int	g_fail_join_at;
static int	g_destroy_calls;
static int	g_fail_destroy_at;

// 이 함수의 시그니처가 pthread_create와 한 글자도 다르지 않은 게 핵심
// smoke.sh에서 -Dpthread_create=test_pthread_create로 소스를 컴파일하면 src/run.c의 pthread_create 호출이
// 전처리 단계에서 전부 이 함수 호출로 바뀌기 때문에, 시그니처가 어긋나면 그 순간 컴파일이 깨짐
int	test_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		void *(*routine)(void *), void *arg)
{
	int	call;

	// "몇 번째 호출인지" 세어두고, 미리 정해둔 인덱스(g_fail_create_at)에 도달했을 때만 실패를 흉내냄
	// 나머지 호출은 그대로 실제 pthread_create로 위임하므로, "N번째 스레드 생성에서만 실패"하는 상황을 재현 가능하게 만들 수 있음.
    // 이 "몇 번째 호출을 실패시킬지"를 바꾸는 것만으로 여러 실패 시나리오를 하나의 가짜 함수로 커버함
	call = g_create_calls++;
	if (call == g_fail_create_at)
		return (EAGAIN);
	return (pthread_create(thread, attr, routine, arg));
}

// pthread_join도 마찬가지로 실전 시그니처와 동일하게 맞춰야 -Dpthread_join=test_pthread_join으로 링크가 성립함.
// g_fail_create_at과 완전히 같은 구조의 "몇 번째 호출을 실패시킬지" 카운터(g_fail_join_at)를 별도로 두고 있어서,
// 이 파일 안에서 "N번째 create가 실패하는 경우"와 "N번째 join이 실패하는 경우"를 서로 독립적으로 재현할 수 있음
int	test_pthread_join(pthread_t thread, void **result)
{
	int	call;
	int	status;

	call = g_join_calls++;
	if (call == g_fail_join_at)
		return (EINVAL);
	status = pthread_join(thread, result);
	if (status != 0)
		return (status);
	return (0);
}

// destroy 버전. 추가로 "같은 mutex를 두 번 세지 않았는지" 같은 별도 검증은 하지 않고 호출 횟수만 세는 단순한 형태
// init_failure.c에 나오는 (다른 파일의) test_mutex_destroy는 이중 파괴 검사까지 하는 더 엄격한 버전이라,
// 같은 이름의 가짜 함수라도 테스트 목적에 따라 검증 강도가 다르게 구현될 수 있음을 보여줌
int	test_mutex_destroy(pthread_mutex_t *mutex)
{
	int	call;

	call = g_destroy_calls++;
	if (call == g_fail_destroy_at)
		return (EBUSY);
	return (pthread_mutex_destroy(mutex));
}

static void	set_config(t_config *config)
{
	config->number = 3;
	config->time_to_die = 1000;
	config->time_to_eat = 2;
	config->time_to_sleep = 2;
	config->must_eat = 1;
	config->has_meal_limit = 1;
}

static int	create_failure_case(int fail_at)
{
	t_config	config;
	t_table		table;
	int			status;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	// 세 가짜 함수(create/join/destroy)가 전부 파일 전역 static 변수를 공유하기 때문에,
	// 이전 케이스에서 남은 호출 횟수/실패 지점이 이번 케이스로 새어 들어오지 않도록 매 케이스 시작 시 전부 초기화함.
	// -1은 "어떤 호출 인덱스와도 절대 일치하지 않는 값"으로 써서 그 seam을 이번 케이스에서는 비활성 상태로 둔다는 뜻
	g_create_calls = 0;
	g_fail_create_at = fail_at;
	g_join_calls = 0;
	g_fail_join_at = -1;
	g_fail_destroy_at = -1;
	status = philo_run(&table);
	// fail_at번째 pthread_create에서 실패했다면, 그 이전까지(정확히 fail_at개)만 스레드가 시작됐어야 하고,
	// run.c의 롤백 경로가 그 시작된 만큼만 정확히 join까지 마쳤어야 함
    // 이 네 조건이 롤백의 정확성을 검증하는 부분
	if (status != PHILO_ERR || table.threads_started != fail_at
		|| table.threads_joined != fail_at || g_join_calls != fail_at)
	{
		fprintf(stderr, "create failure at %d was not rolled back\n", fail_at);
		return (1);
	}
	if (philo_table_destroy(&table) != PHILO_OK)
		return (1);
	return (0);
}

static int	join_failure_case(int fail_at)
{
	t_config		config;
	t_table			table;
	pthread_mutex_t	*forks;
	int				before_destroy;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_create_calls = 0;
	g_fail_create_at = -1;
	g_join_calls = 0;
	g_fail_join_at = fail_at;
	g_fail_destroy_at = -1;
	if (philo_run(&table) != PHILO_UNSAFE || g_join_calls != config.number
		|| table.threads_joined != config.number - 1)
		return (1);
	forks = table.forks;
	before_destroy = g_destroy_calls;
	// join이 한 번이라도 실패하면 destroy는 UNSAFE를 반환하며 아무 자원도 실제로 해제하지 않아야 함
	// forks 포인터가 그대로고, fork_count가 줄지 않았고, destroy용 가짜 함수 호출 횟수도 그대로인 것으로 이를 확인
	if (philo_table_destroy(&table) != PHILO_UNSAFE || table.forks != forks
		|| table.fork_count != config.number
		|| g_destroy_calls != before_destroy)
	{
		fprintf(stderr, "unsafe table resources were released after join failure\n");
		return (1);
	}
	// test_pthread_join은 실패를 흉내낼 때 실제 pthread_join을 호출하지 않고 곧바로 리턴하므로,
	// fail_at번째 스레드는 seam 입장에서는 "실패"지만 OS 입장에서는 여전히 join 가능한 채로 살아있음
	// 테스트가 스스로 만들어낸 이 상태를 직접 정리해주지 않으면 테스트 프로세스보다 스레드가 더 오래 남을 수 있어,
	// 여기서 진짜 pthread_join으로 수동 수거함
	if (pthread_join(table.philos[fail_at].thread, NULL) != 0)
	{
		fprintf(stderr, "failed join did not leave a joinable worker\n");
		return (1);
	}
	// 방금 수동으로 정리했으니, 테이블의 안전 플래그도 손으로 맞춰줘야 destroy가 다시 정상적으로 동작함
	// 실제 코드에서는 join_started()가 이 두 값을 갱신하는데, 여기서는 그 갱신을 테스트가 직접 흉내내는 것
	table.destroy_safe = 1;
	table.threads_joined++;
	if (philo_table_destroy(&table) != PHILO_OK)
		return (1);
	return (0);
}

static int	destroy_failure_case(int fail_at, int remaining_forks)
{
	t_config	config;
	t_table		table;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_destroy_calls = 0;
	g_fail_destroy_at = fail_at;
	// 한 번은 seam을 켠 채로 destroy를 호출해 일부러 중간에 실패시키고,
	// fork_count(=아직 안 지워진 fork mutex 개수)가 remaining_forks로 정확히 남아있는지 확인
	if (philo_table_destroy(&table) != PHILO_ERR
		|| table.forks == NULL || table.fork_count != remaining_forks)
	{
		fprintf(stderr, "destroy failure at %d lost retryable state\n", fail_at);
		return (1);
	}
	// 이제 seam을 꺼서(-1) 다시 destroy를 호출
    // 앞서 실패로 중간에 멈춘 지점부터 이어서 나머지를 정리하고 이번에는 완전히 끝까지 끝나는지 확인
    // 즉 "실패 후 재시도가 안전한가"를 검증하는 흐름
	g_fail_destroy_at = -1;
	if (philo_table_destroy(&table) != PHILO_OK || table.forks != NULL)
		return (1);
	return (0);
}

int	main(void)
{
	if (create_failure_case(0) || create_failure_case(1)
		|| create_failure_case(2) || join_failure_case(0)
		|| join_failure_case(1) || destroy_failure_case(0, 3)
		|| destroy_failure_case(1, 2) || destroy_failure_case(3, 0)
		|| destroy_failure_case(4, 0))
		return (1);
	puts("lifecycle failure: ok");
	return (0);
}