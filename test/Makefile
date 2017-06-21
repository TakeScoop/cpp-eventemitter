TEST_INPUTS=$(wildcard test_*.cpp)
TESTS=$(patsubst %.cpp,%.testrunner,$(TEST_INPUTS))
CATCHHEADER=$(shell node -e 'require("catch")')/catch/single_include

test: $(TESTS)
	[ -f ./$< ] && ./$< -s
	npm run jstests

$(TESTS): $(TEST_INPUTS)
	g++ -std=c++11 -ggdb -Wall -Wextra -isystem $(CATCHHEADER) -o $@ $< 

clean:
	rm $(TESTS)
