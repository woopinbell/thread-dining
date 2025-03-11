NAME := philo

CC := cc
CFLAGS := -Wall -Wextra -Werror -pthread -Iinclude
TSAN_CC ?= $(CC)
TSAN_REQUIRED ?= 0

SRC_DIR := src
OBJ_DIR := .obj

SRCS := \
	$(SRC_DIR)/init.c \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/monitor.c \
	$(SRC_DIR)/parse.c \
	$(SRC_DIR)/routine.c \
	$(SRC_DIR)/run.c \
	$(SRC_DIR)/state.c \
	$(SRC_DIR)/time.c
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all bonus clean fclean re test test-tsan

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c include/philo.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

bonus:
	@printf 'bonus target is unavailable\n'
	@exit 1

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

test: all
	./tests/smoke.sh
	./tests/concurrency.sh

test-tsan:
	TSAN_CC="$(TSAN_CC)" TSAN_REQUIRED="$(TSAN_REQUIRED)" ./tests/tsan.sh
