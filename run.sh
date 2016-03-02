#!/bin/sh

make

#./server PORTNO CWnd loss_prob corr_prob
./server 8003 &
#./receiver sender_hostname sender_portnumber filename loss_prob corr_prob
./receiver localhost 8003
