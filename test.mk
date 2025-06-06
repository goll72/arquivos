TESTS = 1 2 3 4 5 6 7 8 9 10 11 12 a1 a2 a3 a4 a5 a6 a7 a8

TESTS_RUN = $(patsubst %,$(BUILD)/tests/%.run,$(TESTS))
TESTS_FAILED = $(patsubst %,%.failed,$(TESTS_RUN))

# Binário estático para que não tenhamos que
# nos preocupar com bibliotecas dinâmicas
HASH = \#
BASECFLAGS += -static

GEN += $(TESTS_RUN) $(TESTS_FAILED)

test: $(TESTS_RUN)

$(BUILD)/tests/%.run: tests/%.in tests/%.out $(EXE) | $(dir $(TESTS_RUN)) $(BUILD)/generated/ $(BUILD)/empty/
	bwrap \
	    --overlay-src tests/in \
	    --overlay $(BUILD)/generated $(BUILD)/empty /scratch \
	    --ro-bind $(EXE) /arquivos \
	    --chdir /scratch \
	    /arquivos < tests/$*.in > $@

	@{ ./scripts/compare-bin-files.sh $(BUILD)/generated tests/out && git diff --no-index -U100 $@ tests/$*.out; } || mv $@ $@.failed

	@rmdir $(BUILD)/empty/work
	@find $(BUILD)/generated -type f -delete
