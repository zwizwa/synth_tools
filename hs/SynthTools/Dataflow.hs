-- The point here is to do the same as the dataflow.lua code in a
-- backwards compatible way, which means to start out with primitive
-- processors that have a set of controls with defaults and create
-- composition mechanisms that can compose the control and DSP
-- routing.
--
-- It would be nice if this could be done by abstracting the
-- composition mechanism as well, e.g. allow for either C functions or
-- inlined code at all the levels.  But let's start out with just some
-- phantom types for composing the C code, like the dataflow.lua code
-- does.

{-# LANGUAGE DeriveTraversable #-}
{-# LANGUAGE DeriveAnyClass #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE NoMonomorphismRestriction #-}
{-# LANGUAGE TupleSections #-}
{-# LANGUAGE FlexibleContexts #-}

-- {-# LANGUAGE ApplicativeDo #-}

module SynthTools.Dataflow where

import Control.Applicative
import Control.Monad
import Control.Monad.Writer
import Control.Monad.Identity
import Text.Pretty.Simple
import Data.Map


-- The basic abstraction is something that can be represented by OSC:
-- a parameter tree.
data Value = VFloat Float
           | VInt Int
           deriving (Show)
type Name = String
type Table t = Map Name (Node t)
data Node t = Leaf t | Table (Table t)
            deriving (Show, Functor, Foldable, Traversable)

type Params = Node Value
type Path = [Name]
type Paths t = [(Path,t)]

type C = String

newtype CValue = CValue Value
instance Show CValue where
  show (CValue (VFloat f)) = show f
  show (CValue (VInt i)) = show i



-- Use the Traversable instance of List to 1. set the order via
-- toAscList and 2. get the key inside the traversal.
traverseWithPath :: Applicative f => (Path -> a -> f b) -> Node a -> f (Node b)
traverseWithPath f = trav [] where
  trav ns (Leaf leaf)   = fmap Leaf $ f ns leaf
  trav ns (Table table) = fmap (Table . fromList) $ traverse f' $ toAscList table where
    f' (n,a) = fmap (n,) $ trav (ns ++ [n]) a
    
mapPaths :: (Path -> a -> b) -> Node a -> Node b
mapPaths f = runIdentity . traverseWithPath f' where f' p v = pure $ f p v

flatten :: Node t -> Paths t
flatten n = ps where
  (_, ps) = runWriter $ traverseWithPath f n
  f p v = do tell [(p,v)] ; return ()

-- Note that these perform overwrites of incompatible substructure.
setPath :: forall t. t -> Path -> Node t -> Node t
setPath v [] (Table _)   = Leaf $ v
setPath v ps (Table tab) = Table $ setTable v ps tab
setPath v ps (Leaf _)    = Table $ setTable v ps mempty

setTable :: forall t. t -> Path -> Table t -> Table t
setTable v [] _  = error "Empty Path"
setTable v [p] t = insert p (Leaf v) t
setTable v (p:ps) t = insert p v' t where
  v' = setPath v ps node
  node = case Data.Map.lookup p t of
    Just n -> n
    Nothing -> Table mempty
    


mangle [] = ""
mangle (n:ns) = "_" ++ n ++ mangle ns

-- setter :: Params -> Path -> C
pp = pPrintNoColor

c = Prelude.concat

test = do
  putStrLn "SynthTools.Dataflow"
  let base = Leaf $ VFloat 0.0
      -- wrap name inner = Table $ fromList [(name, inner), (name ++ "0", inner)]
      wrap name inner = Table $ fromList [(name, inner)]
      params = wrap "a" $ wrap "b" $ wrap "c" $ base
      params :: Node Value

  putStrLn "mangle:"
  putStrLn $ mangle ["a","b","c"]

  putStrLn "params:"
  pp params

  putStrLn "flatten:"
  pp $ flatten params

  -- For generating OSC setters and initializers.
  putStrLn "traverse:"
  flip traverseWithPath params $
    \p v -> putStrLn $ c ["set",mangle p,"(s, ",show $ CValue v,");"]

