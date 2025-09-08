
module SynthTools.IO where
  
import GHC.IO.Handle
import GHC.IO.StdHandles

-- All logging goes to stderr.  The stdout is used for plugin i/o.
putStrLn' str = putStr' $ str ++ "\n"
putStr'   str = hPutStr stderr $ str
