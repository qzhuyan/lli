##
# Project Title
#
# @file
# @version 0.1

nif:
	@echo "Compiling NIF..."
	@cmake . -B c_build
	@cmake --build c_build

erl:
	@echo "Compiling Erlang..."
	rebar3 compile

fmt:
	@clang-format-14 -i c_src/*
	@rebar3 fmt

all: nif erl


# end
