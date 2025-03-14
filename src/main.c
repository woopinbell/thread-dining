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
	int			run_status;
	int			cleanup_status;

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
	run_status = philo_run(&table);
	if (run_status == PHILO_UNSAFE)
	{
		put_error("Error: worker thread could not be joined\n");
		// join 실패로 인해 스레드가 아직 살아있을 수 있는 상태(UNSAFE)
        // 이 상태에서 philo_table_destroy를 호출하면 실행 중인 스레드가 참조 중인 mutex를 파괴하는 UB가 될 수 있으므로, 정리를 시도하지 않고 곧바로 프로세스를 종료
		_exit(1);
	}
	cleanup_status = philo_table_destroy(&table);
	if (run_status != PHILO_OK)
	{
		if (cleanup_status != PHILO_OK)
			put_error("Error: failed to release table resources\n");
		put_error("Error: failed to run philosophers\n");
		return (1);
	}
	if (cleanup_status != PHILO_OK)
	{
		put_error("Error: failed to release table resources\n");
		return (1);
	}
	return (0);
}