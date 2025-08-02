
{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE TypeFamilies #-}
{-# LANGUAGE FlexibleInstances #-}
{-# LANGUAGE ScopedTypeVariables #-}
--{-# LANGUAGE MultiParamTypeClasses #-}
--{-# LANGUAGE DataKinds #-}
{-# LANGUAGE DeriveFoldable #-}
{-# LANGUAGE DeriveTraversable #-}
{-# LANGUAGE InstanceSigs #-}



import Data.Dynamic
import Data.Reify

-- I'm using Mu from the paper instead.
-- https://ku-fpg.github.io/files/Gill-09-TypeSafeReification.pdf
-- The Mu type from Data.Fix is different
-- import Data.Fix
newtype Mu a = In (a (Mu a))


f a = a

data Prim2 = Add | Sub | Mul deriving (Show)
data Prim1 = Abs | Sig       deriving (Show)

data CNum = I Int | F Float
  deriving (Show)

data Node s = Op1N Prim1 s
            | Op2N Prim2 s s
            | ConstN CNum
            | VarN Dynamic
            | SignalN s s s s
            | PairN s s | FstN s | SndN s
            | ArrayN s s | RefN s s
            deriving (Show, Functor, Foldable, Traversable)

type Stx = Mu Node

instance Traversable a => MuRef (Mu a) where
  type DeRef (Mu a) = a
  mapDeRef f (In expr) = traverse f expr



main = do
  putStrLn "test1.hs"
  let s1 = In $ ConstN $ I 123
      s2 = In $ Op2N Add s1 s1
  s2' <- reifyGraph $ s2
  putStrLn $ show $ s2'
