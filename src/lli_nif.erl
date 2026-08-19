-module(lli_nif).
-export([mac_refcnt/1, unsafe_copy_binary/2]).
-on_load(init/0).

-define(APPNAME, lli_nif).
-define(LIBNAME, liblli_nif).

mac_refcnt(_) ->
    not_loaded(?LINE).

-spec unsafe_copy_binary(non_neg_integer(), non_neg_integer()) ->
    {ok, binary()}
    | {error,
        bad_address
        | bad_header
        | changed_during_read
        | not_supported
        | size_mismatch
        | too_large
        | unsupported_binary}.
unsafe_copy_binary(_BinaryId, _BinarySize) ->
    not_loaded(?LINE).

init() ->
    LoadArg = 0,
    SoName =
        case code:priv_dir(?APPNAME) of
            {error, bad_name} ->
                case filelib:is_dir(filename:join(["..", priv])) of
                    true ->
                        filename:join(["..", priv, ?LIBNAME]);
                    _ ->
                        filename:join([priv, ?LIBNAME])
                end;
            Dir ->
                filename:join(Dir, ?LIBNAME)
        end,
    case load_patch() of
        {true, Patch} ->
            erlang:load_nif(Patch, LoadArg);
        false ->
            erlang:load_nif(SoName, LoadArg)
    end.

not_loaded(Line) ->
    erlang:nif_error({not_loaded, [{module, ?MODULE}, {line, Line}]}).

load_patch() ->
    {ok, CWD} = file:get_cwd(),
    Patch = filename:join([CWD, patches, ?LIBNAME]),
    case filelib:is_file(Patch ++ ".so") of
        true ->
            {true, Patch};
        false ->
            false
    end.
