-- Can't really eliminate that monad and stay pure.  So let's try
-- Type-Safe Observable Sharing in Haskell by Andy Gill
-- https://ku-fpg.github.io/files/Gill-09-TypeSafeReification.pdf
-- https://hackage.haskell.org/package/data-reify

-- Compiler passes / intermediate forms:
-- 1. Typed haskell source
-- 2. Tree with shared nodes (Comp Stx)
-- 3. Explicit graph with backreferences (StxNode)
-- 4. Loop construction traversal (state and intermediate allocation)





{-# LANGUAGE DeriveFunctor, DeriveAnyClass #-}
{-# LANGUAGE TypeFamilies #-} -- For type level functions
{-# LANGUAGE ExistentialQuantification #-} -- For state hiding in Comp
{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE FlexibleInstances #-}
{-# LANGUAGE IncoherentInstances #-}
{-# LANGUAGE UndecidableInstances #-}

import Data.Stream
import Data.Functor
import Data.Dynamic
import Control.Applicative hiding (Const)
import Prelude hiding (take, const)


import Data.Reify

-- Primitive types can be inserted as constants.
class DSLConst r t where
  const :: t -> r t

data Prim2 = Add | Sub | Mul deriving (Show)
data Prim1 = Abs | Sig       deriving (Show)

class DSL r where
  signal  :: (Typeable s, Typeable t) => r s -> (r s -> (r s, r t)) -> r t
  op2     :: Num t => Prim2 -> r t -> r t -> r t
  op1     :: Num t => Prim1 -> r t -> r t


-- These can just be library functions.  But they can be left out
-- completely and just implemented by the Num instance.

-- add :: (Num t, DSL r) => r t -> r t -> r t
-- add = op2 Add

-- sub :: (Num t, DSL r) => r t -> r t -> r t
-- sub = op2 Sub

-- mul :: (Num t, DSL r) => r t -> r t -> r t
-- mul = op2 Mul

-- neg :: (Num t, DSL r) => r t -> r t
-- neg = op1 Neg

instance (Num t, DSL r, DSLConst r t) => Num (r t) where
  (+) = op2 Add
  (-) = op2 Sub
  (*) = op2 Mul
  abs = op1 Abs
  signum = op1 Sig
  fromInteger i = const $ fromInteger i
  


-- Library functions
ramp :: (DSL r, Typeable t, DSLConst r t, Num t) => t -> r t
ramp init = signal (const init) (\s -> (s + 1, s))



data Eval t = Eval (Stream t)
  deriving (Functor)

unEval (Eval s) = s 

instance Applicative Eval where
  pure = Eval . pure
  (Eval f) <*> (Eval a) = Eval $ f <*> a

instance DSLConst Eval Int     where const = Eval . pure
instance DSLConst Eval Float   where const = Eval . pure

eval2 Add = (+)
eval2 Sub = (-)

eval1 Abs = abs

instance DSL Eval where
  op1 = fmap   . eval1
  op2 = liftA2 . eval2

  signal init update = v where
    -- Note that the initial value is encoded as a stream where we
    -- sample the first value.  This is done in order to avoid having
    -- to represent both scalars and streams in the language which
    -- really complicates things at this point.  Maybe later when data
    -- representation is handled better this can be changed.
    Eval (Cons init0 _) = init
    
    -- Given init0 we can just tie the knot
    (Eval s, v) = update $ Eval $ Cons init0 s


-- Comp has phantom type to be able to implement the DSL.


data Number = StxInt     Int
            | StxFloat   Float
  deriving (Show)

data VarType = State
  deriving (Show)

-- Tree type
data Stx = Op1 Prim1 Stx
         | Op2 Prim2 Stx Stx
         | Const Number
         | Var Dynamic
         | Signal Stx Stx Stx Stx
  deriving (Show)

-- Graph node type
data Node s = Op1N Prim1 s
            | Op2N Prim2 s s
            | ConstN Number
            | VarN
            | SignalN s s s s
            deriving (Show)

-- Not clear how to use generic Foldable, Traversable.  Just make it explicit.
instance MuRef Stx where
  type DeRef Stx = Node
  mapDeRef f (Const v)        = pure $ ConstN v
  mapDeRef f (Var _)          = pure $ VarN
  mapDeRef f (Op1 o a)        = Op1N o <$> f a
  mapDeRef f (Op2 o a b)      = Op2N o <$> f a <*> f b
  mapDeRef f (Signal i v s o) = SignalN <$> f i <*> f v <*> f s <*> f o
  

instance DSLConst Comp Int     where  const = Comp . Const . StxInt
instance DSLConst Comp Float   where  const = Comp . Const . StxFloat

data Comp t = Comp Stx
  deriving (Show, Functor)

unComp (Comp stx) = stx

comp1 op1 (Comp a)          = Comp $ Op1 op1 a
comp2 op2 (Comp a) (Comp b) = Comp $ Op2 op2 a b

instance DSL Comp where
  op1 = comp1
  op2 = comp2

  signal (Comp init) update = Comp $ Signal init var state out where
    uniqueTag = toDyn (init, update)
    var = Var $ uniqueTag
    (Comp state, Comp out) = update $ Comp var
 


main = do
  putStrLn "synth-tools.hs"
  testEval
  testComp

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
  let s1 = 1 :: Comp Int
      s2 = s1 + s1 -- add s1 s1
      s3 = ramp 0 :: Comp Int
      s4 = ramp 1 :: Comp Int
      s5 = s3 + s4

      test (Comp s) = do
        --putStrLn "Comp tree:"
        --putStrLn $ show $ s
        s' <- reifyGraph $ s
        putStrLn "Comp graph:"
        putStrLn $ show $ s'

  test s2
  test s3
  test s5


  
