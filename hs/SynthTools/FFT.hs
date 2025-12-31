{-# LANGUAGE NoMonomorphismRestriction #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE FlexibleInstances #-}
{-# LANGUAGE MultiParamTypeClasses #-}

-- Numer Theoretic Transform: DFT/FFT-like algorithms over Galois
-- fields.  The roots of unity have been kept generic so these can
-- probably be generalized to any field/ring with roots of unity.



module SynthTools.FFT where

import SynthTools.Num
import Test.QuickCheck
import Prelude hiding (exp)
import Data.Complex

import Debug.Trace

-- Instances
--
-- Note that 3 works as a generator for the roots of unity for all
-- fields F2,F3,F4.  This is chosen as the "most natural" analogy to
-- e^{i 2pi / N}, because it yields the "sinusoid" 1,3,9,27,... that
-- looks "most natural" because 3 is small.

-- For eacht field pick a default root of unity generator to base the
-- DFT on.
--
-- Is it better to make this a property of vectors of numbers?  That
-- way different roots can be used for different vector sizes over the
-- same field.  EDIT: It seems simpler to just wrap the scalar if a
-- different root / cycle length is needed for a particular field.  (
-- It doesn't seem too important, a can of worms, and I don't have the
-- energy atm. )

class (Eq n, Num n, Show n) => UnitRoot n where
  unitRoot :: Int -> (n, n, n)


badroot tag n = error (tag ++ " bad root: " ++ show n)

-- Support multiple roots of unity for each field, depending on
-- requested order.  For complex numbers the order just needs to be an
-- integer.  For the finite fields the order has to divide the
-- multiplicative group order.
instance UnitRoot F2 where unitRoot    16 = ffRoots 3 ; unitRoot n = badroot "F2" n
instance UnitRoot F3 where unitRoot   256 = ffRoots 3 ; unitRoot n = badroot "F3" n
instance UnitRoot F4 where unitRoot 65536 = ffRoots 3 ; unitRoot n = badroot "F4" n

-- For finite fields the inverse root can just be found from the exact
-- cycle.  The IFFT scaling factor is N^-1 which is -1.
ffRoots :: forall n. UnitRoot n => n -> (n,n,n)
ffRoots root = (root, cycle !! (n-1), -1) where
  cycle = genCycle root
  n = length cycle

-- For 

instance (Eq n, Show n, RealFloat n) => UnitRoot (Complex n) where
  unitRoot n = (ej (-w), ej w, inv_n) where
    inv_n = 1 / (fromIntegral n)
    w = 2 * pi * inv_n
    ej = mkPolar 1




-- Generic code.
genCycle :: (Eq n, Num n) => n -> [n]
genCycle gen = (1 : cycle gen) where
  cycle a = if b == 1 then [a] else (a : cycle b) where b = a * gen


iprod [] [] = 0
iprod (a:as) (b:bs) = a*b + (iprod as bs)

exp :: Num a => a -> [a]
exp a = (1 : exp a) where
  exp x = (x : exp (a * x))

dft' :: forall n. UnitRoot n => Int -> [n] -> [n]
dft' dir input = output where
  order = length input
  (rootGen, invRootGen, _) = unitRoot order
  roots = genCycle $ rootGen
  vec i = take order $ exp $ (roots !! (nnMod order (dir*i)))
  bin i = iprod input $ vec i 
  output = map bin [0..(order-1)] where


pad :: forall n. Num n => Int -> [n] -> [n]
pad order sig = sig' where
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

dft :: UnitRoot n => [n] -> [n]
dft = dft' (-1)

idft :: UnitRoot n => [n] -> [n]
idft = (map (*(-1))) . (dft' 1)


-- Use the tested DFT implementation to test the FFT
-- This is what the "butterflies" do.
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
  
  -- Recursion on half size produced by squared genrator.
  fft' = fftRec (q * q, n `div ` 2)
  (sig0, sig1) = uninterleave sig

  -- Split signal in even and odd and compute ffts.
  fft0 = fft' $ sig0
  fft1 = fft' $ sig1
  
  -- Combine the results.
  fft01 = fftCombine qn fft0 fft1

-- See also DSP.basic
uninterleave (a:b:abs) = (a:as, b:bs) where (as, bs) = uninterleave abs
uninterleave _ = ([], [])

fft :: forall n. (Show n, UnitRoot n) => [n] -> [n]
fft sig = fftRec (q',n) $ sig where
  -- Use the inverse root just like the DFT.  This is arbitrary but
  -- feels right to keep the analogy going (to build some FF FFT intuition).
  n = length sig
  (q, q', _) = unitRoot n

ifft :: forall n. (Show n, UnitRoot n) => [n] -> [n]
ifft sig = map (* inv_n) $ fftRec (q,n) $ sig where
  n = length sig
  (q, q', inv_n) = unitRoot n


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
pointv op = f where
  f (a:as) (b:bs) = (a `op` b : f as bs)
  f as [] = as
  f [] bs = bs

sumv  = pointv (+)
prodv = pointv (*)




-- Regular convolution, reference point.
conv :: forall n. Num n => [n]->[n]->[n]
conv a b = ab where
  n = length a
  prod i = pad i [] ++ map (*(a!!i)) b
  ab = foldr sumv [] (map prod [0..n-1])

-- Circular convolution
convc :: forall n. UnitRoot n => [n]->[n]->[n]
convc a b = ifft $ prodv (fft a) (fft b)

-- Partitioned convolution
convp a b = undefined




newtype V16 n = V16 [n] deriving Show
newtype V256 n = V256 [n] deriving Show

class UnitRoot n => VN v n where unVN :: v n -> [n]
instance UnitRoot n => VN V16  n where unVN (V16  ns) = ns
instance UnitRoot n => VN V256 n where unVN (V256 ns) = ns


instance UnitRoot n => Arbitrary (V16  n) where arbitrary = fmap V16  $ arbitraryV 16
instance UnitRoot n => Arbitrary (V256 n) where arbitrary = fmap V256 $ arbitraryV 256

arbitraryV :: forall n. UnitRoot n => Int -> Gen [n]
arbitraryV order = traverse (\_ -> arb_n) [1..order] where
  (gen, gen', _) = unitRoot order
  arb_n = fmap fromInteger arbitrary

-- Function is an identify function.
isID :: forall v n. VN v n => ([n]->[n]) -> v n -> Bool
isID id x' = x == id x where x = unVN x'

-- Functions are equal
isEQ :: forall v n. VN v n => ([n]->[n]) -> ([n]->[n]) -> v n -> Bool
isEQ f g x' = f x == g x where x = unVN x'


-- Same, but for approximate identity.
isID' :: forall v n. (ApproxEq n, VN v n) => ([n]->[n]) -> v n -> Bool
isID' id x' = x `approxListEq` (id x) where x = unVN x'

isEQ' :: forall v n. (ApproxEq n, VN v n) => ([n]->[n]) -> ([n]->[n]) -> v n -> Bool
isEQ' f g x' = f x `approxListEq` g x where x = unVN x'

class ApproxEq n where
  approxListEq :: [n] -> [n] -> Bool

-- Make it work for the exact numbers as well.
instance ApproxEq F2 where approxListEq = (==)
instance ApproxEq F3 where approxListEq = (==)
instance ApproxEq F4 where approxListEq = (==)
  
-- Test vectors can be exact zero so avoid the divide by zero if error
-- is exactly 0, and use a reasonable relative error of 10 decimal
-- places.
instance (Show n, RealFloat n) => ApproxEq (Complex n) where
  approxListEq as bs = isEq where
    -- isEq' trace msg isEq
    msg = "approxListEq: " ++ show (error,relError)
    isEq = if abs error == 0 then True
           else relError < 10 ^^ (-10)
    relError = error / norm
    norm  = sum $ map magnitude as
    error = sum $ zipWith error' as bs
    error' a b = magnitude $ a - b
    sum = foldr (+) 0



quickCheckFFT = do
  let check  = quickCheck
      check' = verboseCheck

  
      -- Time-reflected vector
      reflect :: forall n. UnitRoot n => [n] -> [n]
      reflect v = map v' [0..n-1] where
        n = length v
        v' i = v !! (nnMod n (-i))

      scale n = map (* n)

      -- Separate DFT tests since these are quadriatic and too time
      -- consuming for F3 F4.
      propsDFT :: forall v n. VN v n => [v n -> Bool]
      propsDFT = [
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
      -- Separate FFT tests to run on F3 as well
      propsFFT :: forall v n. VN v n => [v n -> Bool]
      propsFFT = [
         isID (fft . ifft)
        ,isID (ifft . fft)
        -- This depends on scaling which happens to cancel out to 1 in the FF case.
        ,isID (fft . fft . fft . fft) 
        ]

      -- Inexact comparison for Complex
      propsFFT' :: forall v n. (ApproxEq n, VN v n) => [v n -> Bool]
      propsFFT' = [
         isID' (fft . ifft)
        ,isID' (ifft . fft)
        --,isID' (fft . fft . fft . fft)
        ]


      
  traverse check (propsDFT :: [V16  F2 -> Bool])
  traverse check (propsFFT :: [V256 F3 -> Bool])
  traverse check (propsFFT' :: [V16 (Complex Double) -> Bool])
  traverse check (propsFFT' :: [V256 (Complex Double) -> Bool])


  -- Too compute intensive
  -- traverse check (props :: [VecDFT F3 -> Bool])
  -- traverse check (props :: [VecDFT F4 -> Bool])

  -- It might actually be good to try a setup where N != -1 etc,
  -- e.g. N smaller than F_4 order.

  return ()

-- Divide with negative remainder (smallest multiple that fits).
fits x y = q_nr where
  q_nr =  if r>0 then (q+1,r-y) else (q,0)
  q = x `div` y
  r = x `mod` y



-- With fixed block size, what partitioning is optimal?
--
-- This is not immediately obvious to me so here's a formula to
-- explore the tradeoffs.
--
-- For the 1500 length and 256 block size, the 2 section 1024 point
-- FFT is more efficient than the 1 section 2048 point FFT, because
-- the FFT cost rises dramatically (slightly over double) while the
-- multiplication cost stays the same (1->2 sections, but size
-- halves).  It seems in general the multiplication cost is fairly low
-- wrt the FFT cost.
--
-- The gut feeling I get here is that just doubling the block size
-- makes most sense.




-- n: FFT size 
complexity logn = (sections, cost_fft, cost_mul, cost_dir) where
  -- b: Output stride / block size, is fixed for current application.
  b = 256
  -- l: Filter length is also fixed
  l = 1500
  -- n: FFT size
  n = 2 ^ logn

  -- Cost tradeoffs
  c_fft = 1
  c_mul = 1
  c_dir = 1

  (sections,_) = fits l (n-b+1)

  cost_fft = c_fft * n * logn
  cost_mul = c_mul * sections * n
  cost_dir = c_dir * b * l
