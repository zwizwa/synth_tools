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
