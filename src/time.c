#include "philo.h"

#include <unistd.h>

long	philo_now_ms(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
}

void	philo_sleep_ms(t_table *table, long duration_ms)
{
	long	deadline;
	int		ended;

	deadline = philo_now_ms() + duration_ms;
	while (philo_now_ms() < deadline)
	{
		pthread_mutex_lock(&table->state_mutex);
		ended = table->ended;
		pthread_mutex_unlock(&table->state_mutex);
		if (ended)
			break ;
		usleep(500);
	}
}
