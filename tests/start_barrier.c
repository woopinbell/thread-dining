#include "philo.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

typedef struct s_delayed_start
{
	void	*(*routine)(void *);
	void	*arg;
	useconds_t	delay_us;
}	t_delayed_start;

static pthread_mutex_t	g_gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	g_gate_cond = PTHREAD_COND_INITIALIZER;
static t_delayed_start	g_starts[8];
static int				g_created;
static int				g_released;

static void	*delayed_start(void *arg)
{
	t_delayed_start	*start;

	start = (t_delayed_start *)arg;
	pthread_mutex_lock(&g_gate_mutex);
	while (!g_released)
		pthread_cond_wait(&g_gate_cond, &g_gate_mutex);
	pthread_mutex_unlock(&g_gate_mutex);
	usleep(start->delay_us);
	return (start->routine(start->arg));
}

int	test_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		void *(*routine)(void *), void *arg)
{
	int	index;
	int	status;

	index = g_created;
	g_starts[index].routine = routine;
	g_starts[index].arg = arg;
	g_starts[index].delay_us = (index == 4) * 150000;
	status = pthread_create(thread, attr, delayed_start, &g_starts[index]);
	if (status != 0)
		return (status);
	g_created++;
	usleep(30000);
	if (g_created == 5)
	{
		pthread_mutex_lock(&g_gate_mutex);
		g_released = 1;
		pthread_cond_broadcast(&g_gate_cond);
		pthread_mutex_unlock(&g_gate_mutex);
	}
	return (0);
}

int	main(void)
{
	t_config	config;
	t_table		table;

	config.number = 5;
	config.time_to_die = 80;
	config.time_to_eat = 5;
	config.time_to_sleep = 5;
	config.must_eat = 1;
	config.has_meal_limit = 1;
	if (philo_table_init(&table, &config) != PHILO_OK)
		return (1);
	if (philo_run(&table) != PHILO_OK || table.full_count != config.number
		|| table.ready_count != config.number)
	{
		fprintf(stderr, "workers did not share one release timestamp\n");
		philo_table_destroy(&table);
		return (1);
	}
	philo_table_destroy(&table);
	puts("start barrier: ok");
	return (0);
}
