TESTS = tests/1 tests/2 tests/3 tests/4 tests/5 tests/6 tests/7 tests/8 tests/9 tests/10 tests/11 tests/12
TESTS_RUN = $(patsubst %,$(BUILD)/%.run,$(TESTS))

# Binário estático para que não tenhamos que
# nos preocupar com bibliotecas dinâmicas
HASH = \#
BASECFLAGS += -static

GEN += $(TESTS_RUN)

test: $(TESTS_RUN)

$(BUILD)/%.run: %.in %.out $(EXE) | $(dir $(TESTS_RUN)) $(BUILD)/generated/ $(BUILD)/empty/
	bwrap \
	    --overlay-src tests/in \
	    --overlay $(BUILD)/generated $(BUILD)/empty /scratch \
	    --ro-bind $(EXE) /arquivos \
	    --chdir /scratch \
	    /arquivos < $*.in > $@

	@./scripts/compare-bin-files.sh $(BUILD)/generated tests/out
	@diff -u -U100 $(BUILD)/$*.run $*.out || mv $@ $@.failed

	@rmdir $(BUILD)/empty/work
	@find $(BUILD)/generated -type f -delete
