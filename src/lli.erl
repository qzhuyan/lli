-module(lli).

-export([mac_refcnt/1]).

-type op() ::
    put
    | get
    | show
    %% Suicide OP below
    | crashme_put
    | crashme_get.

-spec mac_refcnt(op()) -> integer() | ok.
mac_refcnt(Op) ->
    lli_nif:mac_refcnt(Op).
