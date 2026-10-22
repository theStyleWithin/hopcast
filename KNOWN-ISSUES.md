# Known issues and open observations

Things that have been seen but not explained yet. Nothing here has a fix in progress unless it says so.

## Wi-Fi dropped during the first multi-stream test (Steam Deck, Linux)

- **Seen:** after the first test streaming to four destinations at once, the Wi-Fi connection kept cycling
  "activated / disconnected / activated". It stopped only after HopCast was closed. The connection was fine
  when checked later.
- **Not known:** whether this comes from HopCast, from the upload load of four simultaneous streams, or from
  the Deck's Wi-Fi adapter. It hasn't been reproduced yet.
- **Plan (decided by the developer):** run many streaming tests to single platforms, one at a time, to see
  whether it can happen without multi-streaming and how stable each one is. No patch is attempted before that.
- **If it happens to you:** please report the version, how many destinations you were sending to, the bitrate,
  and the output of `sudo dmesg | tail -50` taken right after.
