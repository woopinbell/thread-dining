#ifndef PHILO_H
# define PHILO_H

# include <stddef.h>

# define PHILO_OK 0
# define PHILO_ERR 1

typedef struct s_config
{
	int	number;
	long	time_to_die;
	long	time_to_eat;
	long	time_to_sleep;
	int	must_eat;
	int	has_meal_limit;
}	t_config;

int	philo_parse_args(int argc, char **argv, t_config *config);

#endif
