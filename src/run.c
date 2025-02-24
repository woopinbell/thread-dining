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

int	philo_run(t_table *table)
{
	int	i;

	table->start_ms = philo_now_ms();
	i = 0;
	while (i < table->config.number)
	{
		pthread_mutex_lock(&table->state_mutex);
		table->philos[i].last_meal_ms = table->start_ms;
		pthread_mutex_unlock(&table->state_mutex);
		if (pthread_create(&table->philos[i].thread, NULL, philo_routine,
				&table->philos[i]) != 0)
		{
			philo_finish(table);
			join_started(table, i);
			return (PHILO_ERR);
		}
		i++;
	}
	philo_monitor(table);
	join_started(table, table->config.number);
	return (PHILO_OK);
}
