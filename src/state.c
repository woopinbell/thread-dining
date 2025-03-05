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
	int64_t	timestamp;

	table = philo->table;
	pthread_mutex_lock(&table->print_mutex);
	if (!philo_has_ended(table))
	{
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
	pthread_mutex_unlock(&table->print_mutex);
	return (should_print);
}
