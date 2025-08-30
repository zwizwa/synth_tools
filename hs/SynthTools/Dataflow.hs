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

-- {-# LANGUAGE ApplicativeDo #-}

module SynthTools.Dataflow where

import Control.Applicative
import Control.Monad
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


-- How to express the isomorphism between a Node t and a [([Name],t)]?
-- I don't quite see how to do this with generic functions, so let's
-- do it manually first.
flatten   :: Node t -> Paths t
flatten = f [] where
  f path (Leaf leaf) = [(path, leaf)]
  f path (Table tab) = concat $ fmap (f' path) $ toList tab
  f' path (name, node) = f (name:path) node


mapPaths :: ([Name] -> a -> b) -> Node a -> Node b
mapPaths f = down [] where
  down ns (Leaf leaf) = Leaf $ f ns leaf
  down ns (Table table) = Table $ mapWithKey f' table where
    f' n = down (n:ns)

-- Use the Traversable instance of List to 1. set the order and 2. get the key.
traversePaths :: Applicative f => ([Name] -> a -> f b) -> Node a -> f (Node b)
traversePaths f = down [] where
  down ns (Leaf leaf)   = fmap Leaf  $ f ns leaf
  down ns (Table table) = fmap (Table . fromList) $ traverse f' $ toAscList table where
    f' (n,a) = fmap (n,) $ down (n:ns) a
    
  
  


mangle [] = ""
mangle (n:ns) = "_" ++ n ++ mangle ns

-- setter :: Params -> Path -> C
pp = pPrintNoColor

traverse' = flip traverse

test = do
  putStrLn "SynthTools.Dataflow"
  let base = Leaf $ VFloat 0.0
      wrap name inner = Table $ fromList [(name, inner)]
      params = wrap "a" $ wrap "b" $ wrap "c" $ base
      params :: Node Value

  putStrLn "mangle:"
  putStrLn $ mangle ["a","b","c"]

  putStrLn "params:"
  pp params

  putStrLn "traverse:"
  traverse' params $ \node -> putStrLn $ show node

  putStrLn "flatten:"
  pp $ flatten params
  
