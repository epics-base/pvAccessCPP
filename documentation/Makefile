
all: gen

clean:
	rm -rf doxygen_sqlite3.db html

gen: libstdc++.tag
	doxygen

commit: gen
	touch html/.nojekyll
	./commit-gh.sh documentation/html/ html/.nojekyll html/*.* html/search/*.*

libstdc++.tag:
	wget -O $@ https://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/libstdc++.tag

.PHONY: all clean gen commit
