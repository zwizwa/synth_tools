{-# LANGUAGE NoMonomorphismRestriction #-}
{-# LANGUAGE ScopedTypeVariables #-}


module SynthTools.DFT where

import SynthTools.Num
import Test.QuickCheck
import Prelude hiding (exp)

import Debug.Trace

-- Generic code.
genCycle :: FFRoot n => n -> [n]
genCycle gen = (1 : cycle gen) where
  cycle a = if b == 1 then [a] else (a : cycle b) where b = a * gen

iprod [] [] = 0
iprod (a:as) (b:bs) = a*b + (iprod as bs)

exp :: Num a => a -> [a]
exp a = (1 : exp a) where
  exp x = (x : exp (a * x))

dft' :: forall n. FFRoot n => Int -> [n] -> [n]
dft' dir input = output where
  output = map bin [0..(order-1)] where
  (rootGen, order) = ffRoot :: (n,Int)
  roots = genCycle $ rootGen
  vec i = take order $ exp $ (roots !! (nnMod order (dir*i)))
  bin i = iprod input' $ vec i
  input' = pad input

pad :: forall n. FFRoot n => [n] -> [n]
pad sig = padn order sig where
  (rootGen, order) = ffRoot :: (n, Int)

padn :: forall n. Num n => Int -> [n] -> [n]
padn order sig = sig' where
  sig' = take order (sig ++ (cycle [0]))


-- Note that the ( idft . dft ) composition defined by just inverting
-- the rootGen (traversing the roots of unity cycle backwards) will
-- accrue a scaling factor N equal to the size of the DFT, which
-- happens to be equal to -1 in the finite field of order N+1.  We
-- just add that here in the idft definition.

-- The choice of the root generator doesn't really matter much since
-- the spectrum doesn't have a straightforward intuitive meaning of
-- spectrum in the context of sinusoidal base functions.  ( Here my
-- intuition is just "orthogonal smear". )

dft :: FFRoot n => [n] -> [n]
dft = dft' (1)

idft :: FFRoot n => [n] -> [n]
idft = (map (*(-1))) . (dft' (-1))


-- Use the tested DFT implementation to test the FFT
fftCombine qn@(q,n) fft0 fft1 = sumv fft0' fft1' where
  -- The first FFT can just be periodically extended.  The periodic
  -- extension implements the oversampling (interleave zeros) when
  -- relating the smaller and larger FFTs.
  fft0' = fft0 ++ fft0
  -- In addition the second FFT needs modulation of the spectrum
  -- applied to implement the 1 sample shift.
  fft1' = modulate qn $ (fft1 ++ fft1)

fftRec :: forall n. (Show n, Num n) => (n, Int) -> [n] -> [n]
fftRec qn@(_,1) [x] = [x]
fftRec qn@(q,n) sig = fft01 where
  
  -- Recursion on half size
  n' = n `div` 2
  q' = q * q
  fft' = fftRec (q',n')
  split o = map (\i -> sig !! (i*2+o)) [0..n'-1]
  -- Split signal in even and odd and compute ffts.
  fft0 = fft' $ split 0
  fft1 = fft' $ split 1
  -- Combine the results.
  fft01 = fftCombine qn fft0 fft1

fft :: (Show n, FFRoot n) => [n] -> [n]
fft sig = fftRec ffRoot $ pad sig
  

-- The "fold with output" state,in->state,out stateful signal operator.
siso u = f where
  f _ [] = []
  f s (i:is) = o:os where
    (s',o) = u s i
    os = f s' is

modulate (q,n) is = os where
  os = take n $ siso u 1 is
  u s i = (s*q, i*s)
 

-- Vector summation with automatic padding.
sumv (a:as) (b:bs) = (a+b : sumv as bs)
sumv as [] = as
sumv [] bs = bs


-- Regular convolution, reference point.
conv :: forall n. Num n => [n]->[n]->[n]
conv a b = ab where
  n = length a
  prod i = pad i ++ map (*(a!!i)) b
  pad i = take i $ cycle [0]
  ab = foldr sumv [] (map prod [0..n-1])

-- Circular convolution
convc :: forall n. Num n => [n]->[n]->[n]
convc a b = ab where
  ab = []







newtype VecDFT n = VecDFT [n] deriving Show

instance FFRoot n => Arbitrary (VecDFT n) where
  arbitrary = fmap VecDFT $ traverse (\_ -> arbitraryFF) [1..order] where
    (gen, order) = ffRoot :: (n, Int)
    arbitraryFF = fmap fromInteger arbitrary


quickCheckFF = do
  let check  = quickCheck
      check' = verboseCheck

      -- Function is an identify function.
      isID :: forall n. FFRoot n => ([n]->[n]) -> VecDFT n -> Bool
      isID id (VecDFT x) = x == id x

      -- Functions are equal
      isEQ :: forall n. FFRoot n => ([n]->[n]) -> ([n]->[n]) -> VecDFT n -> Bool
      isEQ f g (VecDFT x) = f x == g x
  
      -- Time-reflected vector
      reflect :: forall n. FFRoot n => [n] -> [n]
      reflect v = map v' [0..n-1] where
        n = length v
        v' i = v !! (nnMod n (-i))

      scale n = map (* n)
      
      props :: forall n. FFRoot n => [VecDFT n -> Bool]
      props = [
        -- Inverses
         isID (idft . dft)
        ,isID (dft . idft)
        -- Applied twice scales by N == -1
        ,isID ((scale (-1)) . reflect . dft . dft)
        -- Applied 4x scales by N^2 = 1
        ,isID (dft . dft . dft . dft)
        ,isID (idft . idft . idft . idft)
        -- FFT and DFT are the same
        ,isEQ fft dft
        ]

      
  traverse check (props :: [VecDFT F2 -> Bool])


  -- Too compute intensive
  -- traverse check (props :: [VecDFT F3 -> Bool])
  -- traverse check (props :: [VecDFT F4 -> Bool])

  -- It might actually be good to try a setup where N != -1 etc,
  -- e.g. N smaller than F_4 order.

  return ()
