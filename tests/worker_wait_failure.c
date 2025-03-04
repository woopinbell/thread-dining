#include "philo.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t	g_failure_mutex = PTHREAD_MUTEX_INITIALIZER;
static int				g_failed;

int	test_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int	should_fail;

	pthread_mutex_lock(&g_failure_mutex);
	should_fail = !g_failed;
	if (should_fail)
		g_failed = 1;
	pthread_mutex_unlock(&g_failure_mutex);
	if (should_fail)
		return (EINVAL);
	return (pthread_cond_wait(cond, mutex));
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

int	main(void)
{
	t_config	config;
	t_table		table;
	int			status;

	set_config(&config);
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	g_failed = 0;
	status = philo_run(&table);
	if (!g_failed || status != PHILO_ERR || !table.run_error)
	{
		fprintf(stderr, "worker wait failure returned success\n");
		return (1);
	}
	philo_table_destroy(&table);
	puts("worker wait failure: ok");
	return (0);
}
