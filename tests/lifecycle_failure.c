#include "philo.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static int	g_create_calls;
static int	g_fail_create_at;
static int	g_join_calls;
static int	g_fail_join_at;
static int	g_destroy_calls;
static int	g_fail_destroy_at;

int	test_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		void *(*routine)(void *), void *arg)
{
	int	call;

	call = g_create_calls++;
	if (call == g_fail_create_at)
		return (EAGAIN);
	return (pthread_create(thread, attr, routine, arg));
}

int	test_pthread_join(pthread_t thread, void **result)
{
	int	call;
	int	status;

	call = g_join_calls++;
	if (call == g_fail_join_at)
		return (EINVAL);
	status = pthread_join(thread, result);
	if (status != 0)
		return (status);
	return (0);
}

int	test_mutex_destroy(pthread_mutex_t *mutex)
{
	int	call;

	call = g_destroy_calls++;
	if (call == g_fail_destroy_at)
		return (EBUSY);
	return (pthread_mutex_destroy(mutex));
}

static void	set_config(t_config *config)
{
	config->number = 3;
	config->time_to_die = 1000;
	config->time_to_eat = 2;
	config->time_to_sleep = 2;
	config->must_eat = 1;
	config->has_meal_limit = 1;
}

static int	create_failure_case(int fail_at)
{
	t_config	config;
	t_table		table;
	int			status;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_create_calls = 0;
	g_fail_create_at = fail_at;
	g_join_calls = 0;
	g_fail_join_at = -1;
	g_fail_destroy_at = -1;
	status = philo_run(&table);
	if (status != PHILO_ERR || table.threads_started != fail_at
		|| table.threads_joined != fail_at || g_join_calls != fail_at)
	{
		fprintf(stderr, "create failure at %d was not rolled back\n", fail_at);
		return (1);
	}
	if (philo_table_destroy(&table) != PHILO_OK)
		return (1);
	return (0);
}

static int	join_failure_case(int fail_at)
{
	t_config		config;
	t_table			table;
	pthread_mutex_t	*forks;
	int				before_destroy;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_create_calls = 0;
	g_fail_create_at = -1;
	g_join_calls = 0;
	g_fail_join_at = fail_at;
	g_fail_destroy_at = -1;
	if (philo_run(&table) != PHILO_UNSAFE || g_join_calls != config.number
		|| table.threads_joined != config.number - 1)
		return (1);
	forks = table.forks;
	before_destroy = g_destroy_calls;
	if (philo_table_destroy(&table) != PHILO_UNSAFE || table.forks != forks
		|| table.fork_count != config.number
		|| g_destroy_calls != before_destroy)
	{
		fprintf(stderr, "unsafe table resources were released after join failure\n");
		return (1);
	}
	if (pthread_join(table.philos[fail_at].thread, NULL) != 0)
	{
		fprintf(stderr, "failed join did not leave a joinable worker\n");
		return (1);
	}
	table.destroy_safe = 1;
	table.threads_joined++;
	if (philo_table_destroy(&table) != PHILO_OK)
		return (1);
	return (0);
}

static int	destroy_failure_case(int fail_at, int remaining_forks)
{
	t_config	config;
	t_table		table;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_destroy_calls = 0;
	g_fail_destroy_at = fail_at;
	if (philo_table_destroy(&table) != PHILO_ERR
		|| table.forks == NULL || table.fork_count != remaining_forks)
	{
		fprintf(stderr, "destroy failure at %d lost retryable state\n", fail_at);
		return (1);
	}
	g_fail_destroy_at = -1;
	if (philo_table_destroy(&table) != PHILO_OK || table.forks != NULL)
		return (1);
	return (0);
}

int	main(void)
{
	if (create_failure_case(0) || create_failure_case(1)
		|| create_failure_case(2) || join_failure_case(0)
		|| join_failure_case(1) || destroy_failure_case(0, 3)
		|| destroy_failure_case(1, 2) || destroy_failure_case(3, 0)
		|| destroy_failure_case(4, 0))
		return (1);
	puts("lifecycle failure: ok");
	return (0);
}
