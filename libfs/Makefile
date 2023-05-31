# Target library
lib := libfs.a
objects := fs.o
CFLAGS := -Wall -Wextra -Werror -g

all: $(lib)

$(lib): $(objects)
	ar rcs $@ $^

fs.o: fs.c fs.h
	gcc $(CFLAGS) -c fs.c

clean:
	rm -f $(lib) $(objects)
