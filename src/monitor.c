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
		if (dead != NULL && philo_try_log_death(dead))
			return ;
		usleep(500);
	}
}
