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

import SynthTools.DSL
import SynthTools.Eval
import SynthTools.Comp
import SynthTools.ToC


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

      test s = do
        --putStrLn "Comp tree:"
        --putStrLn $ show $ unComp s
        s' <- reify s
        putStrLn "\n** Node graph:"
        putStr $ show $ s'
        putStrLn "\n** ToC string:"
        putStr $ toC s'
        return s'

  -- Compile and print them
  test s2
  test s3
  test s5
  test s6
  test s7
  test s8
  test s9
  test s10
  
main = do
  putStrLn "synth-tools.hs"
  testEval
  testComp

