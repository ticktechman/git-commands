###############################################################################
##
##       filename: makefile
##    description:
##        created: 2023/08/24
##         author: ticktechman
##
###############################################################################
.PHONY: all build test install doc

GIT_TOOLS := git-index-show

all: $(GIT_TOOLS)

git-index-show: src/git-index-show.c
	gcc -o $@ $^

clean:
	rm -f $(GIT_TOOLS)
###############################################################################
