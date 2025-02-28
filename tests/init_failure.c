#include "philo.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>

static int		init_calls;
static void		*destroyed[32];
static size_t	destroy_count;
static int		duplicate_destroy;

int	test_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	(void)mutex;
	(void)attr;
	init_calls++;
	if (init_calls == 4)
		return (1);
	return (0);
}

int	test_mutex_destroy(pthread_mutex_t *mutex)
{
	size_t	i;

	i = 0;
	while (i < destroy_count)
	{
		if (destroyed[i] == (void *)mutex)
		{
			duplicate_destroy = 1;
			return (1);
		}
		i++;
	}
	destroyed[destroy_count++] = (void *)mutex;
	return (0);
}

int	main(void)
{
	t_config	config;
	t_table		table;

	config.number = 4;
	config.time_to_die = 100;
	config.time_to_eat = 20;
	config.time_to_sleep = 20;
	config.must_eat = 0;
	config.has_meal_limit = 0;
	if (philo_table_init(&table, &config) != PHILO_ERR)
	{
		fprintf(stderr, "injected init failure was not propagated\n");
		return (1);
	}
	if (duplicate_destroy || destroy_count != 3)
	{
		fprintf(stderr, "mutex rollback did not destroy each resource once\n");
		return (1);
	}
	if (table.forks != NULL || table.philos != NULL)
	{
		fprintf(stderr, "failed init retained allocations\n");
		return (1);
	}
	philo_table_destroy(&table);
	if (duplicate_destroy || destroy_count != 3)
	{
		fprintf(stderr, "cleanup was not idempotent after failed init\n");
		return (1);
	}
	puts("init failure rollback: ok");
	return (0);
}
