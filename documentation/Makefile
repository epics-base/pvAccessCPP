
all: gen commit

clean:
	rm -rf doxygen_sqlite3.db html

gen:
	doxygen

commit:
	touch html/.nojekyll
	./commit-gh.sh documentation/html/ html/.nojekyll html/*.* html/search/*.*

.PHONY: all clean gen commit
