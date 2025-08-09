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


-- Why are these necessary?

-- No longer needed
{-# LANGUAGE UndecidableInstances #-}  -- Rearranged const implementation
-- {-# LANGUAGE IncoherentInstances #-}  -- Num (r t) constraint in e.g. ramp
-- {-# LANGUAGE TypeOperators #-}
-- {-# LANGUAGE ExistentialQuantification #-}
-- {-# LANGUAGE DeriveAnyClass #-}
-- {-# LANGUAGE RankNTypes #-}


import Data.Stream
import Data.Functor
import Data.Dynamic
import Control.Applicative hiding (Const)
import Control.Monad.Writer.Lazy
import Data.Proxy
import qualified Data.IntMap.Lazy as IntMap

-- import GHC.TypeLits
import GHC.TypeNats
import Prelude hiding (take, const, zipWith)


import qualified Data.Reify as Reify
import qualified Data.Graph as Graph

-- Number types need to be representable in C.  This class ensures
-- that.  Note that in Eval semantics the Haskell type is used.  Also
-- add the Typeable constraint here needed by toDyn for Data.Reify

class (Typeable t, Num t) => DSLNum t where
  dslnum :: t -> CNum

data CNum = I Int | F Float
  deriving (Show)

instance DSLNum Int   where dslnum = I
instance DSLNum Float where dslnum = F

data Prim2 = Add | Sub | Mul deriving (Show)
data Prim1 = Abs | Sig       deriving (Show)

-- The ability to reify a represented type needs to be a property of
-- the DSL not just the implementation, because the class constraint
-- will need to go in the definition of the class members.  For Eval
-- this is unused but can be toDyn.  For Comp it is essential as types
-- will need to be representable in C eventually.

data Type = TFloat | TInt | TAny
          | TArray Int Type
          | TPair Type Type
          deriving (Show)

class DSLType r t where
  dslType :: r t -> Type

class DSL r where
  signal  :: (Typeable s, Typeable t) => r s -> (r s -> (r s, r t)) -> r t
  const   :: DSLNum t => t -> r t
  op1     :: (DSLType r t, Num t) => Prim1 -> r t -> r t
  op2     :: Num t => Prim2 -> r t -> r t -> r t
  pack    :: r a -> r b -> r (a, b)
  unpack  :: r (a, b) -> (r a, r b)

class DSLArr r a t where
  array :: Typeable t => (r Int -> r t) -> r (a t)
  ref   :: r (a t) -> r Int -> r t



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

instance (DSLType r t, DSLNum t, DSL r) => Num (r t) where
  (+) = op2 Add
  (-) = op2 Sub
  (*) = op2 Mul
  abs = op1 Abs
  signum = op1 Sig
  fromInteger = const . fromInteger


-- This is just a phantom tag
data Arr (n :: Nat) a

arrLength :: forall (n :: Nat) a. KnownNat n => Arr n a -> Natural
arrLength _ = natVal (Proxy :: Proxy n)

-- Library functions

-- Note that if Num (r t) constraint is not made explicit it will
-- complain that IncoherentInstances is necessry.
ramp :: (DSL r, DSLNum t, Num (r t)) => t -> r t
ramp init = signal (const init) (\s -> (s + 1, s))


-- Test fuction for composite state
swap :: (DSL r, Typeable t, DSLNum t) => t -> t -> r t
swap ia ib = signal iab update where
  iab = pack (const ia) (const ib)
  update s =
    let (sa, sb) = unpack s
        s' = pack sb sa  -- flip states
        out = sa
    in (s', out)

data Eval t = Eval { unEval :: Stream t }
  deriving (Functor)


instance Applicative Eval where
  pure = Eval . pure
  (Eval f) <*> (Eval a) = Eval $ f <*> a


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

  -- Attempt to give _some_ semantics to data structures before
  -- implementing Comp instance.
  pack (Eval a) (Eval b) = Eval $ zipWith (,) a b
  unpack (Eval ab) = (Eval $ fmap fst ab, Eval $ fmap snd ab)

  const = Eval . pure


instance DSLArr Eval (Arr n) t where
  array f = Eval a where
    -- n = arrLength a
    a = undefined
  ref = error ""
  




-- Graph node type
--
-- . Comp t phantom type wraps Stx tree type.
--
-- . Stx tree type is defined as Mu Node with generic MuRef instance
--   from data-reify to allow translation to Graph Node.
--
-- . Using Mu approach from the data-reify paper to be able to use a
--   generic MuRef implementation.  Note that the paper appears to be
--   missing the In deconstruction for traverse.

data Term s = Op1 Prim1 s
            | Op2 Prim2 s s
            | Const CNum
            | Var Dynamic
            | Signal s s s s
            -- Multiple of the same, representable for C base type.
            | Array s s | Ref s s
            -- For heterogeneous collections.
            | Pair s s | Fst s | Snd s
            deriving (Show, Functor, Foldable, Traversable)



data Node s = Node Type (Term s)
            deriving (Show, Functor, Foldable, Traversable)
  

data VarType = State
  deriving (Show)

-- Stx as fixed point of Node
newtype Mu a = In (a (Mu a))
type Stx = Mu Node
instance Show Stx where
  show (In n) = show n

-- Generic MuRef for Stx -> Graph Node conversion
instance Traversable a => Reify.MuRef (Mu a) where
  type DeRef (Mu a) = a
  mapDeRef f (In expr) = traverse f expr

-- Comp phantom type wrapper implements DSL for Stx
data Comp t = Comp { unComp :: Stx }
  deriving (Show, Functor)



instance DSL Comp where
  op1 = comp1
  op2 = comp2
  
  signal compInit update = sig where
    Comp init = compInit
    -- stateType = compType compInit  -- FIXME: not working yet
    uniqueTag = toDyn (init, update)
    typ = TAny
    var = In $ Node typ $ Var $ uniqueTag
    (Comp state, Comp out) = update $ Comp var
    sig = Comp $ In $ Node TAny $ Signal init var state out
    
  pack (Comp a) (Comp b) = Comp $ In $ Node TAny $ Pair a b
  
  unpack (Comp ab) = (Comp $ In $ Node TAny $ Fst ab,
                      Comp $ In $ Node TAny $ Snd ab)
  const c = Comp $ In $ Node TAny $ Const $ dslnum c

-- Implementable types.
instance DSLType Comp Int   where dslType _ = TInt
instance DSLType Comp Float where dslType _ = TFloat


instance DSLArr Comp (Arr n) t where
  array f = a where
    uniqueTag = toDyn f
    var = In $ Node TAny $ Var $ uniqueTag
    Comp val = f $ Comp var
    a = Comp $ In $ Node TAny $ Array var val
  ref (Comp a) (Comp i) = Comp $ In $ Node TAny $ Ref a i

comp1 op1 (Comp a)          = Comp $ In $ Node TAny $ Op1 op1 a
comp2 op2 (Comp a) (Comp b) = Comp $ In $ Node TAny $ Op2 op2 a b

instance DSLType Eval Int where dslType _ = TInt

-- Wrap the Unique type (Int) to allow for Show instance
data Reg = Reg Int
instance Show Reg where
  show (Reg u) = "r" ++ show u

-- Override the generic Graph Show instance
data Let = Let (Reify.Graph Node)

instance Show Let where
  show (Let (Reify.Graph bindings return)) = str where
    str = "let\n" ++ concat bs ++ r
    bs = fmap showB $ reverse bindings
    showB (node, Node typ term) =
      "  " ++
      show typ ++ " " ++
      show (Reg node) ++ " = " ++
      showTerm (fmap Reg term) ++ "\n"
    r = "in " ++ show (Reg return) ++ "\n"
    showTerm (Var _) = "Var" -- Don't print the Dynamic tag
    showTerm t = show t
    
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



-- Perform topological sort using Data.Graph.  It does seem that the
-- output of Reify.reifyGraph is already topologically sorted.

tsort (Let (Reify.Graph assoc ret)) = Let $ Reify.Graph assoc' ret where
  -- Convert to Data.Graph representation
  (graph, unVertex, _) = Graph.graphFromEdges $
    fmap (\(key, node) -> (node, key, edges' node)) $ reverse assoc
  edges' (Node _ t) = edges t
  edges (Op1 _ a)        = [a]
  edges (Op2 _ a b)      = [a, b]
  edges (Signal a b c d) = [a, b, c, d]
  edges (Array a b)      = [a, b]
  edges (Ref a b)        = [a, b]
  edges (Pair a b)       = [a, b]
  edges (Fst a)          = [a]
  edges (Snd a)          = [a]
  edges _                = []

  -- Sort
  vs = Graph.topSort graph

  -- Convert back
  assoc' = fmap f vs
  f v = (key, n) where
    (n, key, _) = unVertex v


compile :: Comp t -> IO Let
compile (Comp s) = do
  s' <- Reify.reifyGraph $ s
  let s'' = Let s'
  -- topological sort doesn't seem to be necessary, reifyGraph seems
  -- to produce sorted ouput
  -- let s''' = tsort s''
  return s''
    

testComp = do
  -- Define some Comp terms with sharing
  let s1 = 1 :: Comp Int
      s2 = s1 + s1 -- add s1 s1
      s3 = ramp 0 :: Comp Int
      s4 = ramp 1 :: Comp Int
      s5 = s3 + s4
      s6 = swap 0 1 :: Comp Int
      s7 = (array $ \i -> i + 1) :: Comp (Arr 3 Int)
      s8 = ref s7 0

      test s = do
        --putStrLn "Comp tree:"
        --putStrLn $ show $ unComp s
        s' <- compile s
        putStrLn "Comp graph:"
        putStr $ show $ s'
        return s'

  -- Compile and print them
  test s2
  test s3
  test s5
  test s6
  test s7
  test s8


  
