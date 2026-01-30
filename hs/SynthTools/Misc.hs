{-# LANGUAGE ScopedTypeVariables #-}

-- Misc functions

module SynthTools.Misc where

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
floatEQ = floatEQ' 0.00001 0.0000001

floatEQ' :: Float -> Float -> [Float] -> [Float] -> Bool
floatEQ' relError no_nan_offset a b = (e / (norm a)) < 0.00001 where
  n = fromIntegral $ length a
  sum = foldl (+) 0
  f x y = (x - y) ^ 2
  e = (sqrt $ sum $ zipWith f a b) / n
  -- add a small offset to norm to avoid NaN when norm is 0.0f
  norm x = no_nan_offset + (sqrt $ sum $ zipWith (*) x x) / n

