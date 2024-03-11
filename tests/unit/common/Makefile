SBCC = ../../../sbcc

COMMON_DIR = ../common
SBCC_BUILD_DIR = ../../../build
COVERAGE_BASE_DIR = ./coverage

TEST_NAME = $(notdir $(patsubst %/,%,$(CURDIR)))

.DEFAULT_GOAL = $(TEST_NAME).output

.PHONY: always

always:

programs: $(TEST_NAME).output

%.S: %.sb
	$(eval COV_DEST_DIR = $(COVERAGE_BASE_DIR)/$(basename $@))
	mkdir -p $(COV_DEST_DIR)
	$(SBCC) -i $^ -o $(basename $@).S -I $(COMMON_DIR) -I $(shell dirname $@)
	@mkdir -p $(COV_DEST_DIR)
	cp $(SBCC_BUILD_DIR)/*.gcda $(COV_DEST_DIR)
	@geninfo --build-directory $(SBCC_BUILD_DIR) $(COV_DEST_DIR) -o $(COV_DEST_DIR)/$(basename $@).cov


%.output: %.sb %.txt always
	@-$(SBCC) -i $< -o /dev/null > $@ 2>&1 || true
	@GREP_OUTPUT=$$(grep -xFf $@ $(word 2,$^)); \
	if [ "$$(echo "$$GREP_OUTPUT" | wc -c)" -ne "$$(wc -c < $(word 2,$^))" ] || \
		[ "$$(echo "$$GREP_OUTPUT" | wc -w)" -ne "$$(wc -w < $(word 2,$^))" ] || \
		[ "$$(echo "$$GREP_OUTPUT" | wc -l)" -ne "$$(wc -l < $(word 2,$^))" ]; then \
   		echo "Test '$(TEST_NAME)' failed"; \
		echo "output:"; \
		echo $$GREP_OUTPUT; \
		exit 1; \
		else \
		echo "Test '$(TEST_NAME)' passed"; \
	fi

info:
	$(info "SRCS = $(SRCS)")
	$(info "ASMS = $(ASMS)")
	$(info "OBJS = $(OBJS)")

clean:
	@rm -f *.S
	@rm -f *.o
	@rm -f *.elf
	@rm -f *.output
	@rm -rf coverage


