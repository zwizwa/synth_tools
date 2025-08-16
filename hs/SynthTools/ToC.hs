-- Compiles concrete Refiy.Graph Node Int syntax graph to C code
-- string by monadic traversal.

{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE RecursiveDo #-}

module SynthTools.ToC where

import SynthTools.DSL
import SynthTools.Comp

import qualified Data.List as List
import Control.Monad.State
import Control.Monad.Writer
import Control.Monad
import Control.Monad.Fix
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
               deriving (Functor, Applicative, Monad, MonadFix,
                         MonadWriter String,
                         MonadState ToCState)

instance MonadFail ToCM where
  fail = error "MonadFail ToCM"


$(makeLenses ''ToCState)

toC :: Let -> String
toC (Let (Reify.Graph bindings' retval)) = codeString where
  (ToCM m) = need retval
  state0 = ToCState mempty (fromList bindings') [] mempty
  (((), codeString), _state) = runState (runWriterT m) state0


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

fmtPrim TInt Add = "addi"
fmtConst (I i) = show i
fmtConst (F f) = show f ++ "f"

-- Recursive slice substitution.
fmtSlice arrVar = do
  arrSlice <- maybeSlice arrVar
  arrVar'  <- fmtVar arrVar
  case arrSlice of
    Nothing -> return arrVar'
    Just (pArrVar, pLoopVar) -> do
      pLoopVar' <- fmtVar pLoopVar
      fmt <- fmtSlice pArrVar
      return $ concat $ [fmt,"[",pLoopVar',"]"]



node :: Int -> ToCM (Node Int)
node key = do
  bindings' <- use bindings
  let (Just node) = lookup key bindings'
  return node

defined var = do
  variables' <- use variables
  case lookup var variables' of
    Just _  -> return True
    Nothing -> return False
    
typeOf var = do
  (Node typ _) <- node var
  return typ

indent :: ToCM ()
indent = do
  loopVars' <- use loopVars
  let n = length loopVars'
  tell $ concat $ replicate n "  "


-- Write C code for variable definition if it's not in the dictionary.
need var = do
  defined' <- defined var
  when (not defined') $ do
    node' <- node var
    variables %= insert var LocalVar
    emit var node'

-- Write indented line of C code.
emitC strings = do
  indent
  tell $ concat $ strings
  tell $ "\n"

emit outVar (Node t (Const c)) = do
  (outVarDecl',_) <- fmtVarDecl t outVar
  emitC [outVarDecl'," = ",fmtConst c]

emit outVar (Node t (Op p as)) = do
  traverse need as
  (outVarDecl',_) <- fmtVarDecl t outVar
  as'             <- fmtArgs as
  emitC [outVarDecl'," = ",fmtPrim t p,"(",as',")"]

emit outVar (Node outType (Signal _init stateVar stateVal outVal)) = do
  -- Ignore _init which is in a distinct pass for the init code

  -- Declare the state variable.
  variables %= insert stateVar StateVar

  -- Recurse into arguments to ensure that all intermediate variables
  -- are defined.
  need stateVal
  need outVal
    
  -- Emit the update code.
  -- FIXME: State is loop-dependent
  stateVal' <- fmtVar stateVal
  outVal'   <- fmtVar outVal
  stateVar' <- fmtVar stateVar
  (outVarDecl', outVar') <- fmtVarDecl outType outVar
  
  emitC [outVarDecl'," = ",outVal',  ";"]
  -- TO CHECK: I think it is enough to put the state assignment last
  -- because it will not be referenced in any more in the current
  -- state update interation.  If output refers state it would have
  -- been copied into a different variable by now.
  emitC [stateVar',  " = ",stateVal',";"]

-- FIXME: this emits array assigments and needs to be translated to
-- use slices.
emit arrVar (Node t@(TArray size baseType) (Array loopVar resultVar)) = mdo

  -- Naming scheme below:
  -- xVar  :: Int     is a Graph node key
  -- vVar' :: String  is the C code representation
  
  -- Save variable environment and loop nesting before entering the loop.
  snapVariables  <- use variables
  snapLoopVars   <- use loopVars

  -- Introduce the variable before it is used in formatting below.
  variables %= insert loopVar LoopVar

  -- Format the loop head.
  (arrVarDecl',  arrVar') <- fmtVarDecl t arrVar
  (loopVarDecl', loopVar') <- fmtVarDecl TInt loopVar

  -- It is possible that arrVar has been marked as slice by
  -- surrounding code.  In that case we need to omit the declaration.
  arrSlice <- maybeSlice arrVar
  case arrSlice of
    (Just _) -> emitC ["// omit array declaration ",arrVarDecl']
    Nothing  -> emitC [arrVarDecl',";"]
  emitC ["for(",loopVarDecl'," = 0; ",loopVar'," < ",show size,"; ",loopVar', "++) {"]
  loopVars %= (++[loopVar])

  -- Recurse into the loop body that computes the array element.
  -- Set up any slice aliasing.  If the resultVar is an array then we
  -- need to alias so it can be filled in place in the inner loop.
  resultType <- typeOf resultVar
  when (isTArray resultType) $ do
    slices %= insert resultVar (arrVar, loopVar)
    emitC ["// define slice ",resultVar'," == ",arrVar',"[",loopVar',"]"]
  need resultVar
  resultVar' <- fmtVar resultVar
  
  -- Store the result in an array.
  -- 3 Things can happen here:
  resultSlice <- maybeSlice resultVar
  let sliceAssign = [arrVar',"[",loopVar',"] = ",resultVar',";"]
  case (arrSlice, resultSlice) of
    -- If arr is a slice: Slice substitution assignment. FIXME: Recurse slice.
    (Just (parentArr, parentVar),_) -> do
      parentArr' <- fmtVar parentArr
      parentVar' <- fmtVar parentVar
      slice' <- fmtSlice arrVar
      emitC $ ["// use ",arrVar'," == ", slice', " to implement "] ++ sliceAssign
      emitC $ [slice', "[",loopVar',"] = ",resultVar',";"]
    -- If result is a slice: omit assignment
    (_,Just _) -> do
      emitC $ ["// omit slice assignment "] ++ sliceAssign
    -- Otherwise normal single-element array assignment
    (_,_) ->
      emitC sliceAssign

  -- FIXME: s10 test does not omit middle slice assignment
      
  -- Restore variable environment and loop nesting after leaving the
  -- loop.  Variables that were declared inside the loop are no longer
  -- visible.
  variables .= snapVariables
  loopVars  .= snapLoopVars
  emitC ["}"]

emit _ (Node _ (Var _)) = return () -- stub

emit _ _ = do
  emitC [ "// TODO toC match" ]


maybeSlice var = do
  slices' <- use slices
  return $ lookup var slices'
