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
	assign_philos(table);
	return (PHILO_OK);
}

void	philo_table_destroy(t_table *table)
{
	if (table == NULL)
		return ;
	free(table->forks);
	free(table->philos);
	table->forks = NULL;
	table->philos = NULL;
}
