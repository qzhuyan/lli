.PHONY: default
default: nif erl

nif:
	@echo "Compiling NIF..."
	@cmake . -B c_build
	@cmake --build c_build

erl:
	@echo "Compiling Erlang..."
	rebar3 compile

.PHONY: fmt
fmt:
	@clang-format-14 -i c_src/*
	@rebar3 fmt


.PHONY: clean
clean:
	@rebar3 unlock --all
	@rm -rf _build c_build _release

.PHONY: test
test:
	@rebar3 eunit

.PHONY: release
release: nif erl
	@mkdir -p _release/out/
	@cp _build/default/lib/lli/priv/liblli_nif.so _release/out/
	@cp _build/default/lib/lli/ebin/*.beam _release/out/
	@tar czf _release/lli.tar.gz -C _release/out/ .

# end
