-- Compiles concrete Refiy.Graph Node Int syntax graph to Verilog.
-- Just start over and get inspiration from older Seq when necessary.

-- FIXME: ToV and ToC could probably shared some code to build the
-- data structures.

-- TODO
-- . Review Seq first to get an idea of basic semantics.
-- . The unit is the module
-- . Everything is combinatorial networks between registers
-- . I think nodes can be wires, state variables regs
-- . consts can be localparam or just inlined
-- . every variable has a bit size, or is 1 bit, which needs DSL level support



{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE RecursiveDo #-}
{-# LANGUAGE BlockArguments #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

module SynthTools.ToV where

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



data VarKind = WireVar | RegVar deriving (Show)


data ToVState = ToVState {
  _variables :: IntMap VarKind,     -- Currently visible variables
  _bindings  :: IntMap (Node Int),  -- All syntax nodes
  _stateVars :: IntMap [Int]        -- Map state var to [dim]
  }
newtype ToVM t = ToVM (WriterT String
                      (State ToVState)
                      t)
               deriving (Functor, Applicative, Monad, MonadFix,
                         MonadWriter String,
                         MonadState ToVState)

instance MonadFail ToVM where
  fail = error "MonadFail ToVM"


$(makeLenses ''ToVState)

toV :: Let -> String
toV (Let (Reify.Graph bindings' retval)) = codeString where
  (ToVM m) = mdo

    let pfx = ""

    tell $ concat decls

    -- Perform tree traversal which emits Verilog code and construct
    -- analysis data (FIXME).
    need retval

    -- Generate declarations from gathered variables.
    variables' <- use variables
    let
      fmt RegVar var' = ["reg ",var'," = 0;\n"]
      fmt WireVar var' = ["wire ",var',";\n"]
      f (var, varKind) = do
          var' <- fmtVar var
          return $ concat $ fmt varKind var'
    decls <- traverse f $ toAscList variables'
    return ()
    
  state0 = ToVState mempty (fromList bindings') mempty
  (((), codeString), _state) = runState (runWriterT m) state0

c = concat
s = show


node :: Int -> ToVM (Node Int)
node key = do
  bindings' <- use bindings
  let (Just node) = lookup key bindings'
  return node

typeOf var = do
  (Node typ _) <- node var
  return typ

tab = "  "
indent :: ToVM ()
indent = do
  let n = 1
  tell $ c $ replicate n tab

defined var = do
  variables' <- use variables
  case lookup var variables' of
    Just _  -> return True
    Nothing -> return False

-- Write Verilog code for variable definition if it's not in the dictionary.
need var = do
  defined' <- defined var
  when (not defined') $ do
    node' <- node var
    variables %= insert var WireVar
    emit var node'

fmtVar var = do
  return $ "s" ++ show var

-- Write indented line of C code.
emitV strings = do
  indent
  tell $ c $ strings
  tell $ "\n"

emit outVar (Node t (Const c)) = do
  outVar' <- fmtVar outVar
  emitV [outVar'," <= ",show c,";"]

emit outVar (Node t (Op p aVars@[a1,a2])) = do
  let prim Add = "+"
  traverse need aVars
  outVar' <- fmtVar outVar
  as'@[a1',a2'] <- traverse fmtVar aVars
  emitV [outVar'," <= ",a1'," ",prim p," ",a2',";"]

emit sigOutVar (Node outType (Signal _init stateVar nextStateVar outVar)) = do
  -- Ignore _init which is in a distinct pass for the init code

  -- FIXME: It might be interesting to keep array anyway in the
  -- Verilog target, but implement it as a macro, a way to instantiate
  -- multiple state machines of the same type.  I've removed the glue
  -- code for now though.  It can be recovered from ToC.
  
  -- Declare the state variable.
  -- At this point we also know the dimension of the state
  let stateVarDims = []
  variables %= insert stateVar RegVar         -- mark as state variable
  stateVars %= insert stateVar stateVarDims   -- store state dimensions
  
  -- Recurse into arguments to ensure that all intermediate variables
  -- are defined.
  need nextStateVar
  need outVar

  -- emitVSignal (stateVar, nextStateVar) (outType, sigOutVar, outVar)
  -- emitVSignal (stateVar, nextStateVar) (outType, sigOutVar, outVar) = do
  
  -- Emit the update code.
  nextStateVar' <- fmtVar nextStateVar
  outVar'       <- fmtVar outVar
  stateVar'     <- fmtVar stateVar
  sigOutVar'    <- fmtVar sigOutVar
  
  emitV [sigOutVar'," = ",outVar',";"]
  -- TO CHECK: I think it is enough to put the state assignment last
  -- because it will not be referenced in any more in the current
  -- state update interation.  If output refers state it would have
  -- been copied into a different variable by now.
  emitV [stateVar'," = ",nextStateVar',";"]


  
  -- array/matrix/... from the loopVars.

-- This does not generalize to Verilog because there is no appropriate
-- target representation of array.  The macro version can just be
-- Haskell code, so just don't implement this, or use a Comp flag that
-- would expand it as a macro.  This points at semantics hierarchy for
-- the DSL:

-- 1. Base expression language
-- 2. Causal signal feedback
-- 3. Array definition entangled with 2.

emit arrVar (Node t@(TArray size baseType) (Array loopVar resultVar)) = mdo
  emitV [ " // Array not implemented" ]

emit _ (Node _ (Var _)) = return () -- stub

emit _ _ = do
  emitV [ "// TODO toV match" ]




