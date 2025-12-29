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
-- import qualified SynthTools.NTT as NTT
-- import qualified SynthTools.Num as Num
import SynthTools.NTT
import SynthTools.Num


import SynthTools.IO

import Test.QuickCheck
import Test.QuickCheck.Monadic
import System.Command


ir_areal = do
  setCurrentDirectory "/i/exo/areal/src/"
  putStrLn "test_cproc.hs"
  m <- RunC.runPlugin12 "./armv7-nix/test_armv7.elf" ["ir_areal"]
  let (m':_) = transpose m
  traverse (putStrLn . show) $ m'




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

-- Most natural form of the ntt operation: lifted by `run` and
-- operating on the same data as ifft.
type NTTIO = [F3] -> PropertyM IO [F3]

data Ops = Ops { fftOp :: NTTIO, ifftOp :: NTTIO }

prop_fft :: Ops -> VecDFT F3 -> Property
prop_fft (Ops ntt intt) (VecDFT i) = monadicIO $ do
  o <- ntt i
  let o' = fft $ i
  assert (o' == o)

prop_ifft :: Ops -> VecDFT F3 -> Property
prop_ifft (Ops ntt intt) (VecDFT i) = monadicIO $ do
  o <- intt i
  let o' = ifft $ i
  assert (o' == o)

qc_nttIO = do
  -- Create a single process to compute the NTTs in the test.
  nttIO' <- RunC.int32Runner test_fft_elf ["ntt"]
  let nttIO hdr i = do
        --putStrLn' $ "i: " ++ (show $ i)
        o_raw <- nttIO' hdr $ map unF3 i
        let o = map F3 o_raw
        --let o' = fft i
        --putStrLn' $ "o: " ++ (show $ o)
        --putStrLn' $ "o': " ++ (show $ o')
        return $ o

  -- Create a property bound to the reference.
  let ops = Ops (run . (nttIO [0x100])) -- fft
                (run . (nttIO [0x101])) -- ifft
  quickCheck (prop_fft ops)
  quickCheck (prop_ifft ops)

  nttIO [] [] -- close
  return ()

main = do
  -- ir_areal
  -- Dataflow.test
  qc_nttIO
  


-- NEXT:
-- . generate FFT routine
-- . create quickcheck for that
