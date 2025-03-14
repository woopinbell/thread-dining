#include "philo.h"

#include <stdlib.h>

static void	assign_philos(t_table *table)
{
	int	i;

	i = 0;
	while (i < table->config.number)
	{
		table->philos[i].id = i + 1;
		table->philos[i].meals = 0;
		table->philos[i].last_meal_ms = 0;
		table->philos[i].left_fork = &table->forks[i];
		// (i + 1) % number로 마지막 철학자의 오른쪽 포크가 다시 0번째 포크를 가리키게 만든다.
        // 원형으로 배치된 테이블에서 인접한 철학자끼리 포크를 공유하는 구조가 여기서 만들어진다.
        // 이 공유 관계가 나중에 routine.c의 fork 획득 순서(데드락 회피)를 이해하는 전제가 된다.
		table->philos[i].right_fork = &table->forks[(i + 1) % table->config.number];
		table->philos[i].table = table;
		i++;
	}
}

static int	init_forks(t_table *table, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		if (pthread_mutex_init(&table->forks[i], NULL) != 0)
			return (PHILO_ERR);
		// 성공한 개수만큼만 증가시켜서, 초기화가 중간에 실패해도 destroy 쪽에서 실제로 만들어진 fork_count개만 정리하도록 한다.
		table->fork_count++;
		i++;
	}
	return (PHILO_OK);
}

int	philo_table_init(t_table *table, const t_config *config)
{
	table->config = *config;
	table->start_ms = 0;
	table->ended = 0;
	table->full_count = 0;
	table->fork_count = 0;
	table->state_ready = 0;
	table->start_cond_ready = 0;
	table->print_ready = 0;
	table->start_released = 0;
	table->ready_count = 0;
	table->run_error = 0;
	table->threads_started = 0;
	table->threads_joined = 0;
	table->destroy_safe = 1;
	table->forks = malloc(sizeof(*table->forks) * config->number);
	table->philos = malloc(sizeof(*table->philos) * config->number);
	if (table->forks == NULL || table->philos == NULL)
		return (philo_table_destroy(table), PHILO_ERR);
    // 각 단계가 성공할 때마다 해당 자원의 ready 플래그를 세우고, 이후 어느 단계에서든 실패하면 즉시 philo_table_destroy를 호출한다. 
    // destroy는 ready 플래그를 보고 "이미 만들어진 자원만" 정리하므로, 실패 시점이 어디든 상관없이 안전하게 부분 정리가 가능하다.
	if (pthread_mutex_init(&table->state_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->state_ready = 1;
	if (pthread_cond_init(&table->start_cond, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->start_cond_ready = 1;
	if (pthread_mutex_init(&table->print_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->print_ready = 1;
	if (init_forks(table, config->number) != PHILO_OK)
		return (philo_table_destroy(table), PHILO_ERR);
	assign_philos(table);
	return (PHILO_OK);
}

int	philo_table_destroy(t_table *table)
{
	if (table == NULL)
		return (PHILO_ERR);
	if (!table->destroy_safe || table->threads_joined < table->threads_started)
		return (PHILO_UNSAFE);
	// 시작된 스레드 수보다 join된 수가 적다는 건 아직 살아있을 수 있는 스레드가 이 mutex들을 참조 중일 수 있다는 뜻
    // 그 상태에서 mutex_destroy를 호출하면 undefined behavior이므로, 여기서 거부하고 호출자가 먼저 join을 끝내게 한다
	if (table->forks != NULL)
	{
		while (table->fork_count > 0)
		{
			if (pthread_mutex_destroy(
					&table->forks[table->fork_count - 1]) != 0)
				return (PHILO_ERR);
			table->fork_count--;
		}
	}
	if (table->print_ready)
	{
		if (pthread_mutex_destroy(&table->print_mutex) != 0)
			return (PHILO_ERR);
		table->print_ready = 0;
	}
	if (table->start_cond_ready)
	{
		if (pthread_cond_destroy(&table->start_cond) != 0)
			return (PHILO_ERR);
		table->start_cond_ready = 0;
	}
	if (table->state_ready)
	{
		if (pthread_mutex_destroy(&table->state_mutex) != 0)
			return (PHILO_ERR);
		table->state_ready = 0;
	}
	free(table->forks);
	free(table->philos);
	table->forks = NULL;
	table->philos = NULL;
	return (PHILO_OK);
}