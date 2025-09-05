-- Can't really eliminate that monad and stay pure.  So let's try
-- Type-Safe Observable Sharing in Haskell by Andy Gill
-- https://ku-fpg.github.io/files/Gill-09-TypeSafeReification.pdf
-- https://hackage.haskell.org/package/data-reify

-- Compiler passes / intermediate forms:
-- 1. Typed haskell source
-- 2. Tree with shared nodes (Comp Stx)
-- 3. Explicit graph with backreferences (StxNode)
-- 4. Loop construction traversal (state and intermediate allocation) TODO

-- TODO:
-- Language needs data constructors.
-- Propagate type annotations
-- Simplify pragmas

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE DeriveFoldable #-}
{-# LANGUAGE DeriveTraversable #-}
{-# LANGUAGE TypeFamilies #-} -- data Arr (n :: Nat)
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE DataKinds #-} -- Arr 3
{-# LANGUAGE ScopedTypeVariables #-} -- arrLength
{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE BangPatterns #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

import SynthTools.DSL
import SynthTools.Eval
import SynthTools.Comp

import qualified SynthTools.RunC as RunC

import qualified SynthTools.ToC as ToC
import qualified SynthTools.ToV as ToV

import System.Environment


-- No longer needed
-- {-# LANGUAGE UndecidableInstances #-}  -- Rearranged implementation (repeatedly)
-- {-# LANGUAGE IncoherentInstances #-}  -- Num (r t) constraint in e.g. ramp
-- {-# LANGUAGE TypeOperators #-}
-- {-# LANGUAGE ExistentialQuantification #-}
-- {-# LANGUAGE DeriveAnyClass #-}
-- {-# LANGUAGE RankNTypes #-}


import Data.Stream hiding (fromList)
import Data.Functor
import Data.Dynamic
import qualified Data.List as List
import Control.Applicative hiding (Const)
import Control.Monad.State
import Control.Monad.Reader
import Control.Monad.Writer
import Control.Monad
import Data.Proxy
import Data.IntMap.Lazy


-- import GHC.TypeLits
import GHC.TypeNats
import Prelude hiding (take, const, zipWith, lookup)

import qualified Data.Reify as Reify
import qualified Data.Graph as Graph

import Control.Lens hiding (Const)
import Control.Lens.TH



testEval = do
  let s1 = 1 :: Eval Int
      s2 = s1 + s1
      s3 = ramp 0 :: Eval Int

      test (Eval s) = do
        putStrLn "Eval:"
        putStrLn $ show $ take 10 $ s

  test s2
  test s3


testComp = do
  -- Define some Comp terms with sharing
  let s1 = 1 :: Comp Int
      s2 = s1 + s1
      s3 = ramp 0 :: Comp Int
      s4 = ramp 1 :: Comp Int
      s5 = s3 + s4
      s6 = swap 0 1 :: Comp Int
      s7 = (array $ \i -> i + 1) :: Comp (Arr 3 Int)
      s8 = ref s7 0
      s9 = (array $ \i ->
            array $ \j ->
            i + j) :: Comp (Arr 4 (Arr 5 Int))
      s10 = (array $ \i ->
             array $ \j ->
             array $ \k ->
             i + j + k) :: Comp (Arr 4 (Arr 5 (Arr 6 Int)))
      s11 = (array $ \i ->
             array $ \j ->
             ramp 0) :: Comp (Arr 3 (Arr 4 Int))


      f12 :: Comp (Arr 3 Int) -> (Comp (Arr 3 Int))
      f12 = \input -> array $ \i -> ref input i
      s12 = f12 $ probe 1
                

      -- Towards cproc: make a function that takes an array of signals
      -- to an array of signals.  This should be representable.  EDIT:
      -- It should not be an array.  I can't find my way.  Need an
      -- example first.
  
      -- f1 = undefined
      -- 1 (Arr 4 Float) -> (Arr 

      test id s = do
        --putStrLn "Comp tree:"
        --putStrLn $ show $ unComp s

        -- Run it in the IO monad
        -- s' <- reify' s

        -- Or run it using usafePerformIO
        let s' = reify s

        
        putStrLn "\n** Node graph:"
        putStr $ show $ s'
        putStrLn "\n** ToC string:"
        let c = ToC.toC ("s" ++ show id ++ "_") s'
        putStr $ c
        putStrLn "\n** ToV string:"
        let v = ToV.toV s'
        putStr $ v
        return (id,(c,v))


  -- Compile and print them
  code <- sequence [
     test 1 s1
    ,test 2 s2
    ,test 3 s3
    ,test 4 s4
    ,test 5 s5
    -- FIXME: anonymous struct assignment.  This probably needs name
    -- generation for the composite types.
    -- ,test 6 s6  
    ,test 7 s7
    ,test 8 s8
    ,test 9 s9
    ,test 10 s10
    ,test 11 s11
    ,test 12 s12
    ]
 
  let wr (n,(c,v)) = do
        let header = "ToC.gen." ++ show n ++ ".h"
            guard = "TOC_GEN_" ++ show n ++ "_H"
            c' = concat [
              "#ifndef ",guard,"\n",
              "#define ",guard,"\n",
              "#include \"cprim.h\"\n",
              c,
              "#endif\n"]
        writeFile header c'
        return header
  headers <- traverse wr code
  let includes = concat $ flip fmap headers $ \h -> "#include \"" ++ h ++ "\"\n"
      body = "int main(int argc, char **argv) { return 0; }\n"
  
  writeFile "ToC.gen.c" $ includes ++ body


c_SAMPLE_RATE = 48000
c_PI = 4 * (atan 1)

biquadEQ :: forall t. Floating t => t -> t -> t -> ([t],[t])
biquadEQ fFreq fBoost fQFactor = rv  where
  as = [a0, a1, a2]
  bs = [b0, b1, b2]
  -- rv = (as,bs)
  rv = (f as, f bs)
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

coefs (as,bs) = (fb1,fb2,ff1,ff2,ff3) where
  [a0, a1, a2] = as
  [b0, b1, b2] = bs
  fb1 = -a1 / a0
  fb2 = -a2 / a0
  ff1 =  b0 / a0
  ff2 =  b1 / a0
  ff3 =  b2 / a0

biquadUpdate (fb1,fb2,ff1,ff2,ff3) s i = (s',o) where
  (last, prev) = s
  o' = i + fb1 * last + fb2 * prev
  o = ff1 * o' + ff2 * last + ff3 * prev
  s' = (o, last)

traverse' = flip traverse

-- The faulty behavior is that an EQ with 0 gain and high q will cause
-- problems.

testNumAn = do
  let b = 3
      f q = as ++ bs where (as,bs) = biquadEQ 1000 b q
      test q = do putStrLn $ "\nQ=" ++ (show q)
                  traverse' (f q) $ \c -> putStrLn $ show c
                  return ()

  test 0.01
  test 0.1
  test 1
  test 10
  return ()
  

testVariance = do
  let s1 = 1 + 1 :: Eval (Stochastic' Int)
      test (Eval s) = do
        putStrLn "Variance:"
        putStrLn $ show $ take 10 $ s
  test s1

  
main = do
  args <- getArgs
  putStrLn $ "synth-tools.hs: " ++ (show args)

  case args of
    [] -> do
      testEval
      testVariance
      testComp
    ["NumAn"] -> do
      testNumAn
      

      
  -- RunC.test
  




