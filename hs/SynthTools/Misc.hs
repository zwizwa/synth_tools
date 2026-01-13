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
shiftedImpulse n one i = (zeros i) ++ [one] ++ (zeros (n-i-1))


-- List reference with zero padding.
paddedRef list = c where
  n = length list
  c i | (i>=0) && (i<n) = list !! i
      | otherwise = 0
  
