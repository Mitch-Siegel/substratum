

TEST_TYPES_DIRS = component unit

.PHONY: $(TEST_TYPES_DIRS)

programs: aggregate-coverage.info

aggregate-coverage.info: $(TEST_TYPES_DIRS)
	lcov $(addprefix -a ,$(shell find -name "*.info")) -o $@
	genhtml $@ -o $(basename $@)-report

$(TEST_TYPES_DIRS):
	$(MAKE) -C $@ coverage

clean:
	@for dir in $(TEST_TYPES_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done

info:
	$(info "TEST_TYPES_DIRS: $(TEST_TYPES_DIRS)")