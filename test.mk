TESTS = 1 2 3 4 5 6 7 8 9 10 11 12 a1 a2 a3 a4 a5 a6 a7 a8

TESTS_RUN = $(TESTS:%=$(BUILD)/tests/%.run)
TESTS_FAILED = $(TESTS_RUN:.run=.failed)


GEN += $(TESTS_RUN) $(TESTS_FAILED)


DYN_DEPS = $(shell ldd $(EXE) | awk '/=>/ { print $$3 }') \
           $(shell readelf -l $(EXE) | grep -Po '(?<=\[Requesting program interpreter: )''.*''(?=])')

BWRAP_BIND_DEPS = $(foreach dep,$(DYN_DEPS),--ro-bind $(dep) $(dep))

BWRAP_ARGS = \
    --overlay-src tests/in \
    --overlay $(BUILD)/generated $(BUILD)/empty /scratch \
    --ro-bind $(EXE) /arquivos \
    $(BWRAP_BIND_DEPS) \
    --proc /proc \
    --chdir /scratch


TEST ?= $(firstword $(TESTS))

test: $(TESTS_RUN)

# Gambiarra, mas colocar o lldb numa sandbox daria muito trabalho
debug: tests/$(TEST).in $(EXE)
	-cd tests/in && lldb -o "process launch -s -i ../$(TEST).in" ../../$(EXE)
	git clean -f tests/in
	git restore tests/in

clean-tests:
	@rm -f $(TESTS_RUN) $(TESTS_FAILED)

$(BUILD)/tests/%.run: tests/%.in tests/%.out $(EXE) | $(dir $(TESTS_RUN)) $(BUILD)/generated/ $(BUILD)/empty/
	bwrap $(BWRAP_ARGS) \
	    /arquivos < tests/$*.in > $@

	@{ ./scripts/diff-trees.sh $(BUILD)/generated tests/out && ./scripts/diff.sh $@ tests/$*.out; } || mv $(BUILD)/tests/$*.run $(BUILD)/tests/$*.failed

	@rmdir $(BUILD)/empty/work
	@find $(BUILD)/generated -type f -delete

# Usado para guardar as configurações de diff do git
$(EXE): .git/config

.git/config: .gitconfig
	@git config include.path ../.gitconfig
	@touch $@

.PHONY: test debug clean-tests
