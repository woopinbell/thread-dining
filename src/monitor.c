#include "philo.h"

#include <unistd.h>

static int	all_meals_done(t_table *table)
{
	return (table->config.has_meal_limit
		&& table->full_count >= table->config.number);
}

static t_philo	*find_dead_philo(t_table *table, int64_t now)
{
	int	i;

	i = 0;
	while (i < table->config.number)
	{
		if (now - table->philos[i].last_meal_ms >= table->config.time_to_die)
			return (&table->philos[i]);
		i++;
	}
	return (NULL);
}

void	philo_monitor(t_table *table)
{
	t_philo	*dead;
	int64_t	now;

	while (1)
	{
		now = philo_now_ms();
		pthread_mutex_lock(&table->state_mutex);
		if (table->ended)
		{
			pthread_mutex_unlock(&table->state_mutex);
			return ;
		}
		if (all_meals_done(table))
		{
			table->ended = 1;
			pthread_mutex_unlock(&table->state_mutex);
			return ;
		}
		dead = find_dead_philo(table, now);
		pthread_mutex_unlock(&table->state_mutex);
		// state_mutex는 여기서 먼저 풀어준 뒤 death 처리(출력 포함)를 진행
        // 출력까지 state_mutex를 쥔 채로 넘어가면 critical section이 길어져, 그동안 다른 철학자들이 last_meal_ms를 갱신하지 못해 식사 판정이 지연될 수 있음
		if (dead != NULL && philo_try_log_death(dead))
			return ;
		usleep(500);
		// 정확한 사망 시각에 즉시 반응하는 조건변수 방식 대신 500us 간격 폴링을 택함
        // 철학자마다 개별 타이머를 두는 복잡도 없이, 감지 지연을 500us 이내로 제한하면서 CPU 사용은 낮게 유지하는 절충
	}
}