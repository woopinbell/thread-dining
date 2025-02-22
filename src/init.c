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
		{
			while (--i >= 0)
				pthread_mutex_destroy(&table->forks[i]);
			return (PHILO_ERR);
		}
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
	table->print_ready = 0;
	table->forks = malloc(sizeof(*table->forks) * config->number);
	table->philos = malloc(sizeof(*table->philos) * config->number);
	if (table->forks == NULL || table->philos == NULL)
		return (philo_table_destroy(table), PHILO_ERR);
	if (pthread_mutex_init(&table->state_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->state_ready = 1;
	if (pthread_mutex_init(&table->print_mutex, NULL) != 0)
		return (philo_table_destroy(table), PHILO_ERR);
	table->print_ready = 1;
	if (init_forks(table, config->number) != PHILO_OK)
		return (philo_table_destroy(table), PHILO_ERR);
	assign_philos(table);
	return (PHILO_OK);
}

void	philo_table_destroy(t_table *table)
{
	int	i;

	if (table == NULL)
		return ;
	i = 0;
	if (table->forks != NULL)
	{
		while (i < table->fork_count)
			pthread_mutex_destroy(&table->forks[i++]);
	}
	if (table->print_ready)
		pthread_mutex_destroy(&table->print_mutex);
	if (table->state_ready)
		pthread_mutex_destroy(&table->state_mutex);
	free(table->forks);
	free(table->philos);
	table->forks = NULL;
	table->philos = NULL;
}
