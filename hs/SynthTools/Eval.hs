{-# LANGUAGE DeriveFunctor #-}
{-# LANGUAGE MultiParamTypeClasses #-} -- DSLArr r a t
{-# LANGUAGE FlexibleInstances #-} -- Num (r t)

module SynthTools.Eval where

import SynthTools.DSL

import Data.Stream hiding (fromList)
import Prelude hiding (const, zipWith)

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

  array f = arr where
    arr = Eval $ fmap Arr streams
    len = fromIntegral $ arrLength arr
    streams = distribute $ fmap stream $ [0..len-1]
    stream i = s where Eval s = f $ const i
  
  ref (Eval as) (Eval is) = Eval es where
    es = liftA2 ref' as is
    ref' (Arr a) i = a Prelude.!! i

instance DSLConst Eval Int   where const = Eval . pure
instance DSLConst Eval Float where const = Eval . pure


-- Generic numeric primitive functions and Num instances.  It seems
-- simplest to just spell out the Num instances to avoid the need for
-- UndecidableInstances.  I don't know how else to constrain them in
-- generic Num (r t) form to avoid duplication. Also dependencies on
-- Num are kept out of the base language classes.

---- This needs UndecidableInstances.  Probably ok, but might hide
---- other problems so let's not.
--
-- instance (Num t, DSLPrim r t, DSLConst r t) => Num (r t) where
--   (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
--   signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Int) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger

instance Num (Eval Float) where
  (+) = add' ; (-) = sub' ; (*) = mul' ; abs = abs'
  signum = signum' ; fromInteger = const . fromInteger




