NAME := build/bin/philo
BIN_DIR := build/bin
OBJ_DIR := build/obj

CC := cc
CFLAGS := -Wall -Wextra -Werror -pthread -Iinclude -MMD -MP
TSAN_CC ?= $(CC)
TSAN_REQUIRED ?= 0

SRC := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all bonus clean fclean re test test-tsan

all: $(NAME)

$(NAME): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: src/%.c include/philo.h | $(OBJ_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

bonus:
	@printf 'bonus target is unavailable\n'
	@exit 1

clean:
	rm -rf build tests/__pycache__ .pytest_cache

fclean: clean

re: fclean all

test: all
	PHILO_BIN="$(CURDIR)/$(NAME)" ./tests/smoke.sh
	PHILO_BIN="$(CURDIR)/$(NAME)" ./tests/concurrency.sh

test-tsan:
	TSAN_CC="$(TSAN_CC)" TSAN_REQUIRED="$(TSAN_REQUIRED)" ./tests/tsan.sh

-include $(OBJS:.o=.d)
