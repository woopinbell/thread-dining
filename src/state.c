#include "philo.h"

#include <stdio.h>

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
	long	timestamp;

	table = philo->table;
	pthread_mutex_lock(&table->print_mutex);
	if (!philo_has_ended(table))
	{
		timestamp = philo_now_ms() - table->start_ms;
		printf("%ld %d %s\n", timestamp, philo->id, message);
	}
	pthread_mutex_unlock(&table->print_mutex);
}

void	philo_log_death(t_philo *philo)
{
	t_table	*table;
	long	timestamp;
	int		should_print;

	table = philo->table;
	pthread_mutex_lock(&table->state_mutex);
	should_print = !table->ended;
	table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
	if (should_print)
	{
		pthread_mutex_lock(&table->print_mutex);
		timestamp = philo_now_ms() - table->start_ms;
		printf("%ld %d died\n", timestamp, philo->id);
		pthread_mutex_unlock(&table->print_mutex);
	}
}
