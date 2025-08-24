{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE NoMonomorphismRestriction #-}



module SynthTools.RunC where

import Control.Monad
import System.Process
import Data.List.Split
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

data Proc = Proc ProcessHandle Handle Handle 

open :: String -> [String] -> IO Proc
open cmd args = do
  (Just stdin_h, Just stdout_h, Nothing, proc_h) <- 
    createProcess (proc cmd args) { 
      std_in  = CreatePipe,
      std_out = CreatePipe
      -- std_err = CreatePipe
    }
  hSetBinaryMode stdin_h  True
  hSetBinaryMode stdout_h True
  return $ Proc proc_h stdin_h stdout_h

close (Proc proc_h stdin_h stdout_h) = do
  hClose stdin_h
  hClose stdout_h
  waitForProcess proc_h


-- Method 1: Using Data.Binary (recommended)
runPlugin12 :: String -> [String] -> IO [[Float]]
runPlugin12 cmd args = do
  p@(Proc proc_h stdin_h stdout_h) <- open cmd args

  let
    rows = 2
    columns = 12 -- AREAL_NB_IN_CHANNELS
    nb_blocks = 1
    block_size = 256
    preset = 3
    use_float = 1
    monitor_L = 0
    monitor_R = 0
    noise = 0
    tc = 0
    impulse = [1,0,0,0,0,0,0,0,0,0,0,0]
    config_matrix = [
      [nb_blocks, block_size, preset, use_float,
       monitor_L, monitor_R,  noise,  tc,
       0,         0,          0,      0],
      impulse
      ]
    nbFloat = nb_blocks * block_size * columns
    signal = replicate nbFloat 0
    inputBytes = runPut $ do
      putInt32le rows
      putInt32le columns
      traverse putFloatle $ concat config_matrix ++ signal
      return ()
      
  L.hPut stdin_h inputBytes
  hFlush stdin_h

  let runGet' = flip runGet

  header <- L.hGet stdout_h 8
  let [r,c] = runGet' header $ replicateM 2 getInt32le
      nbFloat' = fromIntegral $ r * c
  
  floats <- L.hGet stdout_h $ 4 * nbFloat'
  let outputFloat = runGet' floats $ replicateM nbFloat' getFloatle
  
  close p
  return $ chunksOf 12 outputFloat




run' cmd args = do
  (Nothing, Nothing, Nothing, proc_h) <- 
    createProcess (proc cmd args)
  return $ proc_h

close' proc_h = do
  waitForProcess proc_h
