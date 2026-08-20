-module(lli).

-export([mac_refcnt/1, process_binary/2, unsafe_copy_binary/2]).

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

-doc """
Copies the complete underlying binary allocation identified by `BinaryId` from
`erlang:process_info(Pid, binary)`.

This function synchronously suspends `Pid`, obtains fresh binary metadata, and
checks that `BinaryId` is still owned by the process before invoking the unsafe
native memory copy. The suspension is always released afterward.

This is debugging functionality, not a memory-safety guarantee. The process can
still terminate or perform system work while suspended. The returned data is
the complete underlying allocation; subbinary offsets and lengths are not
available in `process_info/2`.
""".
-spec process_binary(pid(), non_neg_integer()) ->
    {ok, binary()}
    | {error,
        bad_address
        | bad_header
        | binary_not_found
        | changed_during_read
        | cannot_suspend
        | not_supported
        | process_exited
        | size_mismatch
        | too_large
        | unsupported_binary}.
process_binary(Pid, BinaryId) when is_pid(Pid), is_integer(BinaryId), BinaryId >= 0 ->
    try erlang:suspend_process(Pid) of
        true ->
            try
                copy_suspended_process_binary(Pid, BinaryId)
            after
                %% The target may have exited while the unsafe copy was running.
                try
                    erlang:resume_process(Pid)
                catch
                    error:badarg -> ok
                end
            end
    catch
        error:badarg ->
            {error, cannot_suspend}
    end;
process_binary(_Pid, _BinaryId) ->
    {error, badarg}.

-doc """
Copies bytes from an ERTS `Binary *` address without suspending or checking its
owner. Prefer `process_binary/2`.

This operation is intentionally unsafe and tied to the exact ERTS internal
`Binary` layout against which the NIF was compiled.
""".
-spec unsafe_copy_binary(non_neg_integer(), non_neg_integer()) ->
    {ok, binary()} | {error, atom()}.
unsafe_copy_binary(BinaryId, BinarySize) ->
    lli_nif:unsafe_copy_binary(BinaryId, BinarySize).

copy_suspended_process_binary(Pid, BinaryId) ->
    case erlang:process_info(Pid, binary) of
        {binary, BinaryInfos} ->
            case lists:keyfind(BinaryId, 1, BinaryInfos) of
                {BinaryId, BinarySize, _Refc} ->
                    unsafe_copy_binary(BinaryId, BinarySize);
                false ->
                    {error, binary_not_found}
            end;
        undefined ->
            {error, process_exited}
    end.
