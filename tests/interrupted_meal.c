#include "philo.h"

#include <stdio.h>

static int	interrupted;

int	test_philo_sleep_ms(t_table *table, int64_t duration_ms)
{
	(void)duration_ms;
	pthread_mutex_lock(&table->state_mutex);
	table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
	interrupted = 1;
	return (PHILO_ERR);
}

int	main(void)
{
	t_config	config;
	t_table		table;

	config.number = 2;
	config.time_to_die = 100;
	config.time_to_eat = 50;
	config.time_to_sleep = 10;
	config.must_eat = 1;
	config.has_meal_limit = 1;
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	table.start_ms = philo_now_ms();
	table.start_released = 1;
	table.philos[0].last_meal_ms = table.start_ms;
	philo_routine(&table.philos[0]);
	if (!interrupted || table.philos[0].meals != 0 || table.full_count != 0)
	{
		fprintf(stderr, "interrupted meal changed completion counters\n");
		philo_table_destroy(&table);
		return (1);
	}
	philo_table_destroy(&table);
	puts("interrupted meal: ok");
	return (0);
}
