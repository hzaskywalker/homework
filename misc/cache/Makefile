edit: cache test_cache server step_test

cache: cache.h shareinfo.h cache.c 
	gcc cache.c -o cache -Wall #-DDEBUG

test_cache: test_cache.c cache.h
	gcc test_cache.c -o test_cache -Wall #-DDEBUG

server: server.c shareinfo.h
	gcc server.c -o server -Wall #-DDEBUG

step_test: step_test.c cache.h
	gcc step_test.c -o step_test -Wall

clean:
	rm -f  cache server test_cache step_test cache_record.txt shareinfo flag test_example *.txt
