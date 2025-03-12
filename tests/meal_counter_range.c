#include "philo.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static int	sleep_calls;

int	test_philo_sleep_ms(t_table *table, int64_t duration_ms)
{
	(void)duration_ms;
	sleep_calls++;
	if (sleep_calls == 1)
		return (PHILO_OK);
	pthread_mutex_lock(&table->state_mutex);
	table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
	return (PHILO_ERR);
}

int	main(void)
{
	t_config	config;
	t_table		table;
	int64_t		expected_meals;
	int			status;

	config.number = 2;
	config.time_to_die = 100;
	config.time_to_eat = 50;
	config.time_to_sleep = 10;
	config.must_eat = INT_MAX;
	config.has_meal_limit = 1;
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	table.start_ms = philo_now_ms();
	table.start_released = 1;
	table.full_count = 1;
	table.philos[0].meals = INT_MAX;
	table.philos[0].last_meal_ms = table.start_ms;
	philo_routine(&table.philos[0]);
	expected_meals = INT_MAX;
	expected_meals++;
	status = 0;
	if (sleep_calls != 2
		|| table.philos[0].meals != expected_meals
		|| table.full_count != 1)
	{
		fprintf(stderr, "meal counter did not advance beyond INT_MAX\n");
		status = 1;
	}
	if (philo_table_destroy(&table) != PHILO_OK)
		status = 1;
	if (status == 0)
		puts("meal counter range: ok");
	return (status);
}
