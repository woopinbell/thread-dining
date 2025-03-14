#include "philo.h"

static int	join_started(t_table *table, int count)
{
	int	i;
	int	status;

	i = 0;
	status = PHILO_OK;
	while (i < count)
	{
		if (pthread_join(table->philos[i].thread, NULL) == 0)
			table->threads_joined++;
		else
		{
			// join 실패 시 destroy_safe를 내림
            // 이후 philo_table_destroy가 "이 테이블은 아직 살아있는 스레드가 참조할 수 있어 안전하지 않다"고 판단해 자원 해제를 거부하게 만듦
			table->destroy_safe = 0;
			status = PHILO_UNSAFE;
		}
		i++;
	}
	return (status);
}


static int	release_start(t_table *table, int should_end)
{
	int		i;
	int		status;
	int64_t	start_ms;

	status = PHILO_OK;
	pthread_mutex_lock(&table->state_mutex);
	while (!should_end && table->ready_count < table->config.number)
	{
		if (pthread_cond_wait(&table->start_cond,
				&table->state_mutex) != 0)
		{
			table->run_error = 1;
			should_end = 1;
			status = PHILO_ERR;
		}
	}
	if (table->run_error)
	{
		should_end = 1;
		status = PHILO_ERR;
	}
	start_ms = philo_now_ms();
	table->start_ms = start_ms;
	i = 0;
	while (i < table->config.number)
	{
		// 각 철학자의 last_meal_ms를 스레드가 생성된 시점이 아니라, 전원이 동시에 풀려나는 지금 이 순간의 공통 시각(start_ms)으로 맞춤
        // 스레드 생성/스케줄링 시각이 제각각이면 사망 판정(time_to_die) 기준점이 철학자마다 달라져 버림
		table->philos[i].last_meal_ms = start_ms;
		i++;
	}
	if (should_end)
		table->ended = 1;
	table->start_released = 1;
	pthread_cond_broadcast(&table->start_cond);
	pthread_mutex_unlock(&table->state_mutex);
	return (status);
}

int	philo_run(t_table *table)
{
	int	i;
	int	join_status;

	i = 0;
	while (i < table->config.number)
	{
		if (pthread_create(&table->philos[i].thread, NULL, philo_routine,
				&table->philos[i]) != 0)
		{
			release_start(table, 1);
			// 이미 생성된 스레드들은 wait_for_start에서 barrier를 기다리며 블록된 상태
            // release_start(table, 1)로 강제로 풀어주지 않으면 이 스레드들이 영원히 대기하게 되고, 그러면 곧이어 호출할 join도 끝나지 않음
			join_status = join_started(table, table->threads_started);
			if (join_status != PHILO_OK)
				return (join_status);
			return (PHILO_ERR);
		}
		table->threads_started++;
		i++;
	}
	if (release_start(table, 0) != PHILO_OK)
	{
		join_status = join_started(table, table->threads_started);
		if (join_status != PHILO_OK)
			return (join_status);
		return (PHILO_ERR);
	}
	philo_monitor(table);
	join_status = join_started(table, table->threads_started);
	if (join_status != PHILO_OK)
		return (join_status);
	return (PHILO_OK);
}