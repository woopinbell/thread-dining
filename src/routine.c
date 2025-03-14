#include "philo.h"

// barrier(전원 집합점) 패턴: 각 철학자 스레드가 "준비됨"을 알리고(ready_count++, broadcast) 나면, 
// run.c의 release_start가 전원이 다 모인 걸 확인한 뒤 start_released를 세우고 broadcast할 때까지 여기서 대기함
// 스레드 생성/스케줄링 시각이 제각각이라도 모든 철학자가 정확히 같은 시점에 동시에 출발하게 만들기 위함
static int	wait_for_start(t_philo *philo)
{
	t_table	*table;
	int		ended;

	table = philo->table;
	pthread_mutex_lock(&table->state_mutex);
	table->ready_count++;
	pthread_cond_broadcast(&table->start_cond);
	while (!table->start_released)
	{
		if (pthread_cond_wait(&table->start_cond,
				&table->state_mutex) != 0)
		{
			table->run_error = 1;
			table->ended = 1;
			table->start_released = 1;
			pthread_cond_broadcast(&table->start_cond);
		}
	}
	ended = table->ended;
	pthread_mutex_unlock(&table->state_mutex);
	return (ended);
}

// 짝수/홀수 id마다 포크를 잡는 순서를 반대로 둔 것이 이 코드의 핵심 데드락 회피 장치
// 모두가 "왼쪽 먼저"만 잡으면 전원이 동시에 왼쪽 포크를 쥔 채 오른쪽을 기다리는 순환 대기(circular wait)가 만들어질 수 있는데, 절반의 순서를 뒤집으면 이 순환 자체가 끊어짐
static void	lock_forks(t_philo *philo)
{
	if (philo->id % 2 == 0)
	{
		pthread_mutex_lock(philo->right_fork);
		philo_log(philo, "has taken a fork");
		pthread_mutex_lock(philo->left_fork);
		philo_log(philo, "has taken a fork");
	}
	else
	{
		pthread_mutex_lock(philo->left_fork);
		philo_log(philo, "has taken a fork");
		pthread_mutex_lock(philo->right_fork);
		philo_log(philo, "has taken a fork");
	}
}

static void	unlock_forks(t_philo *philo)
{
	pthread_mutex_unlock(philo->left_fork);
	pthread_mutex_unlock(philo->right_fork);
}

static void	record_meal_start(t_philo *philo)
{
	pthread_mutex_lock(&philo->table->state_mutex);
	// last_meal_ms는 monitor 스레드도 읽는 공유 값이라, 단순 대입이라도 state_mutex로 감싸야 두 스레드 사이에서 갱신된 값이 제때 보이는 것을 보장할 수 있음
	philo->last_meal_ms = philo_now_ms();
	pthread_mutex_unlock(&philo->table->state_mutex);
}

static int	record_meal_done(t_philo *philo)
{
	t_table	*table;

	table = philo->table;
	pthread_mutex_lock(&table->state_mutex);
	if (table->ended)
	{
		pthread_mutex_unlock(&table->state_mutex);
		return (PHILO_ERR);
	}
	philo->meals++;
	if (table->config.has_meal_limit && philo->meals == table->config.must_eat)
		table->full_count++;
	if (table->config.has_meal_limit && table->full_count >= table->config.number)
		table->ended = 1;
	pthread_mutex_unlock(&table->state_mutex);
	return (PHILO_OK);
}

static int	eat_once(t_philo *philo)
{
	lock_forks(philo);
	if (philo_has_ended(philo->table))
	{
		unlock_forks(philo);
		return (PHILO_ERR);
	}
	// 포크를 잡은 직후 종료 여부를 한 번 더 확인
    // 포크를 얻기까지 기다리는 동안 다른 철학자가 죽어 시뮬레이션이 이미 끝났을 수 있으므로, 그 경우 불필요하게 먹는 동작(sleep)까지 진행하지 않고 바로 빠져나감
	record_meal_start(philo);
	philo_log(philo, "is eating");
	if (philo_sleep_ms(philo->table, philo->table->config.time_to_eat)
		!= PHILO_OK || record_meal_done(philo) != PHILO_OK)
	{
		unlock_forks(philo);
		return (PHILO_ERR);
	}
	unlock_forks(philo);
	return (PHILO_OK);
}

// 철학자가 1명뿐이면 포크도 하나뿐이라 왼쪽 포크는 잡을 수 있어도 오른쪽 포크(왼쪽과 동일한 mutex)는 영원히 잡을 수 없음
// 즉 정상적으로는 절대 먹지 못하고 굶어 죽는 것이 이 문제의 정답 동작이라, 일반 루프와 분리해 이 경우만 별도로 처리함
static void	wait_single_philo(t_philo *philo)
{
	pthread_mutex_lock(philo->left_fork);
	philo_log(philo, "has taken a fork");
	philo_sleep_ms(philo->table, philo->table->config.time_to_die + 1);
	pthread_mutex_unlock(philo->left_fork);
}

void	*philo_routine(void *arg)
{
	t_philo	*philo;

	philo = (t_philo *)arg;
	if (wait_for_start(philo))
		return (NULL);
	if (philo->table->config.number == 1)
	{
		wait_single_philo(philo);
		return (NULL);
	}
	if (philo->id % 2 == 0)
		philo_sleep_ms(philo->table, 1);
	// 짝수 id를 1ms만큼 지연시켜 출발
    // 인원이 짝수일 때 전원이 정확히 동시에 포크를 향해 달려들면서 생기는 경쟁 패턴을 깨뜨려, 특정 타이밍에 몰리는 상황을 완화하기 위한 보정
	while (!philo_has_ended(philo->table))
	{
		if (eat_once(philo) != PHILO_OK)
			break ;
		if (philo_has_ended(philo->table))
			break ;
		philo_log(philo, "is sleeping");
		philo_sleep_ms(philo->table, philo->table->config.time_to_sleep);
		philo_log(philo, "is thinking");
	}
	return (NULL);
}