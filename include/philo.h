#ifndef PHILO_H
# define PHILO_H

# include <pthread.h>
# include <stddef.h>
# include <sys/time.h>

# define PHILO_OK 0
# define PHILO_ERR 1

typedef struct s_table	t_table;

typedef struct s_config
{
	int	number;
	long	time_to_die;
	long	time_to_eat;
	long	time_to_sleep;
	int	must_eat;
	int	has_meal_limit;
}	t_config;

typedef struct s_philo
{
	int				id;
	int				meals;
	long			last_meal_ms;
	pthread_t		thread;
	pthread_mutex_t	*left_fork;
	pthread_mutex_t	*right_fork;
	t_table			*table;
}	t_philo;

struct s_table
{
	t_config		config;
	long			start_ms;
	int				ended;
	int				full_count;
	int				fork_count;
	int				state_ready;
	int				print_ready;
	pthread_mutex_t	*forks;
	pthread_mutex_t	state_mutex;
	pthread_mutex_t	print_mutex;
	t_philo			*philos;
};

int	philo_parse_args(int argc, char **argv, t_config *config);
int	philo_table_init(t_table *table, const t_config *config);
void	philo_table_destroy(t_table *table);
int	philo_run(t_table *table);
void	philo_monitor(t_table *table);
int	philo_has_ended(t_table *table);
void	philo_finish(t_table *table);
void	philo_log(t_philo *philo, const char *message);
void	philo_log_death(t_philo *philo);
void	*philo_routine(void *arg);
long	philo_now_ms(void);
void	philo_sleep_ms(t_table *table, long duration_ms);

#endif
