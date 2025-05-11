SRC = src/file.c src/main.c src/response.c src/server.c src/log.c

WEBSITE_SRC_DIR = website

WEBSITE_DEST_DIR = ~/.local/share/cyllenian/

NAME = cyllenian

RM = rm -f

CP = cp -f -r

DESTDIR = /usr/bin/

SRCMAN = man/

MANPAGE = cyllenian.1

COMPMAN = cyllenian.1.gz 

COMPRESS = gzip < $(SRCMAN)$(MANPAGE) > $(SRCMAN)$(COMPMAN)

MANDIR = /usr/share/man/man1/

MANDB = mandb

CC = gcc

OBJS=	$(SRC:.c=.o)

CFLAGS = -Wall -Wextra -pedantic -g -I./include

LDFLAGS = -lssl -lnn

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) -g -o $(NAME) $(OBJS) $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) $(OBJS)

cleanMan:
	$(RM) $(SRCMAN)$(COMPMAN)

fclean: clean cleanMan
	$(RM) $(NAME)

install: $(NAME) 
	$(CP) $(NAME) $(DESTDIR)
	$(CP) $(WEBSITE_SRC_DIR) $(WEBSITE_DEST_DIR)
	$(COMPRESS)
	$(CP) $(SRCMAN)$(COMPMAN) $(MANDIR)
	$(MANDB)

re: fclean all

uninstall: $(NAME)
	$(RM) $(DESTDIR)$(NAME)
	$(RM) $(MANDIR)$(COMPMAN)
	$(MANDB)

.PHONY: all clean cleanMan fclean install re uninstall
