#include "philo.h"

#include <limits.h>

static int	parse_positive_i64(const char *text, int64_t *out)
{
	int64_t	value;
	int		i;

	if (text == NULL || text[0] == '\0')
		return (PHILO_ERR);
	value = 0;
	i = 0;
	if (text[i] == '+')
		i++;
	if (text[i] == '\0')
		return (PHILO_ERR);
	while (text[i] != '\0')
	{
		if (text[i] < '0' || text[i] > '9')
			return (PHILO_ERR);
		if (value > (INT64_MAX - (text[i] - '0')) / 10)
			return (PHILO_ERR);
		value = value * 10 + (text[i] - '0');
		i++;
	}
	if (value <= 0)
		return (PHILO_ERR);
	*out = value;
	return (PHILO_OK);
}

int	philo_parse_args(int argc, char **argv, t_config *config)
{
	int64_t	value;

	if (argc != 5 && argc != 6)
		return (PHILO_ERR);
	if (parse_positive_i64(argv[1], &value) != PHILO_OK || value > 200)
		return (PHILO_ERR);
	config->number = (int)value;
	if (parse_positive_i64(argv[2], &config->time_to_die) != PHILO_OK
		|| config->time_to_die > INT_MAX)
		return (PHILO_ERR);
	if (parse_positive_i64(argv[3], &config->time_to_eat) != PHILO_OK
		|| config->time_to_eat > INT_MAX)
		return (PHILO_ERR);
	if (parse_positive_i64(argv[4], &config->time_to_sleep) != PHILO_OK
		|| config->time_to_sleep > INT_MAX)
		return (PHILO_ERR);
	config->must_eat = 0;
	config->has_meal_limit = (argc == 6);
	if (argc == 6)
	{
		if (parse_positive_i64(argv[5], &value) != PHILO_OK || value > INT_MAX)
			return (PHILO_ERR);
		config->must_eat = (int)value;
	}
	return (PHILO_OK);
}
