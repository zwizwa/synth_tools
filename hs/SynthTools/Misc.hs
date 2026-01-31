{-# LANGUAGE ScopedTypeVariables #-}

-- Misc functions

module SynthTools.Misc where

import Debug.Trace
import Text.Printf

-- Run length encoding with explicit 0 count at the beginning
-- and end. E.g.
-- [1,0,...] is [(0,0),(1,1),(n,0)]
-- [...,0,1] is [(n,0),(1,1),(0,0)]
rle = rle' 0 0 where
  rle' n 0 [] = [(n,0)]
  rle' n s [] = [(n,s),(0,0)]
  rle' n s (a:as) = if s==a then same else diff where
    same = rle' (n+1) s as
    diff = ((n,s) : rle' 1 a as)

        

zeros i = replicate i 0
shiftedImpulse n one i = pad n $ (zeros i ++ [one])


-- List reference with zero padding.
paddedRef list = c where
  n = length list
  c i | (i>=0) && (i<n) = list !! i
      | otherwise = 0
  
-- Truncate or zero-extend to requested length.
pad :: forall n. Num n => Int -> [n] -> [n]
pad len sig = sig' where
  sig' = take len (sig ++ (cycle [0]))


-- An ad-hoc inexact Float equality test.

-- This isn't useful when the range of errors varies wildly due to
-- data size.  See approxEQ comment.
floatEQ = floatEQ' 0.00001 0.00001

floatEQ' :: Float -> Float -> [Float] -> [Float] -> Bool
floatEQ' relError no_nan_offset a b = trace' log $ (e / na) < relError where

  log = "e=" ++ show e ++ ", na=" ++ show na
  -- log = show (take 10 a, take 10 b)

  na = norm a
  
  sum = foldl (+) 0
  f x y = (x - y) ^ 2
  e = (sqrt $ sum $ zipWith f a b)
  -- add a small offset to norm to avoid NaN when norm is 0.0f
  norm x = no_nan_offset + (sqrt $ sum $ zipWith (*) x x)

  -- For quick disable of tracing.
  trace' _ v = v



-- I used this to test equality of two FIR implementations in a
-- QuickCheck test.  What worked in the end was to use existing
-- meaningful FIR coefficients together with uniform noise input.
-- That gives a reasonable output signal where an absolute error is
-- able to be used for comparison.
--
-- Before I used too much parameterization making the input and FIR
-- sizes too variable where it was really hard to qualify what a good
-- error range would be for approximate equality.

approxEQ :: Float -> [Float] -> [Float] -> Bool
approxEQ max_error as bs = log rv where
  -- Piece-wise absolute error bound check
  rv = and rvs
  rvs = zipWith withinBounds as bs
  withinBounds a b = error a b <= max_error
  error a b = abs $ a - b

  -- Log in case of bound error
  log rv = if rv then rv else trace log' rv
  log' = concat $ map show' $ zip [(0::Int)..] $ zipWith logab as bs
  logab a b = (withinBounds a b, error a b, a, b)
  show' (n,(rv,e,a,b)) =
    printf "%3d %+3.7f %+3.7f %+3.7f %s\n" n a b e $ show rv

  -- For quick disable of tracing.
  trace' _ v = v
