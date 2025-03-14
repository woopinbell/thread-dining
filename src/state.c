#include "philo.h"

#include <stdio.h>

// 단순한 bool 하나를 읽는 것도 락 없이 하면, 다른 스레드가 써둔 값이 이 스레드에 언제 반영되어 보일지 보장되지 않음(가시성 문제)
// mutex로 감싸서 항상 최신 값을 읽도록 함
int	philo_has_ended(t_table *table)
{
	int	ended;

	pthread_mutex_lock(&table->state_mutex);
	ended = table->ended;
	pthread_mutex_unlock(&table->state_mutex);
	return (ended);
}

void	philo_finish(t_table *table)
{
	pthread_mutex_lock(&table->state_mutex);
	table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
}

void	philo_log(t_philo *philo, const char *message)
{
	t_table	*table;
	int64_t	timestamp;

	table = philo->table;
	pthread_mutex_lock(&table->print_mutex);
	if (!philo_has_ended(table))
	{
		// 시뮬레이션이 이미 끝난 뒤에 늦게 도착한 로그(예: 이미 죽은 철학자에 대한 "is eating")가 출력되는 걸 막기 위한 재확인
		timestamp = philo_now_ms() - table->start_ms;
		printf("%lld %d %s\n", (long long)timestamp, philo->id, message);
	}
	pthread_mutex_unlock(&table->print_mutex);
}

int	philo_try_log_death(t_philo *philo)
{
	t_table	*table;
	int64_t	now;
	int64_t	timestamp;
	int		should_print;

	table = philo->table;
	should_print = 0;
	timestamp = 0;
	// print_mutex를 먼저 잡고 그 안에서 state_mutex를 잡는 순서를 philo_log 등 다른 곳과 항상 동일하게 유지함
    // 두 mutex를 서로 다른 순서로 잡는 코드가 하나라도 있으면 데드락이 생길 수 있어, 전역적으로 락 순서를 통일해두는 것이 중요
	pthread_mutex_lock(&table->print_mutex);
	pthread_mutex_lock(&table->state_mutex);
	now = philo_now_ms();
	if (!table->ended
		&& now - philo->last_meal_ms >= table->config.time_to_die)
	{
		table->ended = 1;
		timestamp = now - table->start_ms;
		should_print = 1;
	}
	pthread_mutex_unlock(&table->state_mutex);
	if (should_print)
		printf("%lld %d died\n", (long long)timestamp, philo->id);
	// 사망 판정(ended=1)과 "died" 출력을 print_mutex 하나로 계속 감싸둔 채 진행함
    // 그래야 그 사이에 다른 철학자의 로그가 끼어들어 died 메시지보다 늦은 타임스탬프가 먼저 찍히는 순서 역전을 막을 수 있음
	pthread_mutex_unlock(&table->print_mutex);
	return (should_print);
}