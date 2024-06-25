#!/bin/bash

# Array of numbers to be checked by the server for primality
numbers=("2" "3" "4" "11" "22" "35" "37" "96" "100")

# Launch clients in the background
for num in "${numbers[@]}"; do
   ./client3 "$num" &
done

# Wait for all background jobs to finish
wait
echo "All clients have finished."
