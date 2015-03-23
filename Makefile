all::

obj-cla = cla.o proto.o tcp.o ballot.o accept_spawn.o
obj-ctf = ctf.o proto.o tcp.o accept_spawn.o tabulate.o
obj-query = query.o proto.o tcp.o
obj-vote = vote.o proto.o tcp.o ballot.o
obj-results = results.o proto.o tcp.o ballot.o
TARGETS = cla ctf query vote results

CFLAGS = -ggdb3 -Og
LDFLAGS=

ALL_CFLAGS  = $(CFLAGS) -std=gnu99 -MMD -Wall -Wextra
ALL_LDFLAGS = $(LDFLAGS) -pthread -Wl,-O1,--as-needed,--gc-sections

ifneq ($(LTO),)
ALL_CFLAGS += -flto
ALL_LDFLAGS += $(ALL_CFLAGS) -fuse-linker-plugin -fwhole-program
endif

CC     = $(CROSS_COMPILE)gcc
LINK   = $(CC)
LEX    = flex
YACC   = bison
RM     = rm -f

ifndef V
	QUIET_CC   = @ echo '    CC  ' $@;
	QUIET_LINK = @ echo '    LINK' $@;
	QUIET_LEX  = @ echo '    LEX ' $@;
	QUIET_YACC = @ echo '    YACC' $@;
endif

.SECONDARY:
.PHONY: FORCE

### Detect prefix changes
## Use "#')" to hack around vim highlighting.
TRACK_CFLAGS = $(CC):$(subst ','\'',$(ALL_CFLAGS)) #')

.TRACK-CFLAGS: FORCE
	@FLAGS='$(TRACK_CFLAGS)'; \
	    if test x"$$FLAGS" != x"`cat .TRACK-CFLAGS 2>/dev/null`" ; then \
		echo 1>&2 "    * new build flags or prefix"; \
		echo "$$FLAGS" >.TRACK-CFLAGS; \
            fi

TRACK_LDFLAGS = $(LINK):$(subst ','\'',$(ALL_LDFLAGS)) #')

.TRACK-LDFLAGS: FORCE
	@FLAGS='$(TRACK_LDFLAGS)'; \
	    if test x"$$FLAGS" != x"`cat .TRACK-LDFLAGS 2>/dev/null`" ; then \
		echo 1>&2 "    * new link flags"; \
		echo "$$FLAGS" >.TRACK-LDFLAGS; \
            fi

all:: $(TARGETS)

iloc.yy.o: iloc.tab.h
iloc.tab.o iloc.yy.o: ALL_CFLAGS:=$(filter-out -Wextra,$(ALL_CFLAGS))

.SECONDEXPANSION:
$(TARGETS) : .TRACK-LDFLAGS .TRACK-CFLAGS $$(obj-$$@)
	$(QUIET_LINK)$(LINK) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o \
		$@ $(filter-out .TRACK-LDFLAGS,$(filter-out .TRACK-CFLAGS,$^))

%.o : %.c .TRACK-CFLAGS
	$(QUIET_CC)$(CC) $(ALL_CFLAGS) -c -o $@ $<

%.yy.c : %.l
	$(QUIET_LEX)$(LEX) -P"$(<:.l=)_" -o$@ $<

%.tab.c %.tab.h : %.y
	$(QUIET_YACC)$(YACC) -d -b $(<:.y=) -p "$(<:.y=)_" $<

.PHONY: clean
obj-all = $(foreach target,$(TARGETS),$(obj-$(target)))
clean :
	$(RM) $(TARGETS)
	$(RM) $(obj-all)
	$(RM) $(obj-all:.o=.d)
	$(RM) *.tab.[ch] *.yy.[ch] *.output
	$(RM) .TRACK-CFLAGS .TRACK-LDFLAGS

-include $(obj-all:.o=.d)
