# Run Log

First, I built and tested the baseline code on Profile A with 40ms delay.

```text
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ make
cc -O2 -Wall -o sender sender.c
cc -O2 -Wall -o receiver receiver.c
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/A.json --delay_ms 40
endpoints done
relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 77  (5.13%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
```

It failed with a lot of misses (5.13%). I ran it again with a huge 200ms delay to isolate the packet drops from the jitter:

```text
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/A.json --delay_ms 200
endpoints done
relay done: {'up_bytes': 246000, 'down_bytes': 0, 'up_pkts': 1500, 'down_pkts': 0, 'dropped': 34, 'duplicated': 10}
================ SCORE ================
  frames               : 1500
  deadline misses      : 34  (2.27%)   [cap 1.00%]
  playout delay        : 200 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.02x   [cap 2.00x]   (up 246000B, feedback 0B)
  RESULT               : INVALID
```

The misses dropped to 2.27%, which perfectly matches the dropped packet count (34). So the rest of the misses from the first run were because of network jitter delaying the packets past the 40ms deadline.

I rewrote `sender.c` and `receiver.c` to use XOR FEC. Every pair of data packets gets a parity packet sent right after. The receiver forwards immediately and recovers drops instantly without a timer.

Testing the new code with 80ms delay:

```text
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ make clean && make
rm -f sender receiver
cc -O2 -Wall -o sender sender.c
cc -O2 -Wall -o receiver receiver.c
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/A.json --delay_ms 80
endpoints done
relay done: {'up_bytes': 369000, 'down_bytes': 0, 'up_pkts': 2250, 'down_pkts': 0, 'dropped': 52, 'duplicated': 14}
================ SCORE ================
  frames               : 1500
  deadline misses      : 3  (0.20%)   [cap 1.00%]
  playout delay        : 80 ms   <-- your score if valid; lower wins
  bandwidth overhead   : 1.54x   [cap 2.00x]   (up 369000B, feedback 0B)
  RESULT               : VALID
```

It works perfectly, misses dropped to 0.20%, and overhead is 1.54x (well under the 2.0x limit).

Now, trying to minimize the delay on Profile A:

```text
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/A.json --delay_ms 60
...
  deadline misses      : 4  (0.27%)   [cap 1.00%]
  playout delay        : 60 ms   <-- your score if valid; lower wins
  RESULT               : VALID

((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/A.json --delay_ms 40
...
  deadline misses      : 42  (2.80%)   [cap 1.00%]
  playout delay        : 40 ms   <-- your score if valid; lower wins
  RESULT               : INVALID
```

60ms still works (0.27% misses). But 40ms fails (2.80% misses) because the parity packets are arriving too late to recover the lost frames.

Finally, testing on Profile B (the harder network) to find the final delay to submit:

```text
((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/B.json --delay_ms 120
...
  deadline misses      : 12  (0.80%)   [cap 1.00%]
  playout delay        : 120 ms   <-- your score if valid; lower wins
  RESULT               : VALID

((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/B.json --delay_ms 100
...
  deadline misses      : 12  (0.80%)   [cap 1.00%]
  playout delay        : 100 ms   <-- your score if valid; lower wins
  RESULT               : VALID

((env) ) sm829@Lenovo:/mnt/c/Users/sm829/Downloads/systems_handout/systems_handout$ python3 run.py --profile profiles/B.json --delay_ms 80
...
  deadline misses      : 35  (2.33%)   [cap 1.00%]
  playout delay        : 80 ms   <-- your score if valid; lower wins
  RESULT               : INVALID
```

At 120ms and 100ms it is valid (0.80% misses). But at 80ms it breaks (2.33% misses) because Profile B has up to 80ms jitter.

So the final delay we need to use is **100ms**.