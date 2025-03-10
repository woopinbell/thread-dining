#include "philo.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void	normal_exit_hook(void)
{
	write(1, "normal exit hook\n", 17);
}

int	test_parse_args(int argc, char **argv, t_config *config)
{
	(void)argc;
	(void)argv;
	(void)config;
	if (atexit(normal_exit_hook) != 0)
		return (PHILO_ERR);
	return (PHILO_OK);
}

int	test_table_init(t_table *table, const t_config *config)
{
	(void)table;
	(void)config;
	return (PHILO_OK);
}

int	test_run(t_table *table)
{
	(void)table;
	printf("buffered stdio marker\n");
	return (PHILO_UNSAFE);
}

int	test_destroy(t_table *table)
{
	(void)table;
	puts("unsafe destroy called");
	return (PHILO_OK);
}
