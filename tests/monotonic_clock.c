#include "philo.h"

#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int	used_monotonic_clock;
static int	fail_clock;

int	test_clock_gettime(clockid_t clock_id, struct timespec *now)
{
	if (fail_clock)
		return (-1);
	if (clock_id == CLOCK_MONOTONIC)
		used_monotonic_clock = 1;
	now->tv_sec = 12;
	now->tv_nsec = 345000000L;
	return (0);
}

int	main(void)
{
	pid_t	pid;
	int		status;

	if (philo_now_ms() != 12345L || !used_monotonic_clock)
	{
		fprintf(stderr, "elapsed time did not use CLOCK_MONOTONIC\n");
		return (1);
	}
	pid = fork();
	if (pid < 0)
		return (1);
	if (pid == 0)
	{
		fail_clock = 1;
		(void)philo_now_ms();
		_exit(99);
	}
	if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)
		|| WEXITSTATUS(status) != PHILO_ERR)
	{
		fprintf(stderr, "clock failure did not stop the process\n");
		return (1);
	}
	puts("monotonic clock: ok");
	return (0);
}
