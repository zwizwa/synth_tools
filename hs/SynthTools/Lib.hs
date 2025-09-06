
{-# LANGUAGE TypeFamilies #-} -- data Arr (n :: Nat)
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE ScopedTypeVariables #-} -- arrLength
{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)

-- Library functions in terms of DSL
module SynthTools.Lib where

import SynthTools.DSL
import Prelude hiding (const)


-- Library functions

ramp :: (DSLSig r, DSLConst r t, Num (r t)) => t -> r t
ramp init = signal (const init) (\s -> (s + 1, s))

exponential :: (DSLSig r, DSLConst r t, Num t, Num (r t)) => r t -> r t
exponential decay = signal (const 1) (\s -> (s * decay, s))


-- Test fuction for composite state
swap :: (DSLSig r, DSLPair r, DSLType r (t, t), DSLConst r t)
     => t -> t -> r t
swap ia ib = signal iab update where
  iab = pack (const ia) (const ib)
  update s =
    let (sa, sb) = unpack s
        s' = pack sb sa  -- flip states
        out = sa
    in (s', out)


-- These are for Num instances.  See Eval.hs and Comp.hs
type DSLOp1 r t = r t -> r t
type DSLOp2 r t = r t -> r t -> r t

add'    :: (DSLPrim r t) => DSLOp2 r t ; add'    = op2 Add
sub'    :: (DSLPrim r t) => DSLOp2 r t ; sub'    = op2 Sub
mul'    :: (DSLPrim r t) => DSLOp2 r t ; mul'    = op2 Mul
abs'    :: (DSLPrim r t) => DSLOp1 r t ; abs'    = op1 Abs
signum' :: (DSLPrim r t) => DSLOp1 r t ; signum' = op1 Signum

div'    :: (DSLPrim r t) => DSLOp2 r t ; div'    = op2 Div




