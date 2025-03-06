#include "philo.h"

#include <pthread.h>
#include <stdio.h>

#define MODE_COMPLETION 1
#define MODE_STALE_DEATH 2

static t_table	*g_table;
static int		g_mode;
static int		g_injected;
static int		g_ended_at_unlock;

int	test_mutex_unlock(pthread_mutex_t *mutex)
{
	int	status;

	status = pthread_mutex_unlock(mutex);
	if (status != 0)
		return (status);
	if (!g_injected && mutex == &g_table->state_mutex)
	{
		g_injected = 1;
		g_ended_at_unlock = g_table->ended;
		if (g_mode == MODE_STALE_DEATH)
		{
			pthread_mutex_lock(&g_table->state_mutex);
			g_table->philos[0].last_meal_ms = philo_now_ms();
			g_table->full_count = 1;
			pthread_mutex_unlock(&g_table->state_mutex);
		}
	}
	return (0);
}

static void	set_config(t_config *config)
{
	config->number = 1;
	config->time_to_die = 100;
	config->time_to_eat = 10;
	config->time_to_sleep = 10;
	config->must_eat = 1;
	config->has_meal_limit = 1;
}

static int	init_case(t_table *table, t_config *config)
{
	if (philo_table_init(table, config) != PHILO_OK)
		return (1);
	table->start_ms = philo_now_ms();
	table->philos[0].last_meal_ms = table->start_ms;
	return (0);
}

static int	completion_case(void)
{
	t_table		table;
	t_config	config;

	set_config(&config);
	if (init_case(&table, &config))
		return (1);
	table.full_count = 1;
	g_table = &table;
	g_mode = MODE_COMPLETION;
	g_injected = 0;
	g_ended_at_unlock = 0;
	philo_monitor(&table);
	if (!g_injected || !g_ended_at_unlock || !table.ended)
	{
		fprintf(stderr, "meal completion was not committed while locked\n");
		philo_table_destroy(&table);
		return (1);
	}
	philo_table_destroy(&table);
	return (0);
}

static int	stale_death_case(void)
{
	t_table		table;
	t_config	config;

	set_config(&config);
	config.time_to_die = 10;
	if (init_case(&table, &config))
		return (1);
	table.start_ms = philo_now_ms() - 50;
	table.philos[0].last_meal_ms = table.start_ms;
	g_table = &table;
	g_mode = MODE_STALE_DEATH;
	g_injected = 0;
	philo_monitor(&table);
	if (!g_injected || !table.ended || table.full_count != 1)
	{
		fprintf(stderr, "stale death decision was not rechecked\n");
		philo_table_destroy(&table);
		return (1);
	}
	philo_table_destroy(&table);
	return (0);
}

int	main(void)
{
	if (completion_case() || stale_death_case())
		return (1);
	puts("terminal state: ok");
	return (0);
}
