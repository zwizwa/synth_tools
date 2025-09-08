{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE NoMonomorphismRestriction #-}


-- Compute filter coefficients + analysis.
module SynthTools.Filter where

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
