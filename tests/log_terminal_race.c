#include "philo.h"

#include <pthread.h>
#include <stdio.h>

#define LOGGER_COUNT 12
#define LOGS_PER_LOGGER 200

static t_table			g_table;
static pthread_mutex_t	g_gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	g_gate_cond = PTHREAD_COND_INITIALIZER;
static int				g_ready;
static int				g_go;

static void	*log_states(void *arg)
{
	t_philo	*philo;
	int		i;

	philo = (t_philo *)arg;
	pthread_mutex_lock(&g_gate_mutex);
	g_ready++;
	pthread_cond_broadcast(&g_gate_cond);
	while (!g_go)
		pthread_cond_wait(&g_gate_cond, &g_gate_mutex);
	pthread_mutex_unlock(&g_gate_mutex);
	i = 0;
	while (i < LOGS_PER_LOGGER)
	{
		philo_log(philo, "is thinking");
		i++;
	}
	return (NULL);
}

static void	set_config(t_config *config)
{
	config->number = 1;
	config->time_to_die = 1;
	config->time_to_eat = 1;
	config->time_to_sleep = 1;
	config->must_eat = 0;
	config->has_meal_limit = 0;
}

int	main(void)
{
	pthread_t	threads[LOGGER_COUNT];
	t_config	config;
	int			started;
	int			i;

	set_config(&config);
	if (philo_table_init(&g_table, &config) != PHILO_OK)
		return (1);
	g_table.start_ms = philo_now_ms() - 100;
	g_table.philos[0].last_meal_ms = g_table.start_ms;
	started = 0;
	while (started < LOGGER_COUNT)
	{
		if (pthread_create(&threads[started], NULL, log_states,
				&g_table.philos[0]) != 0)
			break ;
		started++;
	}
	pthread_mutex_lock(&g_gate_mutex);
	while (g_ready < started)
		pthread_cond_wait(&g_gate_cond, &g_gate_mutex);
	g_go = 1;
	pthread_cond_broadcast(&g_gate_cond);
	pthread_mutex_unlock(&g_gate_mutex);
	if (started != LOGGER_COUNT || !philo_try_log_death(&g_table.philos[0]))
	{
		fprintf(stderr, "terminal log race could not be started\n");
		philo_finish(&g_table);
	}
	i = 0;
	while (i < started)
	{
		pthread_join(threads[i], NULL);
		i++;
	}
	if (started != LOGGER_COUNT || philo_table_destroy(&g_table) != PHILO_OK)
		return (1);
	return (0);
}
