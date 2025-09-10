-- Evaluator instance for DSL class

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE TypeSynonymInstances #-}
{-# LANGUAGE RankNTypes #-}

module SynthTools.Eval where

import SynthTools.DSL
import SynthTools.Lib
import SynthTools.Num
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


-- 2.2 ... and define instances Num for each supported primitive.

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

-- Stochastic can work as a primitive DSL type.
instance Typeable e => DSLType r (Stochastic e Int)    where dslType _ = TInt
instance Typeable e => DSLType r (Stochastic e Float)  where dslType _ = TFloat
instance Typeable e => DSLType r (Stochastic e Double) where dslType _ = TDouble


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

-- Define Show instance to produce mean ± variance display.
instance Show t => Show (Stochastic' t) where
  show (Stochastic (v, f)) = show f ++ "±" ++ show v

-- Then define the Num instance for the algebraic rules that track
-- variance along side the mean value.

-- See Num.hs


-- 2.4 Exact error tracking.

-- Instead of using a multi-component number that tracks the error, it
-- seems best to define error as the difference between a Float
-- implementation and an Exact number, which can be implemented in
-- terms of Rational = Ratio Integer.  Note that recursive filters
-- will cause the state variables to keep growing.

-- See SynthTools.Num for Num classes


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

-- 2.5 Track Exact plus any number type together


instance Num (Eval FloatErr) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Fractional (Eval FloatErr) where
  fromRational = const . fromRational
  (/) = div'

instance DSLPrim Eval FloatErr where
  op1 = eval1 ; op2 = eval2
instance DSLConst Eval FloatErr where
  const = Eval . pure

instance DSLType r FloatErr where dslType _ = TFloat


instance Num (Eval DoubleErr) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Fractional (Eval DoubleErr) where
  fromRational = const . fromRational
  (/) = div'

instance DSLPrim Eval DoubleErr where
  op1 = eval1 ; op2 = eval2
instance DSLConst Eval DoubleErr where
  const = Eval . pure

instance DSLType r DoubleErr where dslType _ = TDouble



-- 2.6 Symbolic evaluation

-- Note that this is more similar to Comp.hs in spirit, but we do not
-- do variable sharing here so it makes more sense to treat it just as
-- interpretation, like is done in the Neon emulator.  The
-- implementation is in Num.hs

instance Num (Eval Symbolic) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Fractional (Eval Symbolic) where
  fromRational = const . fromRational
  (/) = div'
  
instance DSLPrim Eval Symbolic where
  op1 = eval1 ; op2 = eval2
instance DSLConst Eval Symbolic where
  const = Eval . pure

instance DSLType r Symbolic where dslType _ = TFloat

