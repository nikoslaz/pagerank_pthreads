# PageRank with Pthreads

PageRank over real-world graphs in C with POSIX threads.
Assignment 1 for HY-342 (Parallel Programming), University of Crete.

The program reads an edge list, builds an adjacency-list graph and runs 50 PageRank iterations
(damping factor 0.85), splitting the work between threads. The ranks are written to
`pagerank.csv`.

## Files

| File | Description |
|---|---|
| `main.c` | PageRank implementation |
| `Email-Enron.txt` | Enron email network (36,692 nodes, 367,662 edges) |
| `p2p-Gnutella24.txt` | Gnutella peer-to-peer network (26,518 nodes, 65,369 edges) |
| `facebook/` | Facebook ego networks (`*.edges` are the edge lists) |
| `Report.pdf` | Report with timings for 1–4 threads (in Greek) |

The datasets come from the [Stanford SNAP collection](https://snap.stanford.edu/data/).

## Build & run

```bash
gcc -O2 -pthread main.c -o pagerank
./pagerank
```

The input file (`Email-Enron.txt`) and the number of threads (`NUM_THREADS`, default 2) are set
at the top of `main.c`.

## Output

`pagerank.csv` has a header line `node,pagerank` followed by one `node,rank` line per node.
