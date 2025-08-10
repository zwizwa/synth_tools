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
{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)


-- Why are these necessary?

-- No longer needed
-- {-# LANGUAGE UndecidableInstances #-}  -- Rearranged implementation (repeatedly)
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

data Prim2 = Add | Sub | Mul deriving (Show)
data Prim1 = Abs | Signum    deriving (Show)

-- The ability to reify a represented type needs to be a property of
-- the DSL not just the implementation, because the class constraint
-- will need to go in the definition of the class members.  For Eval
-- this is unused but can be toDyn.  For Comp it is essential as types
-- will need to be representable in C eventually.

data Type = TFloat | TInt | TAny
          | TArray Int Type
          | TPair Type Type
          deriving (Show)

-- Phantom tag for fixed length arrays
data Arr (n :: Nat) a

class (DSL r, Typeable t) => DSLType r t where
  dslType :: r t -> Type

instance (DSLType r a, DSLType r b) => DSLType r (a, b) where
  dslType _ = TPair (dslType a') (dslType b') where
    a' = undefined :: r a
    b' = undefined :: r b

instance DSL r => DSLType r Int   where dslType _ = TInt
instance DSL r => DSLType r Float where dslType _ = TFloat


arrLength :: forall (n :: Nat) a r. KnownNat n => r (Arr n a) -> Natural
arrLength _ = natVal (Proxy :: Proxy n)

instance (KnownNat n, DSL r, DSLType r t) => DSLType r (Arr n t) where
  dslType a = TArray size typ where
    -- Separate type level function mapping Arr to base type is not
    -- needed because we already have ref in DSL.
    rt = ref undefined undefined :: r t
    typ = dslType rt
    size = fromIntegral $ arrLength a


-- FIXME: For op1, op2 create a DSLPrimType r t constraint instead.

-- Structural
class DSL r where
  signal  :: (DSLType r s, DSLType r t) => r s -> (r s -> (r s, r t)) -> r t
  pack    :: (DSLType r a, DSLType r b) => r a -> r b -> r (a, b)
  unpack  :: (DSLType r a, DSLType r b) => r (a, b) -> (r a, r b)
  array   :: (DSLType r t, KnownNat n)  => (r Int -> r t) -> r (Arr n t)
  ref     :: (DSLType r t)              => r (Arr n t) -> r Int -> r t

-- Primitive constants
class DSLType r t => DSLConst r t where
  const   ::                               t -> r t

class DSLPrimType t
instance DSLPrimType Int
instance DSLPrimType Float

-- Primitive operations
class (DSLType r t, DSLPrimType t) => DSLPrim r t where
  op1     :: (DSLType r t)              => Prim1 -> r t -> r t
  op2     :: (DSLType r t)              => Prim2 -> r t -> r t -> r t
  
  












-- Library functions

ramp :: (DSLConst r t, Num (r t)) => t -> r t
ramp init = signal (const init) (\s -> (s + 1, s))


-- Test fuction for composite state
swap :: (DSLType r (t, t), DSLConst r t)
     => t -> t -> r t
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


eval2 Add = liftA2 (+)
eval2 Sub = liftA2 (-)
eval2 Mul = liftA2 (*)
eval1 Abs    = fmap abs
eval1 Signum = fmap signum

instance DSLPrim Eval Int   where op1 = eval1 ; op2 = eval2
instance DSLPrim Eval Float where op1 = eval1 ; op2 = eval2


instance DSL Eval where

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

  array f = Eval a where
    -- n = arrLength a
    a = undefined
  ref = error ""

instance DSLConst Eval Int   where const = Eval . pure
instance DSLConst Eval Float where const = Eval . pure


-- Generic numeric primitive functions and Num instances.  It seems
-- simplest to just spell out the Num instances to avoid the need for
-- InconsistentInstances.  I don't know how else to constrain them in
-- generic Num (r t) form to avoid duplication. Also dependencies on
-- Num are kept out of the base language classes.
 
instance Num (Eval Int) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Float) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Comp Int) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Comp Float) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

type DSLOp1 r t = r t -> r t
type DSLOp2 r t = r t -> r t -> r t

add'    :: (DSLPrim r t) => DSLOp2 r t ; add'    = op2 Add
sub'    :: (DSLPrim r t) => DSLOp2 r t ; sub'    = op2 Sub
mul'    :: (DSLPrim r t) => DSLOp2 r t ; mul'    = op2 Mul
abs'    :: (DSLPrim r t) => DSLOp1 r t ; abs'    = op1 Abs
signum' :: (DSLPrim r t) => DSLOp1 r t ; signum' = op1 Signum




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


data Term s = Op1 Prim1 s
            | Op2 Prim2 s s
            | Const TermNum
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


compOp1 op a   = Comp $ In $ Node (dslType a) $ Op1 op (unComp a)
compOp2 op a b = Comp $ In $ Node (dslType b) $ Op2 op (unComp a) (unComp b)

instance DSLPrim Comp Int   where op1 = compOp1 ; op2 = compOp2
instance DSLPrim Comp Float where op1 = compOp1 ; op2 = compOp2


instance DSL Comp where
  
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
    
  pack a b = Comp $ In $ Node typ $ Pair (unComp a) (unComp b) where
    typ = TPair (dslType a) (dslType b)
  
  unpack (Comp ab) = (fst, snd) where
    fst = Comp $ In $ Node (dslType fst) $ Fst ab
    snd = Comp $ In $ Node (dslType snd) $ Snd ab
    
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
  -- A topological sort doesn't seem to be necessary, reifyGraph seems
  -- to produce sorted ouput.  This is not explicitly mentioned in the
  -- documentation but it will be very obvious in the compiled output
  -- if this condition ever breaks.
  --
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
      s9 = (array $ \i -> array $ \j -> i + j) :: Comp (Arr 4 (Arr 5 Int))

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
  test s9


  
