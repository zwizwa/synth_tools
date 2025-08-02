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
{-# LANGUAGE TypeFamilies #-} -- For type level functions
{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE FlexibleInstances #-}

{-# LANGUAGE DataKinds #-}
{-# LANGUAGE ScopedTypeVariables #-}

{-# LANGUAGE DeriveFoldable #-}
{-# LANGUAGE DeriveTraversable #-}

-- Why are these necessary?

-- No longer needed
-- {-# LANGUAGE UndecidableInstances #-}  -- Rearranged const implementation
-- {-# LANGUAGE IncoherentInstances #-}  -- Num (r t) constraint in e.g. ramp
-- {-# LANGUAGE TypeOperators #-}
-- {-# LANGUAGE ExistentialQuantification #-}
-- {-# LANGUAGE DeriveAnyClass #-}
-- {-# LANGUAGE RankNTypes #-}


import Data.Stream
import Data.Functor
import Data.Dynamic
import Control.Applicative hiding (Const)
import Data.Proxy
import qualified Data.IntMap.Lazy as IntMap

-- import GHC.TypeLits
import GHC.TypeNats
import Prelude hiding (take, const, zipWith)


import Data.Reify

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

class DSL r where
  signal  :: (Typeable s, Typeable t) => r s -> (r s -> (r s, r t)) -> r t
  const   :: DSLNum t => t -> r t
  op1     :: Num t => Prim1 -> r t -> r t
  op2     :: Num t => Prim2 -> r t -> r t -> r t
  pack    :: r a -> r b -> r (a, b)
  unpack  :: r (a, b) -> (r a, r b)

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

instance (DSLNum t, DSL r) => Num (r t) where
  (+) = op2 Add
  (-) = op2 Sub
  (*) = op2 Mul
  abs = op1 Abs
  signum = op1 Sig
  fromInteger = const . fromInteger

class DSLArr r a t where
  array :: Typeable t => (r Int -> r t) -> r (a t)
  ref   :: r (a t) -> r Int -> r t

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
  


data VarType = State
  deriving (Show)

-- Note that the Dynamic in Var constructor is used for node equality
-- by reifyGraph.  If not needed it can just be set to 'todyn ()'

-- Graph node type
data Node s = Op1N Prim1 s
            | Op2N Prim2 s s
            | ConstN CNum
            | VarN Dynamic
            | SignalN s s s s
            | PairN s s | FstN s | SndN s
            | ArrayN s s | RefN s s
            deriving (Show, Functor, Foldable, Traversable)

newtype Mu a = In (a (Mu a))

type Stx = Mu Node
instance Show Stx where
  show (In n) = show n

instance MuRef Stx where
  type DeRef Stx = Node
  mapDeRef f (In expr) = traverse f expr




-- Comp has phantom type to be able to implement the DSL.
data Comp t = Comp { unComp :: Stx }
  deriving (Show, Functor)

comp1 op1 (Comp a)          = Comp $ In $ Op1N op1 a
comp2 op2 (Comp a) (Comp b) = Comp $ In $ Op2N op2 a b

instance DSL Comp where
  op1 = comp1
  op2 = comp2

  signal (Comp init) update = Comp $ In $ SignalN init var state out where
    uniqueTag = toDyn (init, update)
    var = In $ VarN $ uniqueTag
    (Comp state, Comp out) = update $ Comp var
 
  pack (Comp a) (Comp b) = Comp $ In $ PairN a b
  unpack (Comp ab) = (Comp $ In $ FstN ab, Comp $ In $ SndN ab)

  const = Comp . In . ConstN . dslnum

instance DSLArr Comp (Arr n) t where
  array f = a where
    uniqueTag = toDyn f
    var = In $ VarN $ uniqueTag
    Comp val = f $ Comp var
    a = Comp $ In $ ArrayN var val
  ref (Comp a) (Comp i) = Comp $ In $ RefN a i

-- main = do
--   putStrLn "synth-tools.hs main disabled"

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

-- Topological sort
tsort (Graph assoc init) = rv where
  intmap = IntMap.fromList assoc
  node = lookup intmap
  rv = error "NI"
  

testComp = do
  let s1 = 1 :: Comp Int
      s2 = s1 + s1 -- add s1 s1
      s3 = ramp 0 :: Comp Int
      s4 = ramp 1 :: Comp Int
      s5 = s3 + s4
      s6 = swap 0 1 :: Comp Int
      s7 = (array $ \i -> i + 1) :: Comp (Arr 3 Int)
      s8 = ref s7 $ 0

      test (Comp s) = do
        --putStrLn "Comp tree:"
        --putStrLn $ show $ s
        s' <- reifyGraph $ s
        putStrLn "Comp graph:"
        putStrLn $ show $ s'
        let (Graph assoc init) = s'
            intmap = IntMap.fromList assoc
        --putStrLn "IntMap:"
        --putStrLn $ show $ intmap
        return ()

      

  test s2
  test s3
  test s5
  test s6

  test s7
  test s8


  
