-- Can't really eliminate that monad and stay pure.  So let's try
-- Type-Safe Observable Sharing in Haskell by Andy Gill
-- https://ku-fpg.github.io/files/Gill-09-TypeSafeReification.pdf
-- https://hackage.haskell.org/package/data-reify

-- Next: make functions observable in the data structure? Or do I just
-- do this in the Comp implementation?

{-# LANGUAGE DeriveFunctor, DeriveAnyClass #-}
{-# LANGUAGE TypeFamilies #-} -- For type level functions
{-# LANGUAGE ExistentialQuantification #-} -- For state hiding in Comp


import Data.Stream
import Data.Functor
import Control.Applicative hiding (Const)
import Prelude hiding (take, const)

import Data.Reify

class DSL r where
  constI  :: Int -> r Int
  constF  :: Float -> r Float
  signal  :: s -> (r s -> (r s, r t)) -> r t
  add     :: Num t => r t -> r t -> r t
  --lam    :: (r a -> r b) -> r (a -> b)
  --app    :: r (a -> b) -> r a -> r b

data Eval t = Eval (Stream t)
  deriving (Functor)

instance Applicative Eval where
  pure = Eval . pure
  (Eval f) <*> (Eval a) = Eval $ f <*> a

instance DSL Eval where
  constI = pure
  constF = pure
  add = liftA2 (+)
  signal init update = v where
    -- We can just tie the knot here
    (Eval s, v) = update $ Eval $ Cons init s


-- Comp has phantom type to be able to implement the DSL.


class CompState a where


data Number = I Int | F Float
  deriving (Show)

data Stx = Add Stx Stx
         | Const Number
         | Var
         | Signal Stx Stx Stx
  deriving (Show)

data StxNode s = GAdd s s
               | GConst Number
               | GVar
               | GSignal s s s
               deriving (Show)

-- Not clear how to use generic Foldable, Traversable.  Just make it explicit.
instance MuRef Stx where
  type DeRef Stx = StxNode
  mapDeRef f (Const v)      = pure $ GConst v
  mapDeRef f (Add a b)      = GAdd <$> f a <*> f b
  mapDeRef f Var            = pure $ GVar
  mapDeRef f (Signal v s o) = GSignal <$> f v <*> f s <*> f o
  
  
-- instance NewVar (Comp t) where
  

data Comp t = Comp Stx
  deriving (Show, Functor)

instance DSL Comp where
  constI = Comp . Const . I
  constF = Comp . Const . F
  add (Comp a) (Comp b) = Comp $ Add a b

  signal init update = Comp $ Signal var state out where
    var = Var
    (Comp state, Comp out) = update $ Comp var

  -- signal (Comp update) (Comp init) = Comp $ sig init where
  --   sig s = Cons v $ sig s' where
  --     (s', v) = update s
 

main = do
  putStrLn "synth-tools.hs"
  testEval
  testComp

testEval = do
  let s = (constI 1) :: Eval Int
      Eval s2 = add s s
      Eval s3 = signal 0 (\s -> (add s $ constI 1, s))
  putStrLn $ show $ take 10 s2
  putStrLn $ show $ take 10 s3

testComp = do
  let s = (constI 1) :: Comp Int
      Comp s2 = add s s
      Comp s3 = signal 0 (\s -> (add s $ constI 1, s))
      
  putStrLn $ show s2
  s2' <- reifyGraph s2
  putStrLn $ show $ s2'

  s3' <- reifyGraph s3
  putStrLn $ show $ s3'


  
