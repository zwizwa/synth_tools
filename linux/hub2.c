/* Redesign & simplication of original jack midi hub.

   Basic idea is that using a2jmidid and hub doesn't really make a
   whole lot of sense.  Most code comes from managing the distributed
   nature of the setup which has been a huge pain from the start.

   So turn it into a monolith.

   This means:

   - don't use jack when it's not needed: replace jack midi ports
     communication across applications with alsa midi ports tied into
     the same binary.

   - abstract away the substrate: make it so that midi processors can
     run on a different substrate, e.g. microcontroller or jack or
     alsa or inside a pd object or tied to a network socket

   - replace dispatch "case" with function implementation, replace
     send with function call where possible.  processors should be
     written in a way that makes early binding possible

   - manage connections while leaving the possibility to reload code,
     e.g. allow partial restart of a plugin (e.g. double-buffered .so)

   - midi should only be a specialization: try to keep handlers and
     senders generic.

   - only when monolith works, then start distribution by tying one or
     more monoliths together over network.

   - stick to C ABI as base language for processor plugins.  this way
     they can be written in any language

   - write the hub in Rust.  the idea is to keep it simple but of
     course that is not going to happen.  the hub is going to end up
     managing threads and implementing a "bios" for the plugins so
     let's pick a good implementation language.

*/
