#include "philo.h"

#include <unistd.h>

static void	clock_failure(void)
{
	static const char	message[] = "Error: monotonic clock unavailable\n";

	(void)write(2, message, sizeof(message) - 1);
	_exit(PHILO_ERR);
}

int64_t	philo_now_ms(void)
{
	struct timespec	now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		clock_failure();
	return (((int64_t)now.tv_sec * 1000) + (now.tv_nsec / 1000000));
}

void	philo_sleep_ms(t_table *table, int64_t duration_ms)
{
	int64_t	deadline;
	int64_t	remaining;
	int		ended;

	deadline = philo_now_ms() + duration_ms;
	while (philo_now_ms() < deadline)
	{
		pthread_mutex_lock(&table->state_mutex);
		ended = table->ended;
		pthread_mutex_unlock(&table->state_mutex);
		if (ended)
			break ;
		remaining = deadline - philo_now_ms();
		if (remaining > 1)
			usleep(500);
		else
			usleep(100);
	}
}
