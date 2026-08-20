-module(lli_test).

-ifdef(TEST).
-include_lib("eunit/include/eunit.hrl").

lli_nif_test_() ->
    [
        ?_assertMatch(X when X >= 1, lli_nif:mac_refcnt(show)),
        ?_assertMatch(X when X >= 1, lli_nif:mac_refcnt(get)),
        ?_assertEqual(ok, begin
            lli_nif:mac_refcnt(get),
            lli_nif:mac_refcnt(put)
        end),
        ?_assertEqual(ok, begin
            Start = lli_nif:mac_refcnt(show),
            io:format("Start with: ~p~n", [Start]),
            ?assert(Start + 1 == lli_nif:mac_refcnt(get)),
            ?assert(Start + 2 == lli_nif:mac_refcnt(get)),
            ?assert(Start + 3 == lli_nif:mac_refcnt(get)),
            ?assert(Start + 4 == lli_nif:mac_refcnt(get)),
            ?assertEqual(ok, lli_nif:mac_refcnt(put)),
            ?assert(Start + 3 == lli_nif:mac_refcnt(show)),
            ok
        end)
    ].

process_binary_test_() ->
    {setup,
        fun() ->
            Bin = binary:copy(<<0, 1, 2, 3, 4, 5, 6, 7>>, 512),
            Pid = spawn(fun() -> binary_holder(Bin) end),
            {Pid, Bin}
        end,
        fun({Pid, _Bin}) ->
            Pid ! stop
        end,
        fun({Pid, Bin}) ->
            {binary, BinaryInfos} = erlang:process_info(Pid, binary),
            Size = byte_size(Bin),
            Matching = [Info || Info = {_, BinarySize, _} <- BinaryInfos, BinarySize =:= Size],
            ?assertMatch([_ | _], Matching),
            [{BinaryId, Size, _} | _] = Matching,
            [
                ?_assertEqual({ok, Bin}, lli:process_binary(Pid, BinaryId)),
                ?_assertEqual({error, badarg}, lli:process_binary(not_a_pid, BinaryId)),
                ?_assertEqual({error, badarg}, lli:process_binary(Pid, -1))
            ]
        end}.

binary_holder(Bin) ->
    receive
        {size, From} ->
            From ! {size, byte_size(Bin)},
            binary_holder(Bin);
        stop ->
            ok
    end.

lli_nif_unload_test_() ->
    [
        ?_assertMatch(false, code:purge(lli_nif)),
        ?_assertMatch(true, code:delete(lli_nif)),
        %% check that the module could be safely unloaded
        ?_assertException(error, undef, lli_nif:module_info())
    ].
-endif.
