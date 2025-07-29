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

import Data.Stream
import Data.Functor
import Data.Dynamic
import Control.Applicative hiding (Const)
import Prelude hiding (take, const)


import Data.Reify

-- Primitive types can be inserted as constants.
class DSLConst r t where
  const :: t -> r t

class DSL r where
  signal  :: (Typeable s, Typeable t) => r s -> (r s -> (r s, r t)) -> r t
  add     :: Num t => r t -> r t -> r t
  sub     :: Num t => r t -> r t -> r t






data Eval t = Eval (Stream t)
  deriving (Functor)

instance Applicative Eval where
  pure = Eval . pure
  (Eval f) <*> (Eval a) = Eval $ f <*> a

instance DSLConst Eval Int where
  const = Eval . pure

instance DSLConst Eval Float where
  const = Eval . pure

instance DSL Eval where
  add = liftA2 (+)
  sub = liftA2 (-)
  signal init update = v where
    -- Note that the initial value is encoded as a stream where we
    -- sample the first value.  This is done in order to avoid having
    -- to represent both scalars and streams in the language which
    -- really complicates things.  In the Comp language this can be
    -- expressed more directly.
    Eval (Cons init0 _) = init
    
    -- Given init0 we can just tie the knot
    (Eval s, v) = update $ Eval $ Cons init0 s


-- Comp has phantom type to be able to implement the DSL.


data Number = StxInt Int | StxFloat Float
  deriving (Show)

data VarType = State
  deriving (Show)

data Prim2 = Add | Sub
  deriving (Show)

-- TODO: Add type annotations.
data Stx = Op2 Prim2 Stx Stx
         | Const Number
         | Var Dynamic
         | Signal Stx Stx Stx Stx
  deriving (Show)

data StxNode s = GOp2 Prim2 s s
               | GConst Number
               | GVar
               | GSignal s s s s
               deriving (Show)

-- Not clear how to use generic Foldable, Traversable.  Just make it explicit.
instance MuRef Stx where
  type DeRef Stx = StxNode
  mapDeRef f (Const v)        = pure $ GConst v
  mapDeRef f (Var t)          = pure $ GVar
  mapDeRef f (Op2 o a b)      = GOp2 o <$> f a <*> f b
  mapDeRef f (Signal i v s o) = GSignal <$> f i <*> f v <*> f s <*> f o
  
  
-- instance NewVar (Comp t) where
  
instance DSLConst Comp Int where
  const = Comp . Const . StxInt

data Comp t = Comp Stx
  deriving (Show, Functor)

comp2 op (Comp a) (Comp b) = Comp $ Op2 op a b

instance DSL Comp where
  add = comp2 Add
  sub = comp2 Sub

  signal (Comp init) update = Comp $ Signal init var state out where
    uniqueTag = toDyn (init, update)
    var = Var $ uniqueTag
    (Comp state, Comp out) = update $ Comp var
 
-- Library functions
ramp :: (DSL r, Typeable t, DSLConst r t, Num t) => t -> r t
ramp init = signal (const init) (\s -> (add s $ const 1, s))


main = do
  putStrLn "synth-tools.hs"
  testEval
  testComp

testEval = do
  let s1 = (const 1) :: Eval Int
      Eval s2 = add s1 s1
      Eval s3 = ramp 0 :: Eval Int
  putStrLn $ show $ take 10 s2
  putStrLn $ show $ take 10 s3



testComp = do
  let s1 = (const 1) :: Comp Int
      Comp s2 = add s1 s1
      Comp s3 = ramp 0 :: Comp Int
      Comp s4 = ramp 1 :: Comp Int
      Comp s5 = add (Comp s3) (Comp s4)
      
  putStrLn $ show s2
  s2' <- reifyGraph s2
  putStrLn $ show $ s2'

  s3' <- reifyGraph s3
  putStrLn $ show $ s3'

  s5' <- reifyGraph s5
  putStrLn $ show $ s5'


  
