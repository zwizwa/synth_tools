-- Idea here is to port the Lua code used in Areal project.

-- There are a couple of new ideas in that code that should probably
-- be formalized a bit.  Build it top down: first make some test code
-- that can run the original Lua-compiled code.

-- I don't really understand all the dependencies yet between the
-- different layers, but from the Lua code emerges a clear structure
-- that should be easy enough to decouple using some Haskell ADTs.

import qualified SynthTools.RunC as RunC
import System.Directory (setCurrentDirectory)
import Data.List.Split
import Data.List

import qualified SynthTools.Dataflow as Dataflow

ir_areal = do
  setCurrentDirectory "/i/exo/areal/src/"
  putStrLn "test_cproc.hs"
  m <- RunC.runPlugin12 "./armv7-nix/test_armv7.elf" ["ir_areal"]
  let (m':_) = transpose m
  traverse (putStrLn . show) $ m'

main = do
  -- ir_areal
  Dataflow.test


