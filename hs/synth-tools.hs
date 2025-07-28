-- Can't really eliminate that monad and stay pure.  So let's try
-- Type-Safe Observable Sharing in Haskell by Andy Gill
-- https://ku-fpg.github.io/files/Gill-09-TypeSafeReification.pdf
-- https://hackage.haskell.org/package/data-reify

-- Next: make functions observable in the data structure? Or do I just
-- do this in the Comp implementation?

{-# LANGUAGE DeriveFunctor, DeriveAnyClass #-}
{-# LANGUAGE TypeFamilies #-} -- For type level functions
{-# LANGUAGE ExistentialQuantification #-} -- For state hiding in Comp
{-# LANGUAGE MultiParamTypeClasses #-}

import Data.Stream
import Data.Functor
import Control.Applicative hiding (Const)
import Prelude hiding (take, const)

import Data.Reify

class DSLConst r t where
  const :: t -> r t

class DSL r where
  signal  :: r s -> (r s -> (r s, r t)) -> r t
  add     :: Num t => r t -> r t -> r t
  --lam    :: (r a -> r b) -> r (a -> b)
  --app    :: r (a -> b) -> r a -> r b

data Eval t = Eval (Stream t)
  deriving (Functor)

instance Applicative Eval where
  pure = Eval . pure
  (Eval f) <*> (Eval a) = Eval $ f <*> a

instance DSLConst Eval Int where
  const = Eval . pure

instance DSL Eval where
  add = liftA2 (+)
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


data Number = I Int | F Float
  deriving (Show)

data VarType = State
  deriving (Show)

data Stx = Add Stx Stx
         | Const Number
         | Var VarType
         | Signal Stx Stx Stx Stx
  deriving (Show)

data StxNode s = GAdd s s
               | GConst Number
               | GVar VarType
               | GSignal s s s s
               deriving (Show)

-- Not clear how to use generic Foldable, Traversable.  Just make it explicit.
instance MuRef Stx where
  type DeRef Stx = StxNode
  mapDeRef f (Const v)        = pure $ GConst v
  mapDeRef f (Add a b)        = GAdd <$> f a <*> f b
  mapDeRef f (Var t)          = pure $ GVar t
  mapDeRef f (Signal i v s o) = GSignal <$> f i <*> f v <*> f s <*> f o
  
  
-- instance NewVar (Comp t) where
  
instance DSLConst Comp Int where
  const = Comp . Const . I

data Comp t = Comp Stx
  deriving (Show, Functor)

--instance DSLConst Comp where
--  const = Comp . Pure 

instance DSL Comp where
  add (Comp a) (Comp b) = Comp $ Add a b

  signal (Comp init) update = Comp $ Signal init var state out where
    var = Var State
    (Comp state, Comp out) = update $ Comp var


  -- signal (Comp update) (Comp init) = Comp $ sig init where
  --   sig s = Cons v $ sig s' where
  --     (s', v) = update s
 

main = do
  putStrLn "synth-tools.hs"
  testEval
  testComp

testEval = do
  let s = (const 1) :: Eval Int
      Eval s2 = add s s
      Eval s3 = signal (const 0) (\s -> (add s $ const (1 :: Int), s))
  putStrLn $ show $ take 10 s2
  putStrLn $ show $ take 10 s3

testComp = do
  let s = (const 1) :: Comp Int
      Comp s2 = add s s
      Comp s3 = signal (const 0) (\s -> (add s $ const (1 :: Int), s))
      
  putStrLn $ show s2
  s2' <- reifyGraph s2
  putStrLn $ show $ s2'

  s3' <- reifyGraph s3
  putStrLn $ show $ s3'


  
