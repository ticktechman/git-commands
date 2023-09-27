###############################################################################
##
##       filename: makefile
##    description:
##        created: 2023/08/24
##         author: ticktechman
##
###############################################################################
.PHONY: all build test install doc

GIT_TOOLS := git-index-cat git-largefiles

all: $(GIT_TOOLS)

git-index-cat: src/git-index-cat.c
	gcc -o $@ $^
git-largefiles: src/git-largefiles.py
	cp $^ $@

install: $(GIT_TOOLS)
	sudo cp $(GIT_TOOLS) /usr/local/bin/

clean:
	rm -f $(GIT_TOOLS)
###############################################################################
