%% Command line shortcuts for synth tools.

%% Think of this as a configuration file / startup script / terminal
%% user interface for all synth_tools code.

%% In an ideal world, redo could be used to make a dependency-based
%% startup that can do partial restarts but in practice, the effort
%% needed to make that work correctly is just not worth it. So do the
%% dumb thing: idempotent startup, no shutdown.  This models an
%% always-on embedded system.

%% issues are in synth_job.org
%% time tracker is in synth_job.txt


-module(s).
-compile([export_all]).

%% Switch to dev mode.
dev() ->
    exo:load_erl("/i/exo/synth_tools/erl/s.erl"),
    lists:foreach(
      fun(Client) ->
              s:Client() ! {set_dir, "/i/exo/synth_tools/linux"}
      end,
      [hub,synth,pd]).
         
         
         

%% Startup.
init() -> init(erlang:node()).
init('exo@hyperion.zoo') -> pd();
init(_) -> error.

%% Idemptotent start of exo processes.
jack() ->
    application:ensure_all_started(exo),
    exo:need(jack_daemon).
control() ->
    Jack = jack(),
    maps:get(control, obj:get(Jack, clients)).

%% Daemon shortcuts without redo.

%% Idempotent start of a jack client's Erlang wrapper process and port
%% process.  Note that these are separate: the Erlang wrapper process
%% acts as a supervisor of the port process.

%% FIXME: I think this needs to wait a little longer.  Probably hub
%% client needs to be up.

jack_client(Name) when is_atom(Name) ->
    _ = jack(),
    BinName = atom_to_binary(Name),
    Pid = exo:need({jack_client, BinName}),  %% Erlang wrapper
    start(Pid), %% Port process
    Pid.

     

a2jmidid() -> jack_client(a2jmidid).
pd()       -> jack_client(pd).
clock()    -> jack_client(clock).
hub()      -> jack_client(hub).
synth()    -> jack_client(synth).

play()  -> clock() ! {midi, play}.
stop()  -> clock() ! {midi, stop}.

luna(M,F,A) -> rpc:call('exo@luna.zoo',M,F,A).
luna(M,F) -> luna(M,F,[]).



start(Pid) -> obj:call(Pid, start).
stop(Pid)  -> obj:call(Pid, stop).
restart(Pid) -> stop(Pid), start(Pid).



%% Command line glue for redo.

%% These used to call exo:need/1 directly, but I'm switching back to
%% redo.  Every time I bypass that mechanism I regret it.  So keep
%% this in mind: every time there is a desire to move out of redo and
%% put the functionality here, try to move it into a redo rule
%% that computes a value, and put only wrappers in this file.

%% Get the pid of the Erlang monitor process.  Note that the wrapped C
%% daemon might be running or not.

%% FIXME: It is not clear if this is all a good idea....  It would be
%% great if redo was easier to understand, but the tangle of
%% dependencies is inherently difficult to debug, so i end up just
%% bypassing it mostly.

%% pull_jack_client(Name) ->
%%     pull([jack_base_clients]),
%%     {Pid, _} = pull_val({jack_client,atom_to_binary(Name)}), Pid.
%% pull_jack() ->
%%     {Pid, Port} = pull_val(jack_daemon), Pid.


%% pull(Targets) -> exo:pull(Targets).
%% pull_vals(Targets) -> exo:pull_vals(Targets).
%% pull_val(Target) -> [Val] = pull_vals([Target]), Val.
%% pull() -> exo:pull().
     

%% loaded(Modules) -> pull([{loaded,Modules}]).

