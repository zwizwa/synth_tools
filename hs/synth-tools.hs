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
import SynthTools.Lib
import SynthTools.Comp
import SynthTools.Eval
import SynthTools.Num
import SynthTools.Filter
import SynthTools.NTT

import qualified SynthTools.RunC as RunC

import qualified SynthTools.ToC as ToC
import qualified SynthTools.ToV as ToV

import System.Environment

import Debug.Trace

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
import Data.Complex


import Prelude hiding (take, const, zipWith, lookup, putStr, putStrLn)
import qualified Prelude as Prelude

import qualified Data.Reify as Reify
import qualified Data.Graph as Graph

import Control.Lens hiding (Const)
import Control.Lens.TH

import GHC.TypeNats
import GHC.Float

import SynthTools.IO

testEval = do
  let s1 = 1 :: Eval Int
      s2 = s1 + s1
      s3 = ramp 0 :: Eval Int

      test (Eval s) = do
        putStrLn' "Eval:"
        putStrLn' $ show $ take 10 $ s

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
        --putStrLn' "Comp tree:"
        --putStrLn' $ show $ unComp s

        -- Run it in the IO monad
        -- s' <- reify' s

        -- Or run it using usafePerformIO
        let s' = reify s

        
        putStrLn' "\n** Node graph:"
        putStr' $ show $ s'
        putStrLn' "\n** ToC string:"
        let c = ToC.toC ("s" ++ show id ++ "_") s'
        putStr' $ c
        putStrLn' "\n** ToV string:"
        let v = ToV.toV s'
        putStr' $ v
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



traverse' = flip traverse

-- The faulty behavior is that an EQ with 0 gain and high q will cause
-- problems.

testBQCoefs = do
  let b = 3
      f q = biquadEQ 1000 b q
      test q = do
        putStrLn' $ "\nQ=" ++ (show q)
        putStrLn' $ "\nboost=" ++ (show $ db2gain b)
        
        traverse' (f q) $ \c -> putStrLn' $ show c
        return ()

  test 0.0000001
  test 0.01
  test 0.1
  test 1
  test 10
  return ()
  

