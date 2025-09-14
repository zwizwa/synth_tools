-- Compute filter coefficients + analysis.
--
-- Some theory I would like to rehash and encode in haskell:
-- . Z domain DSL?
-- . Bilinear transform
-- . Differential realization
-- . State space (orthogonal) realization



{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

module SynthTools.Filter where

import SynthTools.IO
import SynthTools.Num
import Prelude hiding (putStrLn, putStr)


-- Start with the bilinear transform.

-- A note on z-transform: for convenience I am using the
-- "mathematicians" z-transform, which has a delay mapped to z.  It is
-- the same as the Taylor series.  It is the time-reflected (z domain
-- unit circle reflected) version of the "engineering" z-transform
-- where a delay is z^-1. If all filters are causal, all coefficients
-- will be positive.  Just the stability criterion changes: in the
-- engineering version a system is stable if all poles are inside the
-- unit circle.  In the mathematical version all poles are outside the
-- unit circle.
--
-- https://claude.ai/chat/a4567c4c-b792-4fb9-b462-0c6bcec1df0d (priv)
-- https://claude.ai/share/96c5809c-6526-4119-87d3-f0bfd8427937 (pub)
--
-- Most references quoted below will use the engineering convention.

-- https://en.wikipedia.org/wiki/Bilinear_transform

-- t is the numerical integration step size of the trapezoidal rule
bilin t z = (2 / t) * (1 - z) / (1 + z)

-- If we swap the meaning of s in the Laplace transform as well we get
-- this.  I am probably going to confuse myself mixing all these
-- conventions.

-- bilin t z = (2 / t) * (z - 1) / (z + 1)

-- w = 2pif center frequency
-- q = quality factor  (higher q = sharper peak)
-- g = gain, left out because trivial
-- a = sqrt peak_gain


-- There are 3 regimes:
-- s -> 0    gives 1
-- s -> inf  gives 1
-- s -> w    sq s' = -1 so gain is a

analog_peak w q a s = num/den where
  num = 1 + (a/q) * s' + sq s'
  den = 1 + (1/q) * s' + sq s'
  s' = s/w -- normalized, peaking at s = w, s' = 1

-- Something that puzzled me is that a = sqrt peak_gain.  Why is this?

-- https://ccrma.stanford.edu/~jos/



testZT = do
  let s = SymVar "s"
      z = SymVar "z"
      t = SymVar "T"
      w = SymVar "w"
      q = SymVar "Q"
      a = SymVar "A"

      test exp = do
        putStrLn' $ show exp
   
  putStrLn' "testZT"
  test $ bilin t z
  test $ analog_peak w q a s
  test $ analog_peak w q a $ bilin t z
  

c_SAMPLE_RATE = 48000
c_PI = 4 * (atan 1)


db2gain db = 10 ** (db / 20)

-- This needs Floating because of the use of cos, (**)
-- For exact analysis a conversion into FloatErr is necessary.
-- Coefficients are in a list to make conversion easier.

biquadEQ :: forall t. Floating t => t -> t -> t -> [t]
biquadEQ fFreq fBoost fQFactor = rv  where
  rv = f [a0, a1, a2, b0, b1, b2]
  f = fmap (* (1/a0))

  omega0 = (2 * c_PI * fFreq) / c_SAMPLE_RATE

  alpha = (sin omega0) / (2 * fQFactor)
  _A = 10 ** (fBoost / 40)
  
  a0 = 1 + alpha / _A
  a1 = -2 * cos omega0 
  a2 = 1 - alpha / _A

  b0 = 1 + alpha * _A
  b1 = -2 * cos omega0
  b2 = 1 - alpha * _A

coefs cs = (fb1,fb2,ff1,ff2,ff3) where
  [a0, a1, a2, b0, b1, b2] = cs
  fb1 = -a1 / a0
  fb2 = -a2 / a0
  ff1 =  b0 / a0
  ff2 =  b1 / a0
  ff3 =  b2 / a0

-- FIXME: This is likely wrong.
biquadUpdate (fb1,fb2,ff1,ff2,ff3) s i = (s',o) where
  (last, prev) = s
  o' = i + fb1 * last + fb2 * prev
  o = ff1 * o' + ff2 * last + ff3 * prev
  s' = (o', last)












-- Different transforms.

-- So here is the idea.  The transformations between the s and z
-- domains is just a Moebius transform.  So is the transformation
-- between z and d domains.  These are closed under function
-- composition.  Can I just compute those in an exact way?

-- If I can perform Moebius transforms on just the poles and zeros
-- then I can track those more directly and also represent the point
-- at infinity.  I can track them as exact numbers as well.

-- https://en.wikipedia.org/wiki/M%C3%B6bius_transformation

-- Start with representing the Riemann sphere.

-- Note that we want to use exact numbers, so the standard Complex
-- class doesn't work here.  Fuse the complex numbers into the Riemann
-- data type.
data Riemann t = Fin { rReal :: t, rImag :: t } | Inf
instance (Eq t, Num t, Show t) => Show (Riemann t) where
  show Inf = "∞"
  show (Fin a 0) = show a
  show (Fin 0 b) = show b
  show (Fin a b) = (show $ Fin a 0) ++ " + " ++ (show $ Fin 0 b)


-- instance Show (Moebius t) where show _ = "Moebius"
instance Fractional t => Num (Riemann t) where
  (+) = rop2 (+)
  (-) = rop2 (-)
  
  (*) (Fin a b) (Fin c d) = Fin (a*c-b*d) (a*d+b*c)
  (*) _ _= Inf
  
  fromInteger i = Fin (fromInteger i) 0

  -- Haskell Num is not great for this
  -- Maybe use numeric-prelude instead
  abs    = error "No abs for Riemann"
  signum = error "No signum for Riemann"

rop2 :: Fractional t
     => (t -> t -> t)
     -> Riemann t -> Riemann t -> Riemann t
rop2 op (Fin r1 i1) (Fin r2 i2) = Fin (op r1 r2) (op i1 i2)
rop2 _ _ _ = Inf

rop1 :: Fractional t
     => (t -> t)
     -> Riemann t -> Riemann t
rop1 op (Fin r1 i1) = Fin (op r1) (op i1)
rop1 _ _ = Inf

instance Fractional t => Fractional (Riemann t) where
  fromRational r = Fin (fromRational r) 0
  (/) (Fin a b) (Fin c d) = Fin ((a*c+b*d)/n) ((b*c-a*d)/n) where n = c*c+d*d
 


-- It seems best to represent the Moebius transforms explicitly.
--
--        az + b
-- f(z) = ------
--        cz + d
--
-- ad /= bc
--
-- It seems simpler to use a normal form
--
--          z - p
-- f(z) = k -----
--          z - q



data Moebius t = Moebius t t t
instance (Fractional t, Show t) => Show (Moebius t) where
  show = showM

showM (Moebius k p q) = show (k,p,q)



-- instance Num t => Monoid (Moebius t) where
--   mempty = Moebius 1 0 0 1

-- instance Num t => Semigroup (Moebius t) where
--   (Monoid a b c d) <> (Monoid a b c d) = Monoid a b c d where

-- This is a . b
-- See maxima/moebius.mac
compM (Moebius ka pa qa) (Moebius kb pb qb) = Moebius k p q where
  p = (kb * pb - pa * qb) / (kb - pa)
  q = (kb * pb - qa * qb) / (kb - qa)
  k = ka * (kb - pa) / (kb - qa)

bilinM = Moebius 2 (-1) 1


-- Now it is possible to represent a 2nd order rational function the
-- product of two Moebius transforms and use the Moebius composition
-- to compute each section.  If the rational function has complex
-- poles it just needs to do one.

-- Next:
-- test this, make some examples.
-- handle the infinities

-- It is probably necessary to represent the Rieman numbers explicitly
-- to be able to handle the cases where one of them is infinite.
