My design uses Forward Error Correction (FEC) instead of retransmissions since the playout deadline is too tight for round-trip requests. The sender calculates an XOR parity packet for every two consecutive frames and transmits it, using a fixed 1.5x bandwidth overhead. To identify parity packets without adding extra bytes, the sender sets the most significant bit (MSB) of the 32-bit sequence number to 1. 

The receiver does not use a timer-based jitter buffer, as the test harness already accepts packets early. Instead, it acts completely event-driven, forwarding data packets immediately upon arrival. If a frame is dropped, the receiver instantly recovers the missing payload using the parity XOR logic as soon as the required pieces arrive.

Please grade at `--delay_ms 100`. This covers the 80ms maximum jitter of Profile B plus the 20ms offset between a frame and its parity packet.

This design will fail if the network drops two consecutive packets in the same pair, or if network jitter delays the parity packet by more than 100ms causing it to miss the playout deadline.
