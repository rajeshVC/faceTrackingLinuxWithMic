#! /bin/bash

#find cpu stats when thread num is 1
#if detecting nth frame out of all

for NUMTHREADS in {1..4..2}
do 
	for NTHFRAME in [1, 5, 10, 20, 30, 60]
	do
		./FaceDetection NTH_FRAME NUMTHREADS
		echo "Number of threads: $(NUMTHREADS). Nth Frame: $(NTHFRAME)" >> cpustats.txt
		pidstat 60 -ru -p $(pidof FaceDetection) >> cpustats.txt
		
		kill $(pidof FaceDetection)
	done
	
done
