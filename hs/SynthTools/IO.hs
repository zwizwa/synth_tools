
module SynthTools.IO where
  
import GHC.IO.Handle
import GHC.IO.StdHandles
import System.Environment

-- All logging goes to stderr.  The stdout is used for plugin i/o.
putStrLn' str = putStr' $ str ++ "\n"
putStr'   str = hPutStr stderr $ str


traverse' = flip traverse


-- Trampoline binary entry point.
trampoline' tag tests tests' = do
  args <- getArgs
  -- putStrLn' $ tag ++ (show args)

  let doRun name run = do
        putStrLn' $ "\n" ++ tag ++ name
        run

  case args of
    [] -> do
      putStrLn' $ (tag ++ "choose a test or 'all':")
      putStrLn' $ (tag ++ show (map fst (tests ++ tests')))
      
    ["all"] -> do
      -- Run all execpt those in tests'
      traverse' tests $ \(name, run) -> doRun name run
      return ()
    [name] ->
      case Prelude.lookup name (tests ++ tests') of
        Nothing  -> putStrLn' $ tag ++ "unknown test: " ++ name
        Just run -> doRun name run
    args ->
      putStrLn' $ tag ++ "invalid args: " ++ show args
  return ()

trampoline tag tests = trampoline' tag tests []

