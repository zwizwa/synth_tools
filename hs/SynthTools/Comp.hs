-- Compilation instance for DSL class.
-- Compiles the final tagless code to concreate datastructure Reify.Graph Node Int
-- This is the first pass towards compilation to C.
-- See SynthTools.ToC for monadic syntax traversal that implements the second pass.

{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE DeriveTraversable #-}
{-# LANGUAGE TypeFamilies #-} -- data Arr (n :: Nat)
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)
{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)

module SynthTools.Comp where

import SynthTools.DSL

import Data.Dynamic
import Prelude hiding (take, const, zipWith, lookup)
import qualified Data.Reify as Reify
import qualified Data.Graph as Graph


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

data TermNum = I Int | F Float
  deriving (Show)


data Term s = Op Prim [s]
            | Const TermNum
            | Var Dynamic
            | Signal { sigInit :: s, sigVar :: s, sigState :: s, sigOut :: s }
            -- Multiple of the same, representable for C base type.
            | Array { arrVar :: s, arrVal :: s }
            | Ref s s
            -- For heterogeneous collections.
            | Pair s s
            | Fst s
            | Snd s
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


compOp1 op a   = Comp $ In $ Node (dslType a) $ Op op [(unComp a)]
compOp2 op a b = Comp $ In $ Node (dslType b) $ Op op [(unComp a), (unComp b)]

instance DSLPrim Comp Int   where op1 = compOp1 ; op2 = compOp2
instance DSLPrim Comp Float where op1 = compOp1 ; op2 = compOp2


instance DSLSig Comp where
  
  signal compInit update = sig where
    Comp init = compInit
    uniqueTag = toDyn (init, update)
    varType = dslType compVar
    outType = dslType compOut
    var = In $ Node varType $ Var $ uniqueTag
    compVar = Comp var
    (Comp state, compOut) = update compVar
    Comp out = compOut
    sig = Comp $ In $ Node outType $ Signal init var state out

instance DSLArr Comp where

  array f = a where
    uniqueTag = toDyn f
    var = In $ Node varType $ Var $ uniqueTag
    compVar = Comp var
    varType = dslType compVar
    Comp val = f $ compVar
    arrType = dslType a
    a = Comp $ In $ Node arrType $ Array var val
    
  ref (Comp a) (Comp i) = rv where
    typ = dslType rv
    rv = Comp $ In $ Node typ $ Ref a i


instance DSLPair Comp where
    
  pack a b = Comp $ In $ Node typ $ Pair (unComp a) (unComp b) where
    typ = TPair (dslType a) (dslType b)
  
  unpack (Comp ab) = (fst, snd) where
    fst = Comp $ In $ Node (dslType fst) $ Fst ab
    snd = Comp $ In $ Node (dslType snd) $ Snd ab
    


instance DSLConst Comp Int where
  const c = compc where
    typ = dslType compc
    compc = Comp $ In $ Node typ $ Const $ I c

instance DSLConst Comp Float where
  const c = compc where
    typ = dslType compc
    compc = Comp $ In $ Node typ $ Const $ F c










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
      -- TODO implement show for DSLType
      show typ ++ " : " ++
      show (Reg node) ++ " = " ++
      showTerm (fmap Reg term) ++ "\n"
    r = "in " ++ show (Reg return) ++ "\n"
    showTerm (Var _) = "Var" -- Don't print the Dynamic tag
    showTerm t = show t
    



-- Perform topological sort using Data.Graph.  It does seem that the
-- output of Reify.reifyGraph is already topologically sorted.

tsort (Let (Reify.Graph assoc ret)) = Let $ Reify.Graph assoc' ret where
  -- Convert to Data.Graph representation
  (graph, unVertex, _) = Graph.graphFromEdges $
    fmap (\(key, node) -> (node, key, edges' node)) $ reverse assoc
  edges' (Node _ t) = edges t
  edges (Op _ as)        = as
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


reify :: Comp t -> IO Let
reify (Comp s) = do
  s' <- Reify.reifyGraph $ s
  let s'' = Let s'
  -- A topological sort doesn't seem to be necessary, reifyGraph seems
  -- to produce sorted ouput.  This is not explicitly mentioned in the
  -- documentation but it will be very obvious in the compiled output
  -- if this condition ever breaks.
  --
  -- let s''' = tsort s''
  return s''


-- See comments in Eval.hs Num Eval instances.
instance Num (Comp Int) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Comp Float) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger
