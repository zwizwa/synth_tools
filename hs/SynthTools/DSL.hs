-- DSL class and generic DSL library code

{-# LANGUAGE TypeFamilies #-} -- data Arr (n :: Nat)
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE ScopedTypeVariables #-} -- arrLength
{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)

module SynthTools.DSL where

import Data.Dynamic
import Data.Proxy
import Data.IntMap.Strict

import GHC.TypeNats
import Prelude hiding (const)




-- Number types need to be representable in C.  This class ensures
-- that.  Note that in Eval semantics the Haskell type is used.  Also
-- add the Typeable constraint here needed by toDyn for Data.Reify

data Prim = Add | Sub | Mul | Abs | Signum deriving (Show)

-- The ability to reify a represented type needs to be a property of
-- the DSL not just the implementation, because the class constraint
-- will need to go in the definition of the class members.  For Eval
-- this is unused but can be toDyn.  For Comp it is essential as types
-- will need to be representable in C eventually.

data Type = TFloat | TInt | TBits Int
          | TArray Int Type
          | TPair Type Type
          deriving (Show)


isTArray (TArray _ _) = True
isTArray _ = False

-- Phantom tag for fixed length arrays and random access memories.
-- Used as concrete representation in Eval so we do need a
-- constructor.
data Arr (n :: Nat) a = Arr [a]
data Mem (n :: Nat) a d = Mem [(a,d)]


class Typeable t => DSLType r t where
  dslType :: r t -> Type

instance (DSLType r a, DSLType r b) => DSLType r (a, b) where
  dslType _ = TPair (dslType a') (dslType b') where
    a' = undefined :: r a
    b' = undefined :: r b

instance DSLType r Int   where dslType _ = TInt
instance DSLType r Float where dslType _ = TFloat
-- FIXME: Bit vectors

arrLength :: forall (n :: Nat) a r. KnownNat n => r (Arr n a) -> Natural
arrLength _ = natVal (Proxy :: Proxy n)

instance (KnownNat n, DSLArr r, DSLType r t) => DSLType r (Arr n t) where
  dslType a = TArray size typ where
    -- Separate type level function mapping Arr to base type is not
    -- needed because we already have ref in DSL.
    rt = ref undefined undefined :: r t
    typ = dslType rt
    size = fromIntegral $ arrLength a

-- The DSL is split into a couple of classes to make it cleaner to
-- specify that library code only needs a subset of functionality.
-- E.g. the Verilog target does not support data structures (array and
-- pair -- those would all be macro-level) but it does support a
-- generalization of signal (memory).

-- Structural
class DSLPair r where
  pack    :: (DSLType r a, DSLType r b) => r a -> r b -> r (a, b)
  unpack  :: (DSLType r a, DSLType r b) => r (a, b) -> (r a, r b)

class DSLArr r where
  array   :: (DSLType r t, KnownNat n)  => (r Int -> r t) -> r (Arr n t)
  ref     :: (DSLType r t)              => r (Arr n t) -> r Int -> r t

class DSLSig r where
  signal  :: (DSLType r s, DSLType r t) => r s -> (r s -> (r s, r t)) -> r t

class DSLMem r where
  -- Generalization of DSLSig where the state is a random access
  -- memory.  Interface is pure.  A Monad could be used at the library
  -- end to make this more user friendly, but the monad is kept out of
  -- the interface here.
  memory  :: (DSLType r d, DSLType r a, KnownNat n) => r (Mem n a d) -> (r (Mem n a d) -> (r (Mem n a d), r t)) -> r t
  memGet  :: (DSLType r d, DSLType r a, KnownNat n) => r a -> r (Mem n a d) -> r d
  memSet  :: (DSLType r d, DSLType r a, KnownNat n) => r a -> r d -> r (Mem n a d) -> r (Mem n a d)

-- Primitive constants
class DSLType r t => DSLConst r t where
  const   ::                               t -> r t

-- Primitive operations
class DSLType r t => DSLPrim r t where
  op1     :: (DSLType r t)              => Prim -> r t -> r t
  op2     :: (DSLType r t)              => Prim -> r t -> r t -> r t
  
  










-- Library functions

ramp :: (DSLSig r, DSLConst r t, Num (r t)) => t -> r t
ramp init = signal (const init) (\s -> (s + 1, s))


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

