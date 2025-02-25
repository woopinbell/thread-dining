#include "philo.h"

static void	lock_forks(t_philo *philo)
{
	if (philo->id % 2 == 0)
	{
		pthread_mutex_lock(philo->right_fork);
		philo_log(philo, "has taken a fork");
		pthread_mutex_lock(philo->left_fork);
		philo_log(philo, "has taken a fork");
	}
	else
	{
		pthread_mutex_lock(philo->left_fork);
		philo_log(philo, "has taken a fork");
		pthread_mutex_lock(philo->right_fork);
		philo_log(philo, "has taken a fork");
	}
}

static void	unlock_forks(t_philo *philo)
{
	pthread_mutex_unlock(philo->left_fork);
	pthread_mutex_unlock(philo->right_fork);
}

static void	record_meal_start(t_philo *philo)
{
	pthread_mutex_lock(&philo->table->state_mutex);
	philo->last_meal_ms = philo_now_ms();
	pthread_mutex_unlock(&philo->table->state_mutex);
}

static void	record_meal_done(t_philo *philo)
{
	t_table	*table;

	table = philo->table;
	pthread_mutex_lock(&table->state_mutex);
	philo->meals++;
	if (table->config.has_meal_limit && philo->meals == table->config.must_eat)
		table->full_count++;
	if (table->config.has_meal_limit && table->full_count >= table->config.number)
		table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
}

static int	eat_once(t_philo *philo)
{
	lock_forks(philo);
	if (philo_has_ended(philo->table))
	{
		unlock_forks(philo);
		return (PHILO_ERR);
	}
	record_meal_start(philo);
	philo_log(philo, "is eating");
	philo_sleep_ms(philo->table, philo->table->config.time_to_eat);
	record_meal_done(philo);
	unlock_forks(philo);
	return (PHILO_OK);
}

static void	wait_single_philo(t_philo *philo)
{
	pthread_mutex_lock(philo->left_fork);
	philo_log(philo, "has taken a fork");
	philo_sleep_ms(philo->table, philo->table->config.time_to_die + 1);
	pthread_mutex_unlock(philo->left_fork);
}

void	*philo_routine(void *arg)
{
	t_philo	*philo;

	philo = (t_philo *)arg;
	if (philo->table->config.number == 1)
	{
		wait_single_philo(philo);
		return (NULL);
	}
	if (philo->id % 2 == 0)
		philo_sleep_ms(philo->table, 1);
	while (!philo_has_ended(philo->table))
	{
		if (eat_once(philo) != PHILO_OK)
			break ;
		if (philo_has_ended(philo->table))
			break ;
		philo_log(philo, "is sleeping");
		philo_sleep_ms(philo->table, philo->table->config.time_to_sleep);
		philo_log(philo, "is thinking");
	}
	return (NULL);
}
