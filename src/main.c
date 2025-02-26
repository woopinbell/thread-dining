#include "philo.h"

#include <stddef.h>
#include <unistd.h>

static size_t	ft_strlen(const char *text)
{
	size_t	len;

	len = 0;
	while (text[len] != '\0')
		len++;
	return (len);
}

static void	put_error(const char *message)
{
	write(2, message, ft_strlen(message));
}

int	main(int argc, char **argv)
{
	t_config	config;
	t_table		table;

	if (philo_parse_args(argc, argv, &config) != PHILO_OK)
	{
		put_error("Usage: ./philo number_of_philosophers time_to_die ");
		put_error("time_to_eat time_to_sleep ");
		put_error("[number_of_times_each_philosopher_must_eat]\n");
		return (1);
	}
	if (philo_table_init(&table, &config) != PHILO_OK)
	{
		put_error("Error: failed to initialize table\n");
		return (1);
	}
	if (philo_run(&table) != PHILO_OK)
	{
		philo_table_destroy(&table);
		put_error("Error: failed to run philosophers\n");
		return (1);
	}
	philo_table_destroy(&table);
	return (0);
}
