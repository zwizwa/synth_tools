{-# LANGUAGE OverloadedStrings #-}



module SynthTools.RunC where

-- import System.Process.Typed
import System.Process
import Data.Binary
import Data.Binary.Put
import Data.Binary.Get
import System.IO
import qualified Data.ByteString.Lazy as L
--import qualified Data.ByteString as B
import Control.Exception (bracket)

-- Take C code and turn it into a function.
class RunC r where
  runC :: String -> r a -> IO (r b)

-- There are too many parameters that are not yet clear so start from
-- something really simple: the C program takes an infinite stream on
-- stdin and produces an infinite stream on stdout.  The base type is
-- the parameter, e.g. (Float,Float)

-- In the end I want remote execution, i.e. I want to run the test
-- framework on the dev host and run the code under test on a remote
-- device accessible over network (no shared memory).

-- It might be simplest to split it in 2 parts: create a C<->Haskell
-- interface and then write the remote glue in C.

-- Maybe it's good to just keep it simple.  Write the data to files
-- and use text format.


-- Simplify: there is already an API to run the process code for
-- Octave.  Just reuse that.

-- From Claude

-- Method 1: Using Data.Binary (recommended)
run :: String -> [String] -> Float -> IO Float
run cmd args inputFloat = do
  (Just stdin_h, Just stdout_h, Just stderr_h, proc_h) <- 
    createProcess (proc cmd args) { 
      std_in = CreatePipe,
      std_out = CreatePipe, 
      std_err = CreatePipe
    }
  
  -- Set handles to binary mode
  hSetBinaryMode stdin_h True
  hSetBinaryMode stdout_h True
  
  -- Write float as 4-byte little-endian binary
  let inputBytes = runPut (putFloatle inputFloat)
  L.hPut stdin_h inputBytes
  hFlush stdin_h
  -- hClose stdin_h  -- Important: close stdin so process knows input is complete
  
  -- Read float back as 4-byte little-endian binary
  outputBytes <- L.hGet stdout_h 4
  let outputFloat = runGet getFloatle outputBytes
  
  -- Clean up
  hClose stdout_h
  hClose stderr_h
  _ <- waitForProcess proc_h
  
  return outputFloat

