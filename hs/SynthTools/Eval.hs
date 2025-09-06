-- Evaluator instance for DSL class

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE TypeSynonymInstances #-}

module SynthTools.Eval where

import SynthTools.DSL
import SynthTools.Lib
import Data.Stream hiding (fromList)
import Data.Dynamic
import Data.Ratio
import GHC.Float
import Prelude hiding (const, zipWith)


-- Generic evaluator representation type, parameterized by base type.
-- See below for Int, Float and error analysis base types.  We reuse
-- the element-wise Applicative that is defined on Stream.
newtype Eval t = Eval { unEval :: Stream t }
  deriving (Functor, Applicative)


-- 1. Language structure

instance DSLSig Eval where

  signal init update = v where
    -- Note that the initial value is encoded as a stream where we
    -- sample the first value.  This is done in order to avoid having
    -- to represent both scalars and streams in the language which
    -- really complicates things at this point.  Maybe later when data
    -- representation is handled better this can be changed.
    Eval (Cons init0 _) = init
    
    -- Given init0 we can just tie the knot
    (Eval s, v) = update $ Eval $ Cons init0 s

instance DSLPair Eval where

  -- Attempt to give _some_ semantics to data structures before
  -- implementing Comp instance.
  pack (Eval a) (Eval b) = Eval $ zipWith (,) a b
  unpack (Eval ab) = (Eval $ fmap fst ab, Eval $ fmap snd ab)

instance DSLArr Eval where

  array f = arr where
    arr = Eval $ fmap Arr streams
    len = fromIntegral $ arrLength arr
    streams = distribute $ fmap stream $ [0..len-1]
    stream i = s where Eval s = f $ const i
  
  ref (Eval as) (Eval is) = Eval es where
    es = liftA2 ref' as is
    ref' (Arr a) i = a Prelude.!! i


-- 2. Language primitives

-- 2.1. It's simplest to just rely on the Haskell Num class and the
-- Applicative instance of Stream ...

eval2 Add = liftA2 (+)
eval2 Sub = liftA2 (-)
eval2 Mul = liftA2 (*)

eval21 Div = undefined -- FIXME

eval1 Abs    = fmap abs
eval1 Signum = fmap signum

instance DSLPrim Eval Int    where op1 = eval1 ; op2 = eval2
instance DSLPrim Eval Float  where op1 = eval1 ; op2 = eval2
instance DSLPrim Eval Double where op1 = eval1 ; op2 = eval2

instance DSLConst Eval Int    where const = Eval . pure
instance DSLConst Eval Float  where const = Eval . pure
instance DSLConst Eval Double where const = Eval . pure


-- 2.32 ... and define instances Num for each supported primitive.

-- Note that I could not avoid the need for UndecidableInstances
-- without spelling it out for each base type.
--
---- This needs UndecidableInstances.  Probably ok, but might hide
---- other problems so let's not.
--
-- instance (Num t, DSLPrim r t, DSLConst r t) => Num (r t) where
--   (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
--   signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Int) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Float) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Double) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger



-- 2.3 For stochastic error analysis, define a new type.

-- The value is the second value in the pair to be able to reuse the
-- Functor and Applicative instances for utility functions

newtype Stochastic e t = Stochastic (e, t) deriving (Functor, Applicative)
floatStochasticError (e,v) = e
floatStochasticValue (e,v) = v

type Stochastic'  = Stochastic  Double
type Stochastic'' = Stochastic' Double

-- This can work as a primitive DSL type.
instance Typeable e => DSLType r (Stochastic e Int)    where dslType _ = TInt
instance Typeable e => DSLType r (Stochastic e Float)  where dslType _ = TFloat
instance Typeable e => DSLType r (Stochastic e Double) where dslType _ = TFloat

-- Use the same approach as for regular Int and Float: define
-- everything in therms of Num instnaces and Functor, Applicative of
-- stream

instance Num (Eval (Stochastic' Int)) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger
instance DSLPrim Eval (Stochastic' Int)  where
  op1 = eval1 ; op2 = eval2
instance DSLConst Eval (Stochastic' Int) where
  const = Eval . pure

instance Show t => Show (Stochastic' t) where
  show (Stochastic (v, f)) = show f ++ "±" ++ show v

instance ToDouble t => Num (Stochastic' t) where
  (*) = avMul
  (+) = avSum (+)
  (-) = avSum (-)
  fromInteger i = Stochastic (0, fromInteger i)
  -- These do not change the error
  abs    = fmap abs
  signum = fmap signum

sq a = a * a

-- Simplest to track variance as doubles, which then needs some type
-- conversion when going from the value type to the variance type.
-- Probably also need to implement the integral to double cast
-- explicitly.
class Num t => ToDouble t where toDouble :: t -> Double
instance ToDouble Double  where toDouble = id
instance ToDouble Float   where toDouble = float2Double
instance ToDouble Int     where toDouble = fromIntegral
 

avSum :: Num t
  => (t -> t -> t)
  -> Stochastic' t
  -> Stochastic' t
  -> Stochastic' t
avSum op (Stochastic (v1, f1)) (Stochastic (v2, f2)) = Stochastic (v,f) where
  f = f1 `op` f2
  v = sqrt (sq v1 + sq v2)

avMul :: ToDouble t
  => Stochastic' t
  -> Stochastic' t
  -> Stochastic' t

avMul (Stochastic (v1,f1)) (Stochastic (v2,f2)) = Stochastic (v,f) where
  f1' = toDouble f1
  f2' = toDouble f2
  f'  = toDouble f
  f   = f1 * f2
  v   = f' * sqrt( sq (v1/f1') + sq (v2/f2') )

-- 2.3.2 Exact error tracking.

-- Thinking for a while, exact error tracking doesn't need a
-- multi-component number.  It is much simpler to compute everything
-- using an exact number (or very high precision floating point) and
-- compare output signals between float32 and exact implementations.

newtype Exact = Exact Rational deriving (Num, Fractional)

-- Wrapped in newtype so we can re-implement Show
instance Show Exact where
  show e@(Exact r)= rv where
    rv = show d ++ " (" ++ show a ++ "/" ++ show b ++ ")"
    d = toDouble e
    a = numerator r
    b = denominator r

instance ToDouble Exact where toDouble (Exact e) = fromRational e

instance Num (Eval Exact) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Fractional (Eval Exact) where
  fromRational = const . fromRational
  (/) = div'
  
instance DSLPrim Eval Exact where
  op1 = eval1 ; op2 = eval2
instance DSLConst Eval Exact where
  const = Eval . pure

instance DSLType r Exact where dslType _ = TFloat




