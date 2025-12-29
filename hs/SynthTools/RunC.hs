{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

-- General idea: a collection of ad-hoc binary protocols to run over
-- stdio.  These are "no design" protocols: just make something that
-- is easy to bulk read/write at the C end.


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

import SynthTools.IO

import Prelude hiding (putStr, putStrLn)

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

-- Started from Claude examples.


-- 1. Everything needed to run the ad-hoc areal 12-channel API

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

  writeMatrix stdin_h rows columns $ concat config_matrix ++ signal
      

  outputFloat <- readMatrix stdout_h
  
  
  close p
  return $ chunksOf 12 outputFloat



-- The plugin API is one float matrix in, one float matrix out.  This
-- is pretty raw but up to now it has worked just fine.
bePlugin :: IO ()
bePlugin = do
  cfg <- readMatrix stdin
  putStrLn' $ show cfg
  writeMatrix stdout 2 2 [1,2,3,4]

  

runGet' = flip runGet


  

writeMatrix handle rows columns floats = do
  let bytes = runPut $ do
        putInt32le rows
        putInt32le columns
        traverse putFloatle floats
        return ()
  L.hPut handle bytes
  hFlush handle

readMatrix :: Handle -> IO [Float]
readMatrix handle = do
  header <- L.hGet handle 8
  let [r,c] = runGet' header $ replicateM 2 getInt32le
      nbFloat' = fromIntegral $ r * c
  
  floats <- L.hGet handle $ 4 * nbFloat'
  let outputFloat = runGet' floats $ replicateM nbFloat' getFloatle
  -- putStrLn' $ show outputFloat
  return outputFloat


run' cmd args = do
  (Nothing, Nothing, Nothing, proc_h) <- 
    createProcess (proc cmd args)
  return $ proc_h

close' proc_h = do
  waitForProcess proc_h



-- 2. Additional routines for simpler raw formats to run individual
-- processors.

runWither (word_size, put, get, cmd, args) = do
  
  p@(Proc proc_h stdin_h stdout_h) <- open cmd args

  let read nb = do
        bytes <- L.hGet stdout_h $ word_size * nb
        let output = runGet' bytes $ replicateM nb get
        return output

      write hdr words = do
        let bytes = runPut $ do
              traverse putInt32le hdr
              traverse put words
              return ()
        L.hPut stdin_h bytes
        hFlush stdin_h

      tick [] [] = do
        close p
        return []
        
      tick hdr input = do
        write hdr input
        output <- read (length input)
        return $ output

  return tick

runWith cfg input = do
  tick <- runWither cfg
  let hdr = []
  output <- tick hdr input
  tick [] [] -- close
  return output

runFloat c a = runWith (4, putFloatle, getFloatle, c, a)
runInt32 c a = runWith (4, putInt32le, getInt32le, c, a)

floatRunner c a = runWither (4, putFloatle, getFloatle, c, a)
int32Runner c a = runWither (4, putInt32le, getInt32le, c, a)

-- readInt321 :: Handle -> Int -> IO [Int]
-- readInt321 handle nb = do
--   floats <- L.hGet handle $ 4 * nb
--   let output = runGet' floats $ replicateM nb getInt32le
--   return $ fromIntegral output

-- writeInt321 handle ints = do
--   let bytes = runPut $ do
--         traverse putInt32le $ map fromIntegral ints
--         return ()
--   L.hPut handle bytes
--   hFlush handle

-- -- Run a single channel raw processor (see test_fft.c)
-- runInt321 :: String -> [String] -> [Int] -> IO [Int]
-- runInt321 cmd args inputInt32 = do
--   p@(Proc proc_h stdin_h stdout_h) <- open cmd args
--   writeInt321 stdin_h inputInt32
--   outputInt32 <- readInt321 stdout_h (length inputInt32)
--   close p
--   return $ outputInt32



