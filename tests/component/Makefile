SBCC = ../../sbcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
RISCV_ARGS = -march=rv64g
sh=zsh

COMMON_DIR = ./common/
COVERAGE_DIR = ./coverage/
SBCC_BUILD_DIR = ../../build
COMPONENT_TEST_COVERAGE_NAME = "component-test-coverage"

ALL_TESTS_DIRS = $(filter-out $(COMMON_DIR) $(COVERAGE_DIR) ./ .., $(sort $(dir $(wildcard ./*/))))
ALL_TESTS = $(patsubst ./%/,%,$(ALL_TESTS_DIRS))
ALL_TESTS_OUTPUTS = $(foreach test,$(ALL_TESTS), $(test)/$(test).output)
ALL_TESTS_ELFS = $(foreach test,$(ALL_TESTS), $(test)/$(test).elf)

.DEFAULT_GOAL = all

.PHONY: always all $(ALL_TESTS_OUTPUTS)

always:

all: $(ALL_TESTS_OUTPUTS) always
# if we make it down here we have successfully run all individual tests
	$(info "All tests passed.")

coverage: $(ALL_TESTS_ELFS)
	mkdir -p $(COVERAGE_DIR)
	lcov $(addprefix -a ,$(shell find -name "*.cov")) -o $(COVERAGE_DIR)/$(COMPONENT_TEST_COVERAGE_NAME).info

coverage-report: coverage
	genhtml $(COVERAGE_DIR)/$(COMPONENT_TEST_COVERAGE_NAME).info -o $(COVERAGE_DIR)/$(COMPONENT_TEST_COVERAGE_NAME)-report

$(ALL_TESTS_ELFS):
	$(MAKE) -C $(subst /,,$(dir $@)) $(subst /,,$(dir $@)).elf

$(ALL_TESTS_OUTPUTS):
	$(MAKE) -C $(subst /,,$(dir $@)) $(subst /,,$(dir $@)).output

info:
	$(info "ALL_TESTS_DIRS: $(ALL_TESTS_DIRS)")
	$(info "ALL_TESTS: $(ALL_TESTS)")
	$(info "ALL_TESTS_OUTPUTS: $(ALL_TESTS_OUTPUTS)")
	$(info "ALL_TESTS_ELFS: $(ALL_TESTS_ELFS)")
	$(info "PROGRAMS: $(programs)")

clean:
	@for dir in $(ALL_TESTS_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -rf coverage
