{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TemplateHaskell #-}

module SynthTools.ToC where

import SynthTools.DSL
import SynthTools.Comp

import qualified Data.List as List
import Control.Monad.State
import Control.Monad.Writer
import Control.Monad
import Data.IntMap.Lazy

import Prelude hiding (const, lookup)

import qualified Data.Reify as Reify
import qualified Data.Graph as Graph

import Control.Lens hiding (Const)
import Control.Lens.TH





-- Compile the graph directly to (pseudo) C and refactor when it
-- becomes clear.

-- Variable location is determined by the comp pass.  The type
-- information is available in the bindings map.

data VarLoc = StateVar | LocalVar | LoopVar

data ToCState = ToCState {
  _variables :: IntMap VarLoc,      -- Currently visible variables
  _bindings  :: IntMap (Node Int),  -- All syntax nodes
  _loopVars  :: [Int],              -- Current loop nesting
  _slices    :: IntMap (Int,Int)    -- Map array var to (var,index)
  }
newtype ToCM t = ToCM (WriterT String
                      (State ToCState)
                      t)
               deriving (Functor, Applicative, Monad,
                         MonadWriter String,
                         MonadState ToCState)

instance MonadFail ToCM where
  fail = error "MonadFail ToCM"


$(makeLenses ''ToCState)


fmtVar :: Int -> ToCM String
fmtVar var = do
  sv' <- use variables
  let Just varLoc = lookup var sv'
  return $
    case varLoc of
      StateVar -> "s->s" ++ show var
      LocalVar -> "r"    ++ show var
      LoopVar  -> "l"    ++ show var

      

fmtVarDecl :: Type -> Int -> ToCM (String, String)
fmtVarDecl t var = do
  let (baseType, arrayType) = fmtType t 
  fmtVar' <- fmtVar var
  return $ (baseType ++ " " ++ fmtVar' ++ arrayType, fmtVar')

fmtType TFloat = ("float","")
fmtType TInt   = ("int","")
fmtType (TArray size baseType) = (bt, at' ++ at) where
  at' = "[" ++ show size ++ "]"
  (bt,at) = fmtType baseType

fmtArgs :: [Int] -> ToCM String
fmtArgs rands = do
  rands' <- traverse fmtVar rands
  return $ concat $ List.intersperse ", " rands'

node :: Int -> ToCM (Node Int)
node key = do
  bindings' <- use bindings
  let (Just node) = lookup key bindings'
  return node

toC :: Let -> String
toC (Let (Reify.Graph bindings' retval)) = codeString where
  (ToCM m) = need retval
  state0 = ToCState mempty (fromList bindings') [] mempty
  (((), codeString), _state) = runState (runWriterT m) state0


-- Note that the Node struct doesn't have much internal
-- structure. We we rely on the construction of the graph by typed
-- code that ensure the direct matches of ref results will succeed.

-- Probably should add variables to the environment and define them
-- locally if they are not yet defined.  I am not sure if I just
-- completely lost the structure here, i.e. where should the
-- variable definitions go when there is nesting?

defined var = do
  variables' <- use variables
  case lookup var variables' of
    Just _  -> return True
    Nothing -> return False
    
need var = do
  -- Compile if it's not in the dictionary yet.
  defined' <- defined var
  when (not defined') $ do
    node' <- node var
    variables %= insert var LocalVar
    emit var node'

typeOf var = do
  (Node typ _) <- node var
  return typ

indent :: ToCM ()
indent = do
  loopVars' <- use loopVars
  let n = length loopVars'
  tell $ concat $ replicate n "  "

fmtPrim TInt Add = "addi"
fmtConst (I i) = show i
fmtConst (F f) = show f ++ "f"


tell' strings = do
  indent
  tell $ concat $ strings
  tell $ "\n"

-- Terminal node, nothing to compile.
emit n (Node _ (Var _)) = return ()

emit n (Node t (Const c)) = do
  (decl,_) <- fmtVarDecl t n
  tell' [decl," = ",fmtConst c]

emit n (Node t (Op p as)) = do
  traverse need as
  (decl,_) <- fmtVarDecl t n
  as'      <- fmtArgs as
  tell' [decl," = ",fmtPrim t p,"(",as',")"]

emit n (Node outType (Signal _init stateVar stateVal outVal)) = do
  -- Ignore _init which is in a distinct pass for the init code

  -- Declare the state variable.
  variables %= insert stateVar StateVar

  -- Recurse into arguments to ensure that all intermediate variables
  -- are defined.
  need stateVal
  need outVal
    
  -- Emit the update code.
  -- FIXME: Can we guarantee that state is not read anymore until the
  -- next iteration?
  -- FIXME: State is loop-dependent
  stateVar' <- fmtVar stateVar
  stateVal' <- fmtVar stateVal
  tell' [stateVar'," = ",stateVal',";"]

-- FIXME: this emits array assigments and needs to be translated to
-- use slices.
emit n (Node t@(TArray size baseType) (Array loopVar resultVar)) = do
  -- Save variable environment and loop nesting before entering the loop.
  snapVariables  <- use variables
  snapLoopVars   <- use loopVars
  -- Introduce the variable before it is used in formatting below.
  variables %= insert loopVar LoopVar
  -- Format the loop head.
  (arrDecl,arr) <- fmtVarDecl t n
  (varDecl,var) <- fmtVarDecl TInt loopVar
  tell' [arrDecl,";"]
  tell' ["for(",varDecl," = 0; ",var," < ",show size,"; ",var, "++) {"]
  loopVars %= (++[loopVar])
  -- Set up any slice aliasing.  If the resultVar is an array then we
  -- need to alias so it can be filled in place in the inner loop.
  resultType <- typeOf resultVar
  when (isTArray resultType) $ do
    -- FIXME
    -- let slice = (arr,var)
    tell' [ "// FIXME"]
    return ()
  -- Recurse into the loop body that computes the array element.
  need resultVar
  result <- fmtVar resultVar
  -- FIXME
  -- 3 Things can happen here:
  -- . If arr is a slice: Slice substitution assignment
  -- . If result is a slice: omit assignment
  -- . Otherwise normal single-element array assignment
  tell' [arr,"[",var,"] = ",result,";"]
  -- Restore variable environment and loop nesting after leaving the
  -- loop.  Variables that were declared inside the loop are no longer
  -- visible.
  variables .= snapVariables
  loopVars  .= snapLoopVars
  tell' ["}"]

emit _ _ = do
  tell $ "// TODO toC match"

