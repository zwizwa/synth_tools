-- Compiles concrete Refiy.Graph Node Int syntax graph to C code
-- string by monadic traversal.

-- TODO:
-- . Initial vialues
-- . Array refs
-- . Validate pairs
-- . Generate C code and run it

{-# LANGUAGE FlexibleContexts #-} -- constraint DSLType r (t, t)
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE RecursiveDo #-}
{-# LANGUAGE BlockArguments #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

module SynthTools.ToC where

import SynthTools.DSL
import SynthTools.Comp

import qualified Data.List as List
import Control.Monad.State
import Control.Monad.Writer
import Control.Monad
import Control.Monad.Fix
import Data.IntMap.Lazy
import Data.Dynamic

import Prelude hiding (const, lookup)

import qualified Data.Reify as Reify
import qualified Data.Graph as Graph

import Control.Lens hiding (Const)
import Control.Lens.TH





-- Compile the graph directly to (pseudo) C and refactor when it
-- becomes clear.

-- Variable location is determined by the comp pass.  The type
-- information is available in the bindings map.

data VarLoc = StateVar | LocalVar | LoopVar | OutVar deriving (Show)
data Alias = Slice Int Int -- Array slices
           | Output Int    -- Function output
           deriving (Show)

data ToCState = ToCState {
  _variables :: IntMap VarLoc,      -- Currently visible variables
  _bindings  :: IntMap (Node Int),  -- All syntax nodes
  _loopVars  :: [(Int,Int)],        -- Current loop nesting (var,range)
  _aliases   :: IntMap Alias,       -- Map array var to (var,index)
  _stateVars :: IntMap (Int,[Int]), -- Map state var to (init,[dim])
  _nextNode  :: Int                 -- Counter for creating new bindings
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
toC (Let (Reify.Graph bindings' retVar)) = codeString where
  (ToCM m) = mdo

    let pfx = ""

    -- Struct definitions need to come before C code that refers to
    -- it.  Use recursive do to avoid multiple passes.
    tell $ c ["struct ",pfx,"state {\n"]
    tell $ c stateFields
    tell $ "};\n"
    
    -- Perform tree traversal which emits C code and construct
    -- analysis maps.
    tell $ c ["void ",pfx,"run(struct ",pfx,"state *s, ",outDecl,") {\n"]
    outDecl <- needOutput retVar
    tell $ "}\n"
    
    -- Format the C structures definitions
    stateVars' <- use stateVars
    stateFields <- traverse stateField $ toAscList stateVars'
    let stateField (var, (initVar, dims)) = do
          -- Initializers are constants but might be wrapped in Pair.
          -- Node initType (Const initVal) <- node initVar
          init <- node initVar
          
          -- FIXME: This is ignoring some structure information
          -- derived from Haskell type reification.
          -- FIXME: Generate initializer
          typ <- typeOf var
          let field' = c $ [tab, baseType', " s", show var, fmtDims dims,
                            "; // init = ", show init, "\n"]
              (baseType', _FIXME) = fmtType typ
          return $ field'
    return ()

  nextNode' = 1 + (Prelude.foldr max 0 $ fmap fst bindings')
    
  state0 = ToCState mempty (fromList bindings') [] mempty mempty nextNode'
  (((), codeString), _state) = runState (runWriterT m) state0

c = concat
s = show

 
fmtVar :: Int -> ToCM String
fmtVar var = do
  sv' <- use variables
  let varLoc = case lookup var sv' of
        Just vl -> vl
        Nothing -> error $ "Internal error: fmtVar, undefined variable " ++ show var
  case varLoc of
    StateVar -> do
      loopVars' <- use loopVars
      let slv v = "[l" ++ s v ++ "]"
          index = case loopVars' of
            [] -> ""
            _ -> c $ fmap (slv . fst) loopVars'
      return $ c ["s->s",s var,index]
    LocalVar -> return $ "r"    ++ s var
    LoopVar  -> return $ "l"    ++ s var
    OutVar   -> return $ "o"    ++ s var

fmtVarDecl :: Type -> Int -> ToCM (String, String)
fmtVarDecl t var = do
  let (baseType, arrayType) = fmtType t 
  fmtVar' <- fmtVar var
  return $ (baseType ++ " " ++ fmtVar' ++ arrayType, fmtVar')

fmtType TFloat = ("float","")
fmtType TInt   = ("int","")
fmtType (TPair fst snd) = (struct,"") where
  struct = c ["struct { ",
              fstBase," ",fstC,fstArr,"; ",
              sndBase," ",sndC,sndArr,"; ",
              "}"]
  (fstBase, fstArr) = fmtType fst
  (sndBase, sndArr) = fmtType snd
fmtType (TArray size baseType) = (bt, at' ++ at) where
  at' = "[" ++ s size ++ "]"
  (bt,at) = fmtType baseType

fmtArgs :: [Int] -> ToCM String
fmtArgs rands = do
  rands' <- traverse fmtVar rands
  return $ c $ List.intersperse ", " rands'


fmtPrim TInt Add = "addi"
fmtConst (I i) = s i
fmtConst (F f) = s f ++ "f"

fmtDims :: [Int] -> String
fmtDims is = c $ fmap (\i -> c $ ["[",s i,"]"]) is

-- Recursive slice substitution.
fmtSlice arrVar = do
  arrSlice <- maybeSlice arrVar
  arrVar'  <- fmtVar arrVar
  case arrSlice of
    Nothing -> return arrVar'
    Just (Slice pArrVar pLoopVar) -> do
      pLoopVar' <- fmtVar pLoopVar
      fmt <- fmtSlice pArrVar
      return $ c $ [fmt,"[",pLoopVar',"]"]
    Just (Output pOutVar) -> do
      fmt <- fmtSlice pOutVar
      return $ c $ [fmt]


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

tab = "  "
indent :: ToCM ()
indent = do
  loopVars' <- use loopVars
  let n = 1 + length loopVars'
  tell $ c $ replicate n tab


-- Write C code for variable definition if it's not in the dictionary.
need var = do
  defined' <- defined var
  when (not defined') $ do
    node' <- node var
    variables . at var .= Just LocalVar
    emit var node'

-- Create a node with the same type as a given node.
-- Used e.g. for creating the output node alias.
newNode = do
  var <- use nextNode
  nextNode %= (+ 1)
  return var

-- Similar to need, but handle output variable differently
needOutput retVar = do
  Just retNode@(Node typ _) <- use $ bindings . at retVar
  outVar <- newNode
  bindings  . at outVar .= Just (Node typ (Var $ toDyn ())) -- dummy tag
  variables . at outVar .= Just OutVar
  variables . at retVar .= Just LocalVar
  aliases   . at retVar .= Just (Output outVar)
  (outVarDecl,_) <- fmtVarDecl typ outVar

  -- Like slice annotation
  retVar' <- fmtVar retVar
  outVar' <- fmtVar outVar
  emitC ["// define output alias: ",retVar'," == ",outVar']

  emit retVar retNode
  return $ outVarDecl
 
    

-- Write indented line of C code.
emitC strings = do
  indent
  tell $ c $ strings
  tell $ "\n"

--fstC = "fst"
--sndC = "snd"
fstC = "a"
sndC = "b"

emitPairRef outVar t pairVar field = do
  need pairVar
  (outVarDecl',_) <- fmtVarDecl t outVar
  pairVar'        <- fmtVar pairVar
  emitC [outVarDecl'," = ",pairVar',".",field,";"]

emit outVar (Node t (Const c)) = do
  (outVarDecl',_) <- fmtVarDecl t outVar
  emitC [outVarDecl'," = ",fmtConst c,";"]

emit outVar (Node t (Op p as)) = do
  traverse need as
  (outVarDecl',_) <- fmtVarDecl t outVar
  as'             <- fmtArgs as
  emitC [outVarDecl'," = ",fmtPrim t p,"(",as',");"]

emit outVar (Node t (Fst pairVar)) = emitPairRef outVar t pairVar fstC
emit outVar (Node t (Snd pairVar)) = emitPairRef outVar t pairVar sndC

emit outVar (Node t (Pair fstVar sndVar)) = do
  need fstVar ; need sndVar
  (outVarDecl',_) <- fmtVarDecl t outVar
  fstVar'         <- fmtVar fstVar
  sndVar'         <- fmtVar sndVar
  emitC [outVarDecl'," = { ",fstVar',", ", sndVar'," };"]


emit sigOutVar (Node outType (Signal init stateVar nextStateVar outVar)) = do
  -- Ignore _init which is in a distinct pass for the init code

  -- Declare the state variable.
  -- At this point we also know the dimension of the state
  loopVars' <- use loopVars
  let stateVarDims = fmap snd loopVars'
  variables . at stateVar .= Just StateVar            -- mark as state variable
  stateVars . at stateVar .= Just (init,stateVarDims) -- store state dimensions
  
  -- Recurse into arguments to ensure that all intermediate variables
  -- are defined.
  need nextStateVar
  need outVar

  -- emitCSignal (stateVar, nextStateVar) (outType, sigOutVar, outVar)
  -- emitCSignal (stateVar, nextStateVar) (outType, sigOutVar, outVar) = do
  
  -- Emit the update code.
  nextStateVar' <- fmtVar nextStateVar
  outVar'       <- fmtVar outVar
  stateVar'     <- fmtVar stateVar
  (sigOutVarDecl', sigOutVar') <- fmtVarDecl outType sigOutVar
  
  emitC [sigOutVarDecl'," = ",outVar',";"]
  -- TO CHECK: I think it is enough to put the state assignment last
  -- because it will not be referenced in any more in the current
  -- state update interation.  If output refers state it would have
  -- been copied into a different variable by now.
  emitC [stateVar'," = ",nextStateVar',";"]


  
  -- array/matrix/... from the loopVars.

-- FIXME: this emits array assigments and needs to be translated to
-- use aliases.
emit arrVar (Node t@(TArray size baseType) (Array loopVar resultVar)) = mdo

  -- Naming scheme below:
  -- xVar  :: Int     is a Graph node key
  -- vVar' :: String  is the C code representation
  
  -- Save variable environment and loop nesting before entering the loop.
  snapVariables  <- use variables
  snapLoopVars   <- use loopVars

  -- Introduce the variable before it is used in formatting below.
  variables . at loopVar .= Just LoopVar

  -- Format the loop head.
  (arrVarDecl',  arrVar') <- fmtVarDecl t arrVar
  (loopVarDecl', loopVar') <- fmtVarDecl TInt loopVar

  -- It is possible that arrVar has been marked as slice by
  -- surrounding code.  In that case we need to omit the declaration.
  arrSlice <- maybeSlice arrVar
  case arrSlice of
    (Just _) -> emitC ["// omit declaration: ",arrVarDecl']
    Nothing  -> emitC [arrVarDecl',";"]
  emitC ["for(",loopVarDecl'," = 0; ",loopVar'," < ",s size,"; ",loopVar', "++) {"]
  loopVars %= (++[(loopVar,size)])

  -- Recurse into the loop body that computes the array element.
  -- Set up any slice aliasing.  If the resultVar is an array then we
  -- need to alias so it can be filled in place in the inner loop.
  resultType <- typeOf resultVar
  when (isTArray resultType) $ do
    aliases . at resultVar .= Just (Slice arrVar loopVar)
    emitC ["// define slice alias: ",resultVar'," == ",arrVar',"[",loopVar',"]"]
  need resultVar
  resultVar' <- fmtVar resultVar
  
  -- Store resultVar in arrVar[loopVar].
  -- 3 Things can happen here:
  resultSlice <- maybeSlice resultVar
  let sliceAssign = c $ [arrVar',"[",loopVar',"] = ",resultVar',";"]
  case (arrSlice, resultSlice) of
    -- If result is a slice: omit assignment.  Element-wise assignment
    -- has already happened inside the inner loop.
    (_,Just _) -> do
      emitC $ ["// omit assignment: ", sliceAssign]
    -- If arr is a slice and result is not a slice: this is scalar
    -- inner loop assigment.  We perform recursive slice substitution
    -- to find the storage cell.
    (Just _,_) -> do
      slice' <- fmtSlice arrVar
      emitC $ ["// use ",arrVar'," == ", slice', " to implement ", sliceAssign]
      emitC $ [slice', "[",loopVar',"] = ",resultVar',";"]
    -- Otherwise: normal scalar to array cell assignment.
    (_,_) ->
      emitC [sliceAssign]
      
  -- Restore variable environment and loop nesting after leaving the
  -- loop.  Variables that were declared inside the loop are no longer
  -- visible.
  variables .= snapVariables
  loopVars  .= snapLoopVars
  emitC ["}"]

emit _ (Node _ (Var _)) = return () -- stub

emit elVar (Node elType (Ref aVar iVar)) = do
  need aVar ; need iVar
  -- FIXME: Array ref is wrong because it needs to be multi-dimensional.
  -- (outVarDecl',_) <- fmtVarDecl t outVar
  emitC [ "// TODO Ref" ]

emit _ _ = do
  emitC [ "// TODO toC match" ]




maybeSlice var = do
  aliases' <- use aliases
  return $ lookup var aliases'
