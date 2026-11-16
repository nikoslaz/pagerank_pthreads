# PageRank with Pthreads

PageRank over real-world graphs in C with POSIX threads.
Assignment 1 for HY-342 (Parallel Programming), University of Crete.

The program reads an edge list and stores the graph by incoming edges (CSR). It then runs 50
iterations of

```
PR(v) = (1 - d) + d * Σ PR(u) / outdegree(u)    over all u → v,   d = 0.85
```

The nodes are split evenly between the threads. In every iteration each thread updates its own
nodes, reading the previous iteration's ranks. The threads then meet at a barrier before the next
iteration starts. The ranks are written to `pagerank.csv`.

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
./pagerank [edge_list_file] [threads]

./pagerank Email-Enron.txt 4
./pagerank p2p-Gnutella24.txt 2
./pagerank facebook/0.edges 4
```

Defaults: `Email-Enron.txt` and 2 threads. The program prints the run time of the PageRank
iterations.

## Note

The timings in the report were measured with an earlier version of the code.

## Output

`pagerank.csv` has a header line `node,pagerank` followed by one `node,rank` line per node.
