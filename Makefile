
CPPFLAGS := -D_GNU_SOURCE $(CPPFLAGS)
CFLAGS := -std=c99 -Wall -Wextra -pedantic $(CFLAGS)
LDLIBS = -lattr -lacl -lcap

metacp: metacp.c metacp.h

clean:
	$(RM) metacp

print-%:
	@echo $($*)
