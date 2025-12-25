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



-- 6. Finite Fields for testing FFT-based algorithms.  I'm interested
--    in power-of-two FFTs, so this needs a field of order 2^n+1 to
--    have an order 2^n root of unity.  This is covered by Fermat
--    primes F_n=2^(2^n)+1 where n=1,2,3,4 for orders 3,5,17,257 and
--    65537.  I am only going to use FFT size around 256 in practice
--    so use F3, where 3 is a root of unity.

data F3 = F3 Int deriving (Eq)
instance Show F3 where show (F3 n) = show n

modF3 :: Int -> Int
modF3 a = if b>=0 then b else b + 257 where b = a `mod` 257

opF3 op (F3 a) (F3 b) = F3 $ modF3 $ op a b

addF3 = opF3 (+)
subF3 = opF3 (-)
mulF3 = opF3 (*)

_NOT_F3 tag = error $ tag ++ " is not defined for F3"

instance Num F3 where
  fromInteger = F3 . modF3 . fromInteger
  (+) = opF3 (+)
  (-) = opF3 (-)
  (*) = opF3 (*)
  -- These have no reasonable definition.
  abs    (F3 _) = _NOT_F3 "abs"
  signum (F3 _) = _NOT_F3 "signum"

cycleF3 gen = (1 : cycle gen) where
  cycle a = if b == 1 then [a] else (a : cycle b) where b = a * gen

vprod [] [] = 0
vprod (a:as) (b:bs) = a*b + (vprod as bs)

expF3 :: Num a => a -> [a]
expF3 a = (1 : exp a) where
  exp x = (x : exp (a * x))

dftF3 input = dftF3' (length input, input)
dftF3' (256, input) = map bin [0..255] where
  vec i = take 256 $ expF3 $ roots !! i
  bin i = vprod input $ vec i
  roots = cycleF3 $ F3 3
