
module SynthTools.TestCproc where

-- The point of this module is to:
-- . expose QuickCheck IO monad functions
-- . interface with external binary using RunC
-- . TODO: test generated cproc code (now only using manual test_fft.c)



-- Old comments:

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
import Data.IORef
import Control.Monad
import GHC.Int
import Debug.Trace

import qualified SynthTools.Dataflow as Dataflow
import SynthTools.FFT
import SynthTools.Num


import SynthTools.IO
import SynthTools.Misc

import Test.QuickCheck
import Test.QuickCheck.Monadic
import System.Command




test_fft_elf = "linux/test_fft.dynamic.host.elf"

test_common = do
  let elf = test_fft_elf
  -- Note: test-cproc.sh will cd to synth_tools
  -- -- setCurrentDirectory "/i/exo/synth_tools"
  
  -- 1. Generate the header
  writeFile "generic/ns_fft_gen.h" "// FIXME\n"

  -- 2. Compile the code
  command_ [] "./make.sh" [elf]
  return elf

test_fft = do
  elf <- test_common
  
  -- 3. Run the code with i/o
  let input = fmap fromIntegral [0..2*256-1]
  m <- RunC.runFloat elf ["fft"] input
  traverse (putStrLn . show) $ m
  
  return ()

test_ntt = do
  elf <- test_common
  
  -- 3. Run the code with i/o
  let input = fmap fromIntegral [0..256-1]
  m <- RunC.runInt32 elf ["ntt8"] input
  traverse (putStrLn . show) $ m
  
  return ()

-- Most natural form of the ntt operation exposed by the process
-- running in the backtround: lifted by `run` and operating on the
-- same data representation as SynthTools.NTT functions.
type NTTIO = [F3] -> PropertyM IO [F3]

-- The test_fft.c binary can support both forward and reverse ops.
-- Both are presented as NTTIO functions.
data Ops t = Ops {
  fftOp  :: t,
  ifftOp :: t
  }



-- Compare test_fft.c implementation (ns_fft.h) to SynthTools.FFT
-- Haskell implementation.
prop_fft :: Ops NTTIO -> V256 F3 -> Property
prop_fft ops (V256 probe) = monadicIO $ do
  let fft'  = fftOp  ops
      ifft' = ifftOp ops
      eq ref_op io_op = do
        o <- io_op probe
        let o' = ref_op probe
        assert (o' == o)
  eq fft  fft'
  eq ifft ifft'

-- prop_ols :: Ops NTTIO -> V256 F3 -> Property
-- prop_ols ops (V256 probe) = monadicIO $ do
--   let ols = olsOp ops
--       eq ref_op io_op = do
--         o <- io_op probe
--         let o' = ref_op probe
--         assert (o' == o)
--   eq fft  ntt
--   eq ifft intt

-- Note that the ola function in test_fft_elf is not stateless, so it
-- needs to be wrapped as a sequence like:
-- . opload coefficients and reset state
-- . run a number of frames




run_test_fft_elf' tag = do
  -- Create a single process to compute the NTTs in the test.
  service@(nttIO', nttClose) <- RunC.int32Runner test_fft_elf [tag]
  -- Get the logn of the instantiated service.
  [logn] <- nttIO' [0x104] []
  -- Verify size and run the corrsponding test.
  case tag of
    "ntt4" -> do
      let True = 4 == logn
      test_nttIO_16 service
    "ntt8" -> do
      let True = 8 == logn
      test_nttIO_256 service

run_test_fft_elf = do
  run_test_fft_elf' "ntt8"
  run_test_fft_elf' "ntt4"

test_nttIO_16 (nttIO', nttClose) = do
  putStrLn' "test_nttIO_16"

  let nttIO hdr i = do
        --putStrLn' $ "i: " ++ (show $ i)
        o_raw <- nttIO' hdr $ map unF2 i
        let o = map F2 o_raw
        --let o' = fft i
        --putStrLn' $ "o: " ++ (show $ o)
        --putStrLn' $ "o': " ++ (show $ o')
        return $ o

      op' id = nttIO [id]
      op  id = run . (op' id)
      
      ols_tick = op' 0x102
      ols_init = op' 0x103

      ols = do
        let init1 = ([9,3,1] ++ (take 20 $ cycle [1,2]))
            init2 = [1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13]
            init  = [1,1,2,2,3,3,4,4, 5,5,6,6,7,7,8,8]
        ols_init init
        traverse' [1,0,0] $ \i -> do
          ols' <- ols_tick $ shiftedImpulse 8 i 0
          putStrLn' $ show $ ols'

      ols_impulse t = do
        let nb_blocks = 3
            bs = 8
            frames = shiftedImpulse (nb_blocks * bs) 1 t
            frames_list = chunksOf bs frames
        ols_init [1,2,3,4,5,6,7,8,9,10,11,12]
        out <- traverse ols_tick frames_list
        -- putStrLn' $ show out
        return out
        
        
      ols_impulse' = do
        putStrLn' "ols_impulse'"
        log <- traverse ols_impulse [0 .. 15]
        putStrLn' "ols_impulse':log"
        traverse (putStrLn' . show) log
        return ()

      


  -- 1. I have a reference test in SynthTools.FFT that can compute the
  --    output of a circular convolution with an impulse in any of the
  --    16 positions, where 0-8 is past, 9 is current, and 10-16 are
  --    future.
  --
  -- 2. Simulate that input by sending 2 frames.  Instrument the C
  --    code to print the contents of the buffer.
  
  ols_impulse'

  
  -- ols_impulse 15
  

  return ()

test_nttIO_256 (nttIO', nttClose) = do
  putStrLn' "test_nttIO_256"
  
  let nttIO hdr i = do
        --putStrLn' $ "i: " ++ (show $ i)
        o_raw <- nttIO' hdr $ map unF3 i
        let o = map F3 o_raw
        --let o' = fft i
        --putStrLn' $ "o: " ++ (show $ o)
        --putStrLn' $ "o': " ++ (show $ o')
        return $ o

      op' id = nttIO [id]
      op  id = run . (op' id)
      
      ols_tick = op' 0x102
      ols_init = op' 0x103
      

  -- Note that the _stateless_ op for ols testing is composed of
  -- stateful ops.

  -- The Ops bundle is set up so it can be used directly inside the
  -- property monad, i.e. already lifted by run.  The Int32 header
  -- serves as a command to the C code, see test_fft.c
  let ops = Ops (op 0x100)  -- fft
                (op 0x101)  -- ifft

      qc prop = quickCheck (prop ops)

  -- Run a number of tests
  qc prop_fft

  -- Clean up the service process when done.
  nttClose
  return ()

  


-- NEXT:
-- . generate FFT routine
-- . create quickcheck for that
