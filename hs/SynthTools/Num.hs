-- Special Num classes that might be useful by itself.

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE TypeSynonymInstances #-}
{-# LANGUAGE RankNTypes #-}
{-# LANGUAGE NoMonomorphismRestriction #-}
{-# LANGUAGE ScopedTypeVariables #-}

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


-- errors :: Real t => (NumErr Exact t) -> (Double, Double)
-- errors (NumErr (Exact ref', approx)) = (absErr, relErr) where
--   -- With tick is exact, without is approx.  Perform all computations
--   -- using rational numbers then convert the result to Double.
--   approx' = toRational approx
--   absErr' = approx' - ref'
--   relErr' = absErr' / ref'
--   absErr  = toDouble $ Exact absErr'
--   relErr  = toDouble $ Exact relErr'

errors (NumErr (ref, approx)) = (absErr, relErr) where
  ref' = toDouble ref
  approx' = toDouble approx
  absErr = approx' - ref'
  relErr = absErr / ref'


db e = -20 * (logBase 10 $ abs e)
db' e = case e of
      0 -> "∞"
      _ -> (show $ round $ db e)

-- When displaying the numbers add the SNR and absolute noise level in dB.
instance (Show t, ToDouble t, ToDouble t') => Show (NumErr t' t) where
  show nn@(NumErr (ex, n)) = show n ++ " (" ++ e' ++ ")" where
    (absErr, relErr) = errors nn
    e' = "r=" ++ db' relErr ++ ", a=" ++ db' absErr



-- In practice we care about exact errors for Float and Double
-- implementations.  (Or maybe later difference between Double and
-- Float)

type FloatErr  = NumErr Exact Float
type DoubleErr = NumErr Exact Double

float2FloatErr :: Float -> FloatErr
float2FloatErr f = NumErr (Exact $ toRational f, f)

  
-- How to represent an exact pure sine?  The 3,4,5 triangle works but
-- that has a pretty high frequency.  Any other Pythagorian triples?
-- https://en.wikipedia.org/wiki/Pythagorean_triple
--
-- In practice this isn't really needed: 1. it is enough to represent
-- a floating point number as a rational to then generate a damped
-- sinusoid with damping factor very close to 1, or 2. just generate
-- finite precision input signal directly and convert it to
-- fractional.


-- 4. Interpret to formula.  This is useful for working with z
-- transforms.

data Sym = SymInt Int
         | SymRat Rational
         | SymAdd Sym Sym
         | SymSub Sym Sym
         | SymAbs Sym
         | SymSignum Sym
         | SymMul Sym Sym
         | SymDiv Sym Sym
         | SymVar String
         deriving (Eq)

probeOp op args = c $ ["(",op, c $ c $ fmap a args,")"] where
  c = concat
  a x = [" ",show x]

instance Show Sym where

  show (SymVar v)    = v
  show (SymInt i)    = show i
  show (SymRat r)    = show r
  show (SymAdd a b)  = probeOp "+" [a,b]
  show (SymSub a b)  = probeOp "-" [a,b]
  show (SymMul a b)  = probeOp "*" [a,b]
  show (SymDiv a b)  = probeOp "/" [a,b]
  show (SymAbs a)    = probeOp "abs" [a]
  show (SymSignum a) = probeOp "signum" [a]
                 
instance Num Sym where
  (+) = SymAdd
  (-) = SymSub
  (*) = SymMul
  abs = SymAbs
  signum = SymSignum
  fromInteger = SymInt . fromInteger

instance Fractional Sym where
  fromRational = SymRat . fromRational
  (/) = SymDiv


-- 5. Dual numbers for automatic differentiation.  
-- https://www.cs.cornell.edu/~bindel/nmds/01-Fund1d/04-AutoDiff.html

data Dual t = Dual t t

instance Show t => Show (Dual t) where
  -- show (Dual f f') = show f ++ "+" ++ show f' ++ "ε"
  show (Dual f f') = show f ++ " +ε " ++ show f'

_NOT_DIFF tag = error $ tag ++ " is not differentiable"
                     
instance Num t => Num (Dual t) where
  fromInteger i = Dual (fromInteger i) 0
  (Dual f f') + (Dual g g') = Dual (f+g) (f'+g')
  (Dual f f') - (Dual g g') = Dual (f-g) (f'-g')
  (Dual f f') * (Dual g g') = Dual (f*g) (f*g'+f'*g)
  -- These are not differentiable
  abs (Dual f f')    = _NOT_DIFF "abs"
  signum (Dual f f') = _NOT_DIFF "signum"

instance Fractional t => Fractional (Dual t) where
  fromRational r = Dual (fromRational r) 0
  (Dual f f') / (Dual g g') = Dual (f/g) (f'/g - f*g'/(g*g))



-- 6. Finite Fields, initially added here to have an exact number
--    system for testing FFT partitioned convolution algorithms.
--
--    I'm interested in simulating power-of-two DFTs to allow for FFT
--    testing as well, so this needs a field of order 2^n+1 and to use
--    its order 2^n root of unity to define the DFT. This is covered
--    by Fermat primes F_n=2^(2^n)+1 where n=1,2,3,4 for orders
--    3,5,17,257 and 65537.
--
--    See SynthTools.NNT for generic DFT/FFT code
--

-- Non-negative modulo.  Note that argument order is flipped.
nnMod :: Int -> Int -> Int
nnMod n a = if b>=0 then b else b + n where  b = a `mod` n

-- For Num members that have no reasonable definition.
_NI tag = error $ tag ++ " is not defined"


-- 6.1 F_2
--
-- Useful for illustrations of basic principles.

data F2 = F2 Int deriving (Eq)
instance Show F2 where show (F2 n) = show n

modF2 = nnMod 17
opF2 op (F2 a) (F2 b) = F2 $ modF2 $ op a b

instance Num F2 where
  fromInteger = F2 . modF2 . fromInteger
  (+) = opF2 (+)
  (-) = opF2 (-)
  (*) = opF2 (*)
  abs    (F2 _) = _NI "F3.abs"
  signum (F2 _) = _NI "F3.signum"


-- 6.2 F_3
--
-- Useful for illustrations of a realistic 256 tap DFT size.

data F3 = F3 Int deriving (Eq)
instance Show F3 where show (F3 n) = show n

modF3 = nnMod 257
opF3 op (F3 a) (F3 b) = F3 $ modF3 $ op a b

instance Num F3 where
  fromInteger = F3 . modF3 . fromInteger
  
  (+) = opF3 (+)
  (-) = opF3 (-)
  (*) = opF3 (*)
  -- These have no reasonable definition.
  abs    (F3 _) = _NI "F3.abs"
  signum (F3 _) = _NI "F3.signum"


-- 6.3 F_4
--
-- This one is getting too computationally intensive with such an
-- inefficient implementation.  It is possible to use it for smaller
-- DFTs though: any subcycle of 65536 should work.

data F4 = F4 Int deriving (Eq)
instance Show F4 where show (F4 n) = show n

modF4 = nnMod 65537
opF4 op (F4 a) (F4 b) = F4 $ modF4 $ op a b

instance Num F4 where
  fromInteger = F4 . modF4 . fromInteger
  (+) = opF4 (+)
  (-) = opF4 (-)
  (*) = opF4 (*)
  -- These have no reasonable definition.
  abs    (F4 _) = _NI "F4.abs"
  signum (F4 _) = _NI "F4.signum"




-- Another number type I would like is something that can compute the
-- number of operations that gets executed.  This probably involves
-- letting the interpretation create a data structure and passing it
-- to a stateful evaluator.


