-- Special Num classes that might be useful by itself.

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE TypeSynonymInstances #-}
{-# LANGUAGE RankNTypes #-}

module SynthTools.Num where

import Data.Ratio
import GHC.Float

-- 0. Tools

class Num t => ToDouble t where toDouble :: t -> Double
instance ToDouble Double  where toDouble = id
instance ToDouble Float   where toDouble = float2Double
instance ToDouble Int     where toDouble = fromIntegral



-- 1. Stochastic variables

-- Stochastic variables are modeled as a mean value and a variance.
--
-- The maen value is the second value in the pair to be able to reuse
-- the Functor and Applicative instances for utility functions

newtype Stochastic e t = Stochastic (e, t) deriving (Functor, Applicative)
floatStochasticError (e,v) = e
floatStochasticValue (e,v) = v

-- It seems simplest to track variance as Double, which then needs
-- some type conversion when going from the value type to the variance
-- type.  Probably also need to implement the integral to double cast
-- explicitly.
type Stochastic'  = Stochastic  Double

instance ToDouble t => Num (Stochastic' t) where
  (*) = avMul
  (+) = avSum (+)
  (-) = avSum (-)
  fromInteger i = Stochastic (0, fromInteger i)
  -- These do not change the error
  abs    = fmap abs
  signum = fmap signum

sq a = a * a

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



-- 2. Exact rational numbers
newtype Exact = Exact Rational deriving (Num, Fractional)

-- Wrapped in newtype so we can re-implement Show
instance Show Exact where
  show e@(Exact r)= rv where
    rv = show d ++ " (" ++ show a ++ "/" ++ show b ++ ")"
    d = toDouble e
    a = numerator r
    b = denominator r

instance ToDouble Exact where toDouble (Exact e) = fromRational e




-- 3. Two Num implementations in tandem (e.g. Exact, Float)

newtype NumErr a b = NumErr (a, b)
instance (Num a, Num b) => Num (NumErr a b) where
  (+) = nn2op (+)
  (-) = nn2op (-)
  (*) = nn2op (*)
  abs = nn1op abs
  signum = nn1op signum
  fromInteger i = NumErr (fromInteger i, fromInteger i)

instance (Fractional a, Fractional b) => Fractional (NumErr a b) where
  fromRational r = NumErr (fromRational r, fromRational r)
  (/) = nn2op' (/)
  

nn2op :: forall a b. (Num a, Num b)
  => (forall n. Num n => n -> n -> n)
  -> NumErr a b -> NumErr a b -> NumErr a b
nn2op op (NumErr (a1,b1)) (NumErr (a2,b2)) = NumErr (op a1 a2, op b1 b2)

nn2op' :: forall a b. (Fractional a, Fractional b)
  => (forall n. Fractional n => n -> n -> n)
  -> NumErr a b -> NumErr a b -> NumErr a b
nn2op' op (NumErr (a1,b1)) (NumErr (a2,b2)) = NumErr (op a1 a2, op b1 b2)

  
nn1op :: forall a b. (Num a, Num b)
  => (forall n. Num n => n -> n)
  -> NumErr a b -> NumErr a b
nn1op op (NumErr (a1,b1)) = NumErr (op a1, op b1)


errors (NumErr (ref, approx)) = (absErr, relErr) where
  ref' = toDouble ref
  approx' = toDouble approx
  absErr = approx' - ref'
  relErr = absErr / ref'


-- When displaying the numbers add the SNR in dB.
instance (Show t, ToDouble t, ToDouble t') => Show (NumErr t' t) where
  show nn@(NumErr (ex, n)) = show n ++ " (" ++ snr' ++ ")" where
    (absErr, relErr) = errors nn
    snr' = case absErr of
      0 -> "exact"
      _ -> (show $ round $ snr relErr) ++ "dB"

snr e = -20 * (logBase 10 $ abs e)


-- In practice we care about exact errors for Float and Double
-- implementations.  (Or maybe later difference between Double and
-- Float)

type FloatErr  = NumErr Exact Float
type DoubleErr = NumErr Exact Double

  
