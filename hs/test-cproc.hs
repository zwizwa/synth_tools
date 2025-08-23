-- Idea here is to port the Lua code used in Areal project.

-- There are a couple of new ideas in that code that should probably
-- be formalized a bit.  Build it top down: first make some test code
-- that can run the original Lua-compiled code.

-- I don't really understand all the dependencies yet between the
-- different layers, but from the Lua code emerges a clear structure
-- that should be easy enough to decouple using some Haskell ADTs.

import qualified SynthTools.RunC as RunC


test_armv7 = "/i/exo/areal/src/armv7-nix/test_armv7.elf"

main = do
  putStrLn "test_cproc.hs"
  f <- RunC.run test_armv7 ["test_hs"] 123
  putStrLn $ show f
  
  
