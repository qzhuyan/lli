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
-endif.
