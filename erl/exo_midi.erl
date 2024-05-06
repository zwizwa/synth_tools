%% Global midi configuration

-module(exo_midi).
-export([clock_ports/0,
         port_id/1,
         db_init/1,
         jack_notify/2]).

%% DB=exo:db_local().
%% sqlite3:sql(DB,[{<<"select dst from jack_connect where src = ?">>,[<<"jack_clock:out">>]}]).
%% [[[<<"a2j:USB Midi 4i4o (playback): USB Midi 4i4o MIDI 3">>]]]

%% Next step: map input/output port notification to list of other ends
%% then send connect request to jack_control.

jack_notify(DB, Evt) ->
    exo:info("exo_midi:jack_notify(~999p)~n", [Evt]),
    Connect = 
        fun(Src,Dst) ->
                jack_daemon ! {connect, Src, Dst},
                ok
        end,
    case Evt of
        {port, reg, out, Port} ->
            Src = iolist_to_binary(Port),
            [Dsts] = sqlite3:sql(DB,[{<<"select dst from jack_connect where src = ?">>,[Src]}]),
            [Connect(Src,Dst) || [Dst] <- Dsts],
            ok;
        {port, reg, in, Port} ->
            Dst = iolist_to_binary(Port),
            [Srcs] = sqlite3:sql(DB,[{<<"select src from jack_connect where dst = ?">>,[Dst]}]),
            [Connect(Src,Dst) || [Src] <- Srcs],
            ok;
        _ ->
            ok
    end.

db_init(DB) ->
    [[],[],[]] = sqlite3:sql(DB,
      [{<<"create table if not exists midiport ("
          "  port_id   INTEGER PRIMARY KEY NOT NULL,"
          "  port_name TEXT    NOT NULL"
          ");">>,[]},
       {<<"create table if not exists midiclock ("
          "  port_name  TEXT    PRIMARY KEY NOT NULL,",
          "  ts         INTEGER,"
          "  enable     TEXT",
          ");">>,[]},
       {<<"create view if not exists midiclock_mask as"
          "  select sum(1<<port_id) from midiclock left join midiport on midiclock.port_name = midiport.port_name;">>,[]}
      ]),
    ok.

clock_ports() ->
    [<<"USB-Midi-4i4o-MIDI-1">>,
     <<"USB-Midi-4i4o-MIDI-2">>,
     <<"USB-Midi-4i4o-MIDI-3">>,
     <<"USB-Midi-4i4o-MIDI-4">>,
     <<"MicroBrute-MIDI-1">>,
     <<"TD-3-MIDI-1">>,
     <<"Arturia-MicroFreak-MIDI-1">>].

port_id(Name) ->
    Ports = 
        #{<<"CH345-MIDI-1">>                   =>  0, %% solderstation cheapo
          <<"BCR2000-MIDI-1">>                 =>  1, %% Behringer rotary control
          <<"BCR2000-MIDI-2">>                 =>  2, %% ...
          <<"BCR2000-MIDI-3">>                 =>  3, %% ...
          <<"WORLDE-easy-control-MIDI-1">>     =>  4, %% Worlde rotary + slider
          <<"USB-Midi-4i4o-MIDI-1">>           =>  5, %% Midi box to Volcas
          <<"USB-Midi-4i4o-MIDI-2">>           =>  6, %% ...
          <<"USB-Midi-4i4o-MIDI-3">>           =>  7, %% ...
          <<"USB-Midi-4i4o-MIDI-4">>           =>  8, %% ...
          <<"LPK25-MIDI-1">>                   =>  9, %% AKAI small keyboard
          <<"M-Audio-Delta-1010-MIDI">>        => 10, %% Delta1010
          <<"Axiom-25-MIDI-1">>                => 11, %% Small M-Audio
          <<"Axiom-25-MIDI-2">>                => 12, %% ...
          <<"Axiom-25-MIDI-3">>                => 13, %% ...
          <<"MicroBrute-MIDI-1">>              => 14, %% Arturia Microbrute
          <<"MicroBrute-MIDI-2">>              => 15, %% ...
          <<"USB-Device-0x5f9-0xfff0-MIDI-1">> => 16, %% Staapl Sheep
          <<"TD-3-MIDI-1">>                    => 17, %% Behringer TD-3
          <<"Arturia-MicroFreak-MIDI-1">>      => 18, %% Arturia Microfreak
          <<"Keystation-88-MK3-MIDI-1">>       => 19, %% Large M-Audio
          <<"Keystation-88-MK3-MIDI-2">>       => 20, %% ...
          <<"FL-STUDIO-FIRE-MIDI-1">>          => 21, %% AKAI FIRE
          <<"UMA25S-MIDI-1">>                  => 22, %% Behringer U-Control
          <<"jack_clock">>                     => 23  %% synth_tools jack_clock.c
         },
    maps:get(Name, Ports, 0).
