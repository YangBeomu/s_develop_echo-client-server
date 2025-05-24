PROJECTS := echo-client echo-client-server

.PHONY: all clean qmake_make

all: qmake_make

qmake_make:
	@for proj in $(PROJECTS); do \
		echo "Running qmake and make for $$proj"; \
		(cd src/$$proj && qmake && make); \
	done

clean:
	@for proj in $(PROJECTS); do \
		echo "Cleaning $$proj"; \
		(cd src/$$proj && make clean); \
	done

