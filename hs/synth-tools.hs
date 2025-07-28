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

data Comp t = Add (Comp t) (Comp t)
            | ConstI Int
            | ConstF Float
            | SVar
            | Signal (Comp t)
  deriving (Show, Functor)

data CompNode s = GAdd s s
                | GLitI Int
                | GLitF Float
                | GSVar
                deriving (Show)

-- Not clear how to use generic Foldable, Traversable.  Just make it explicit.
instance MuRef (Comp t) where
  type DeRef (Comp t) = CompNode
  mapDeRef f (ConstI v) = pure $ GLitI v
  mapDeRef f (ConstF v) = pure $ GLitF v
  mapDeRef f (Add a b) = GAdd <$> f a <*> f b
  mapDeRef f SVar      = pure $ GSVar
  
-- instance NewVar (Comp t) where
  

instance DSL Comp where
  constI = ConstI
  constF = ConstF
  add = Add

  signal init update = Signal out where
    -- We can just tie the knot here
    (state, out) = update $ SVar

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
      s2 = add s s
  putStrLn $ show s2
  s2' <- reifyGraph s2
  putStrLn $ show $ s2'
  
