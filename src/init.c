#include "philo.h"

#include <stdlib.h>

static void	assign_philos(t_table *table)
{
	int	i;

	i = 0;
	while (i < table->config.number)
	{
		table->philos[i].id = i + 1;
		table->philos[i].meals = 0;
		table->philos[i].last_meal_ms = 0;
		table->philos[i].left_fork = &table->forks[i];
		table->philos[i].right_fork = &table->forks[(i + 1) % table->config.number];
		table->philos[i].table = table;
		i++;
	}
}

static int	init_forks(t_table *table, int count)
{
	int	i;

	i = 0;
	while (i < count)
	{
		if (pthread_mutex_init(&table->forks[i], NULL) != 0)
			return (PHILO_ERR);
		table->fork_count++;
		i++;
	}
	return (PHILO_OK);
}

int	philo_table_init(t_table *table, const t_config *config)
{
	table->config = *config;
	table->start_ms = 0;
	table->ended = 0;
	table->full_count = 0;
	table->fork_count = 0;
	table->state_ready = 0;
	table->start_cond_ready = 0;
	table->print_ready = 0;
	table->start_released = 0;
	table->ready_count = 0;
	table->run_error = 0;
	table->threads_started = 0;
	table->threads_joined = 0;
	table->destroy_safe = 1;
	table->forks = malloc(sizeof(*table->forks) * config->number);
	table->philos = malloc(sizeof(*table->philos) * config->number);
	if (table->forks == NULL || table->philos == NULL)
		return (philo_table_destroy(table), PHILO_ERR);
	if (pthread_mutex_init(&table->state_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->state_ready = 1;
	if (pthread_cond_init(&table->start_cond, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->start_cond_ready = 1;
	if (pthread_mutex_init(&table->print_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->print_ready = 1;
	if (init_forks(table, config->number) != PHILO_OK)
		return (philo_table_destroy(table), PHILO_ERR);
	assign_philos(table);
	return (PHILO_OK);
}

int	philo_table_destroy(t_table *table)
{
	if (table == NULL)
		return (PHILO_ERR);
	if (!table->destroy_safe || table->threads_joined < table->threads_started)
		return (PHILO_UNSAFE);
	if (table->forks != NULL)
	{
		while (table->fork_count > 0)
		{
			if (pthread_mutex_destroy(
					&table->forks[table->fork_count - 1]) != 0)
				return (PHILO_ERR);
			table->fork_count--;
		}
	}
	if (table->print_ready)
	{
		if (pthread_mutex_destroy(&table->print_mutex) != 0)
			return (PHILO_ERR);
		table->print_ready = 0;
	}
	if (table->start_cond_ready)
	{
		if (pthread_cond_destroy(&table->start_cond) != 0)
			return (PHILO_ERR);
		table->start_cond_ready = 0;
	}
	if (table->state_ready)
	{
		if (pthread_mutex_destroy(&table->state_mutex) != 0)
			return (PHILO_ERR);
		table->state_ready = 0;
	}
	free(table->forks);
	free(table->philos);
	table->forks = NULL;
	table->philos = NULL;
	return (PHILO_OK);
}
