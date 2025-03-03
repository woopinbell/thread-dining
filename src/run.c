#include "philo.h"

static void	join_started(t_table *table, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		pthread_join(table->philos[i].thread, NULL);
		i++;
	}
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

	i = 0;
	while (i < table->config.number)
	{
		if (pthread_create(&table->philos[i].thread, NULL, philo_routine,
				&table->philos[i]) != 0)
		{
			release_start(table, 1);
			join_started(table, i);
			return (PHILO_ERR);
		}
		i++;
	}
	if (release_start(table, 0) != PHILO_OK)
	{
		join_started(table, table->config.number);
		return (PHILO_ERR);
	}
	philo_monitor(table);
	join_started(table, table->config.number);
	return (PHILO_OK);
}