testVariance = do
  let s1 = 1 + 1 :: Eval (Stochastic' Int)
      test (Eval s) = do
        putStrLn' "Variance:"
        putStrLn' $ show $ take 10 $ s
  test s1

testExact = do
  let s1 = exponential $ const $ Exact 2/3
      test (Eval s) = do
        putStrLn' "Exact:"
        traverse (putStrLn' . show) $ take 10 s
        return ()
        
  test s1

testErr tag c = do
  let s1 = exponential $ const c
      test (Eval s) = do
        putStrLn' tag
        traverse (putStrLn' . show) $ take 40 s
        return ()
  test s1

testSNR = do
  testErr "DoubleErr:" (2/3 :: DoubleErr)
  testErr "FloatErr:"  (2/3 :: FloatErr)



runSys :: (s -> i -> (s, o)) -> s -> [i] -> [o]
runSys _ _ [] = []
runSys u s (i:is) = (o:os) where
  (s',o) = u s i
  os = runSys u s' is
      

testBiquad = do
  let
    -- Compute coefficients using Double precision
    c :: [Double]
    c = biquadEQ freq boost q
    freq = 1000
    boost = 10
    q = 1.0

    -- Add Exact reference point so they can be used in FloatErr computation
    c' :: [FloatErr]
    c' = fmap (float2FloatErr . double2Float) c
  
    s0 = (0,0)

    -- Compute the biquad update on a finite [FloatErr]
    u = biquadUpdate $ coefs c'

    -- Input signal
    is = [1] ++ replicate (n-1) 0
    n = 16 * 256
    -- is = fmap (float2FloatErr . sin . (* 0.1)) [0..1000]

    
    os = runSys u s0 is

  putStrLn' "coefs:"
  traverse' c' $ putStrLn' . show

  putStrLn' "output:"
  traverse (putStrLn' . show) os
    
  return ()

testOctave = do
  RunC.bePlugin


-- Use dual numbers to show that the d/dw h is 0 at w=1
testDual = do
  let
    -- Laplace transform, a=peak gain, s=Laplace parameter
    h a s = num / den where
      num = 1 + a*s + s*s
      den = 1 +   s + s*s

    -- Evaluate the transfer function amplitude at s=jw
    test1 a w = do
      putStrLn' $ "\ntest1: a=" ++ show a ++ ", w=" ++ show w
      putStrLn' $ show $ abs $ h (a :+ 0) (0 :+ w)

    -- Evaluate the norm squared as a dual complex number at s=jw
    test2 a w = do
      putStrLn' $ "\ntest2: a=" ++ show a ++ ", w=" ++ show w
      putStrLn' $ show $ h' jw * h' (-jw) where
        jw = Dual (0 :+ w) (0 :+ 1) -- we are deriving wrt to w, so derivative is 1
        h' = h (Dual (a :+ 0) 0)    -- for constants, derivative is 0

      
  putStrLn' "testDual"

  test1 10 1
  test1 10 1.01

  test2 10 1
  test2 10 0.99
  test2 10 1.01
  
testMoebius = do
  let show' :: Riemann Rational -> String
      show' = show
      m1 = Moebius3 1 1 Inf
      m2 = compM3 m1 m1
      -- show'' :: Moebius (Riemann Rational) -> String
      show'' :: Moebius3 (Riemann Float) -> String
      show'' = show
      
  -- putStrLn' $ show $ Moebius 1 2 3 4
  putStrLn' $ show' $ Fin 1 2
  putStrLn' $ show' $ (Fin 1 2) / (Fin 3 4)

  putStrLn' $ show' $ (Fin 1 2) / (Fin 3 4)

  putStrLn' $ show'' m1

  putStrLn' $ show'' bilinM3

  return ()


testFF = do
  let cyc n = putStrLn' $ show $ (length c, c) where
        c = genCycle $ F2 n

      conv' :: [F2] -> [F2] -> IO ()
      conv' a b = do
        putStrLn' $ "conv: " ++ show a ++ " " ++ show b
        putStrLn' $ show $ conv a b
        putStrLn' $ show $ convc a b

      fft' :: [F2] -> IO ()
      fft' sig = do
        putStrLn' $ "fft: " ++ (show $ fft sig)
        putStrLn' $ "dft: " ++ (show $ dft sig)

      dfts sig = do
        putStrLn' $ "dfts: " ++ (show sig)
        let sig' = pad (sig :: [F2])
            dft1 = dft $ sig'
            dft2 = dft $ dft1
            dft3 = dft $ dft2
            dft4 = dft $ dft3
            idft1 = idft $ dft1
            p x = putStrLn' $ show x

        p dft1
        p dft2
        p dft3
        p dft4
        putStrLn' "idft1"
        p idft1

      complexity' logn = putStrLn' $ show (logn, 2^logn, complexity logn)

  putStrLn' "testF3:complexity'"
  complexity' 9
  complexity' 10
  complexity' 11
      
  putStrLn' "testF3:fft'"
  -- fft' [0,1,0,1,16,16,0,0,16,0,1,0,1,0,1,16]
  fft' [0,1]

  putStrLn' "testF3"
  conv' [1] [1]
  conv' [1] [1,1]
  conv' [1,1] [1,1]
  conv' [1,1,1] [1,1,1]

  
  -- cyc 2 ; cyc 3  -- just to find the generator
  dfts [1,2,3]
  quickCheckFF

  
main = do
  args <- getArgs
  putStrLn' $ "synth-tools.hs: " ++ (show args)

  let tests = [
        ("Eval",     testEval),
        ("Variance", testVariance),
        ("Comp",     testComp),
        ("BQCoefs",  testBQCoefs),
        ("Exact",    testExact),
        ("SNR",      testSNR),
        ("Biquad",   testBiquad),
        ("ZT",       testZT),
        ("Dual",     testDual),
        ("Moebius",  testMoebius),
        ("FF",       testFF)
        ]
      tests' = [
        -- These read from stdin so don't put them in the full list.
        ("Octave",   testOctave)
        ]
      doRun name run = do
        putStrLn' $ "\ntest: " ++ name
        run
        
  case args of
    [] -> do
      traverse' tests $ \(name, run) -> doRun name run
      return ()
    [name] ->
      case Prelude.lookup name (tests ++ tests') of
        Nothing  -> putStrLn' $ "unknown test: " ++ name
        Just run -> doRun name run
    args ->
      putStrLn' $ "invalid args: " ++ show args

      
  -- RunC.test
  




