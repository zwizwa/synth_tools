{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE NoMonomorphismRestriction #-}

-- General idea: a collection of ad-hoc binary protocols to run over
-- stdio.  These are "no design" protocols: just make something that
-- is easy to bulk read/write at the C end.


module SynthTools.RunC where

import Control.Monad
import Control.Exception
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


-- 1. Everything needed to run the 12-channel API used for the Areal
--    test binary test_armv7.elf -- This uses a float matrix to
--    configure the impulse response which makes it a bit easier to
--    interface from octave.  Since 2025/12 an extra message handler
--    has been added to further customize the emulation.  Note that
--    this is not an i/o processor: the input is always the impulse
--    simulation built into test_armv7.c

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


-- The configuration is awkward.  It is kept for backwards
-- compatibility with the octave code, but it seems that wherever
-- possible the OSC commands should be used instead to make everything
-- more uniform.

runPlugin12 :: (String, [String]) -> [String] -> Int -> IO [[Float]]
runPlugin12 (cmd, args) presets nb_blocks' = do
  p@(Proc proc_h stdin_h stdout_h) <- open cmd args

  let
    rows = 2
    columns = 12 -- AREAL_NB_IN_CHANNELS
    block_size = 256
    nb_blocks = fromIntegral nb_blocks'
    preset = 3
    use_float = 1  -- Tests only run the full integer plugin with output stage.
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

  -- Additional test binary config
  traverse (writePreset stdin_h) presets

  -- Write the old 2 x 12 config matrix, also used by octave.
  writeMatrix stdin_h rows columns $ concat config_matrix
      

  outputMatrix <- readMatrix stdout_h
  
  close p
  return $ outputMatrix



-- The plugin API is one float matrix in, one float matrix out.  This
-- is pretty raw but up to now it has worked just fine.
bePlugin :: IO ()
bePlugin = do
  cfg <- readMatrix stdin
  putStrLn' $ show cfg
  writeMatrix stdout 2 2 [1,2,3,4]

  

runGet' = flip runGet

-- 2025/12 switched to uc_tools packet framing by default
uc_tools_framing = True

writePreset handle preset = do
  let tag32 = 0xA1000001
      bytes = runPut $ do
        when uc_tools_framing $ do
          putInt32be $ 4 + fromIntegral (L.length data_bytes)
          putInt32be tag32
        return ()
      data_bytes = runPut $ do
        putStringUtf8 preset
        return ()

  L.hPut handle bytes
  L.hPut handle data_bytes


writeMatrix handle rows columns floats = do
  -- putStrLn' $ show (rows,columns,floats)
  let len = fromIntegral $ 4 * (1 + 2 + length floats)
      bytes = runPut $ do
        when uc_tools_framing $ do
          putInt32be len
          putInt32be 0x1EEE0001
        putInt32le rows
        putInt32le columns
        traverse putFloatle floats
        return ()
  L.hPut handle bytes
  hFlush handle

readMatrix :: Handle -> IO [[Float]]
readMatrix handle = do
  when uc_tools_framing $ do
    uct_header <- L.hGet handle 8
    let uct_hdr@[uct_len, uct_tag] = runGet' uct_header $ replicateM 2 getInt32be
    -- putStrLn' $ show uct_hdr
    assert (uct_tag == 0x1EEE0001) $ return ()
  
  matrix_header <- L.hGet handle 8
  let [r,c] = runGet' matrix_header $ replicateM 2 getInt32le
      nbFloat' = fromIntegral $ r * c

  --putStrLn' $ show ("readMatrix",[r,c])
  
  floats_bin <- L.hGet handle $ 4 * nbFloat'
  let outputFloat = runGet' floats_bin $ replicateM nbFloat' getFloatle
  -- putStrLn' $ show ("size", length outputFloat)

  return $ chunksOf (fromIntegral c) outputFloat


run' cmd args = do
  (Nothing, Nothing, Nothing, proc_h) <- 
    createProcess (proc cmd args)
  return $ proc_h

close' proc_h = do
  waitForProcess proc_h



-- 2. Additional routines for simpler raw formats to run individual
-- processors.  These are made compatible with the uc_tools tagged
-- protocol wrapped in {packet,4} framing.

runRaw (word_size, put, get, cmd, args) = do
  
  p@(Proc proc_h stdin_h stdout_h) <- open cmd args

  let read nb = do
        -- size, uc_tools compatible tagging (ignored for now)
        let nb_wrap_bytes = 8
        header_bytes <- L.hGet stdout_h nb_wrap_bytes
        -- FIXME: Check what is returned.
        bytes <- L.hGet stdout_h $ word_size * nb
        let output = runGet' bytes $ replicateM nb get
        return output

      write hdr words = do
        let n = fromIntegral $ length words
            wrapHdr = [
              -- Protocol is kept compatible with uc_tools format with
              -- {packet,4} big endian 32 bit size tag for framing.
              -- Areal tag space is reserved as A1xx in uc_tools/packet_tags.h
              -- We have a 16 bit subtag to extend that.
              4 * (2 + n),
              0x1EEE0000
              ]
            bytes = runPut $ do
              traverse putInt32be $ wrapHdr
              traverse putInt32le $ hdr
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

runRawWith cfg input = do
  tick <- runRaw cfg
  let hdr = []
  output <- tick hdr input
  tick [] [] -- close
  return output

runFloat c a = runRawWith (4, putFloatle, getFloatle, c, a)
runInt32 c a = runRawWith (4, putInt32le, getInt32le, c, a)

floatRunner c a = runRaw (4, putFloatle, getFloatle, c, a)
int32Runner c a = runRaw (4, putInt32le, getInt32le, c, a)

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



