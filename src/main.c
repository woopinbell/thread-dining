#include "philo.h"

#include <unistd.h>

static void	print_usage(void)
{
	write(2, "Usage: ./philo number_of_philosophers time_to_die ", 50);
	write(2, "time_to_eat time_to_sleep ", 26);
	write(2, "[number_of_times_each_philosopher_must_eat]\n", 45);
}

int	main(int argc, char **argv)
{
	t_config	config;

	if (philo_parse_args(argc, argv, &config) != PHILO_OK)
	{
		print_usage();
		return (1);
	}
	(void)config;
	return (0);
}
