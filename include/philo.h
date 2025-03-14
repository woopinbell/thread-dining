#ifndef PHILO_H
# define PHILO_H

# include <pthread.h>
# include <stddef.h>
# include <stdint.h>
# include <time.h>

# define PHILO_OK 0
# define PHILO_ERR 1
// 실패를 두 단계로 구분하며 종로 경로(정리 후 종료 vs 정리 없이 즉시 종류)를 다르게 구현
// ERR는 "정리(destroy)까지 마치고 안전하게 종료 가능한 실패", 
// UNSAFE는 "스레드가 아직 mutex/condvar를 참조하고 있을 수 있어 자원 해제를 시도하면 안 되는 상태"
# define PHILO_UNSAFE 2

typedef struct s_table	t_table;

typedef struct s_config
{
	int	number;
	int64_t	time_to_die;
	int64_t	time_to_eat;
	int64_t	time_to_sleep;
	int	must_eat;
	int	has_meal_limit;
}	t_config;

typedef struct s_philo
{
	int				id;
	int64_t			meals;
	int64_t		last_meal_ms;
	pthread_t		thread;
	// left_fork/right_fork는 이 철학자가 소유하는 mutex가 아니라 table->forks 배열 안의 mutex를 가리키는 포인터
    // 인접한 두 철학자가 포크 하나(mutex 하나)를 공유하는 원형 테이블 구조의 관례적 표현
	pthread_mutex_t	*left_fork;
	pthread_mutex_t	*right_fork;
	t_table			*table;
}	t_philo;

struct s_table
{
	t_config		config;
	int64_t		start_ms;
	int				ended;
	int				full_count;
	int				fork_count;
	// state_ready/start_cond_ready/print_ready는 각 mutex/condvar가 실제로 init에 성공했는지를 개별로 추적하는 플래그
    // 초기화가 중간에 실패해도 destroy 쪽에서 "실제로 만들어진 것만" 골라 해제할 수 있게 하기 위한 설계
	int				state_ready;
	int				start_cond_ready;
	int				print_ready;
	// start_released/ready_count는 모든 철학자 스레드가 정확히 같은 시점에 동시에 출발하도록 만드는 barrier 동기화 상태
    // routine.c의 wait_for_start, run.c의 release_start에서 사용
	int				start_released;
	int				ready_count;
	int				run_error;
	// threads_joined가 threads_started보다 적거나(join 실패 포함) destroy_safe가 0이면, 아직 스레드가 이 mutex들을 참조할 수 있는 상태로 간주해 destroy 자체를 거부함
    // 실행 중일 수 있는 스레드가 참조하는 mutex를 파괴하는 undefined behavior를 막기 위한 안전장치
	int				threads_started;
	int				threads_joined;
	int				destroy_safe;
	pthread_mutex_t	*forks;
	// state_mutex(공유 상태 보호)와 print_mutex(출력 직렬화)를 별도 mutex로 분리
    // printf 같은 I/O가 상대적으로 오래 걸릴 수 있어, 출력 중에 상태 갱신 로직까지 같은 락에 묶여 블로킹되는 걸 피하기 위함
	pthread_mutex_t	state_mutex;
	pthread_cond_t	start_cond;
	pthread_mutex_t	print_mutex;
	t_philo			*philos;
};

int	philo_parse_args(int argc, char **argv, t_config *config);
int	philo_table_init(t_table *table, const t_config *config);
int	philo_table_destroy(t_table *table);
int	philo_run(t_table *table);
void	philo_monitor(t_table *table);
int	philo_has_ended(t_table *table);
void	philo_finish(t_table *table);
void	philo_log(t_philo *philo, const char *message);
int	philo_try_log_death(t_philo *philo);
void	*philo_routine(void *arg);
int64_t	philo_now_ms(void);
int	philo_sleep_ms(t_table *table, int64_t duration_ms);

#endif