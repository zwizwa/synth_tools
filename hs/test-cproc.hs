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
import GHC.Int

import qualified SynthTools.Dataflow as Dataflow
import SynthTools.FFT
import SynthTools.Num


import SynthTools.IO

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
  m <- RunC.runInt32 elf ["ntt"] input
  traverse (putStrLn . show) $ m
  
  return ()

-- Most natural form of the ntt operation exposed by the process
-- running in the backtround: lifted by `run` and operating on the
-- same data representation as SynthTools.NTT functions.
type NTTIO = [F3] -> PropertyM IO [F3]

-- The test_fft.c binary can support both forward and reverse ops.
-- Both are presented as NTTIO functions.
data Ops = Ops { fftOp :: NTTIO, ifftOp :: NTTIO }




prop_eq :: Ops -> V256 F3 -> Property
prop_eq (Ops ntt intt) (V256 probe) = monadicIO $ do
  let eq ref_op io_op = do
        o <- io_op probe
        let o' = ref_op probe
        assert (o' == o)
  eq fft  ntt
  eq ifft intt


qc_nttIO = do
  -- Create a single process to compute the NTTs in the test.
  (nttIO', nttClose) <- RunC.int32Runner test_fft_elf ["ntt"]
  let nttIO hdr i = do
        --putStrLn' $ "i: " ++ (show $ i)
        o_raw <- nttIO' hdr $ map unF3 i
        let o = map F3 o_raw
        --let o' = fft i
        --putStrLn' $ "o: " ++ (show $ o)
        --putStrLn' $ "o': " ++ (show $ o')
        return $ o

  -- The Ops bundle is set up so it can be used directly inside the
  -- property monad, i.e. already lifted by run.  The Int32 header
  -- serves as a command to the C code, see test_fft.c
  let ops = Ops (run . (nttIO [0x100])) -- fft
                (run . (nttIO [0x101])) -- ifft
  quickCheck (prop_eq ops)

  nttClose
  return ()

main = do
  -- Dataflow.test
  qc_nttIO
  


-- NEXT:
-- . generate FFT routine
-- . create quickcheck for that
