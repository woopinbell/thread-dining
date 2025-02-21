NAME := philo

CC := cc
CFLAGS := -Wall -Wextra -Werror -pthread -Iinclude

SRC_DIR := src
OBJ_DIR := .obj

SRCS := \
	$(SRC_DIR)/init.c \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/parse.c
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all bonus clean fclean re

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
