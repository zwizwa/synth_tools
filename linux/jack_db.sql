-- Map port name to connections.
CREATE VIEW connections AS
SELECT 
src_client||':'||src_port AS src,
dst_client||':'||dst_port AS dst
FROM snapshot;

-- When an output is created:
-- SELECT src FROM connections WHERE dst = 'system:playback_1';

-- When an input is created:
-- SELECT dst FROM connections WHERE src = 'jack_synth:audio_out_0';

