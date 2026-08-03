# Evil makfile containing various run commands

PROJECT_ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

.PHONY: build-core build-example-function

build-core:
	cargo run -- -i $(PROJECT_ROOT)/core/lib.sb -crate-name core -crate-path ${PROJECT_ROOT}/core

build-example-function:
	cargo run -- -i $(PROJECT_ROOT)/examples/function.sb -crate-path ${PROJECT_ROOT}/examples -crate-name function -bin function