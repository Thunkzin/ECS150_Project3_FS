# Target library
lib := libfs.a
objects := fs.o disk.o 
CFLAGS := -Wall -Wextra -Werror -g

all: $(lib)

$(lib): $(objects)
	ar rcs $@ $^

fs.o: fs.c fs.h
	gcc $(CFLAGS) -c fs.c

disk.o: disk.c disk.h
	gcc $(CFLAGS) -c disk.c

clean:
	rm -f $(lib) $(objects)
