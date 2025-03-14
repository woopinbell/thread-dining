#include "philo.h"

#include <unistd.h>

static void	clock_failure(void)
{
	static const char	message[] = "Error: monotonic clock unavailable\n";

	(void)write(2, message, sizeof(message) - 1);
	_exit(PHILO_ERR);
}

int64_t	philo_now_ms(void)
{
	struct timespec	now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		clock_failure();
	// CLOCK_REALTIME이 아니라 CLOCK_MONOTONIC을 쓴 이유: 
    // REALTIME은 시스템 시간이 NTP 동기화나 수동 변경으로 갑자기 앞뒤로 튈 수 있어, time_to_die 같은 상대 시간 계산에 쓰면 음수나 역행하는 델타가 나올 수 있음.
    // MONOTONIC은 그런 외부 조정의 영향을 받지 않고 항상 앞으로만 흐르는 시계라 이 시뮬레이션의 타이밍 계산에 적합함
	return (((int64_t)now.tv_sec * 1000) + (now.tv_nsec / 1000000));
}

int	philo_sleep_ms(t_table *table, int64_t duration_ms)
{
	int64_t	deadline;
	int64_t	now;
	int64_t	remaining;
	int		ended;

	deadline = philo_now_ms() + duration_ms;
	while (1)
	{
		now = philo_now_ms();
		if (now >= deadline)
			return (PHILO_OK);
		pthread_mutex_lock(&table->state_mutex);
		ended = table->ended;
		pthread_mutex_unlock(&table->state_mutex);
		if (ended)
			return (PHILO_ERR);
		remaining = deadline - now;
		if (remaining > 1)
			usleep(500);
		else
			usleep(100);
		// duration 전체를 한 번에 usleep하지 않고 짧은 간격으로 나눠 깨어나 매번 ended를 확인하는 이유:
        // 자는 도중에 다른 철학자가 죽어 시뮬레이션이 끝나도 그걸 즉시 감지해서 빠르게 리턴할 수 있어야 하기 때문
        // 응답성과 CPU 사용량 사이의 절충
	}
}