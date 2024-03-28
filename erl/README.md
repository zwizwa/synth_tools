Erlang `studio`
---------------

This is a stripped down version of https://github.com/zwizwa/studio
which is no longer distributed separately.  Code will probably be
phased out eventually, but for now it is somewhat working so it will
be used as scaffolding.

The Erlang `studio` code and C and Rust code in this repository will
evolve together, as they are bound by a network protocol, so it makes
sense to put all the Erlang code in this repository.

The Erlang `studio` code runs on top of he public
https://github.com/zwizwa/erl_tools library and private `exo` service
modules, which are still separate, implementing the system layer
installed on each machine.


As for Erlang-side organization.  Since there are no separate
supervisors -- everything runs under the `exo` supervisor -- it seems
ok to expose the Erlang code as a collection of modules.  These will
be compiled using the main `rules.mk` Makefile.


