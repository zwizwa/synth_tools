#lang racket/base

;; What does midi routing look like from the point of specification?

;; The point of implementation is simple: there is a processor for
;; each port, which handles a couple of different cases, updates state
;; and sends out new messages.

(display "midiroute\n")

;; The problem is that these have to be state machines and I fucking
;; hate statemachines because it creates a state representation
;; problem, not just messages, which are relatively easy to manage.
