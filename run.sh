#!/bin/sh

g++ server.cpp -o server
#g++ receiver.cpp -o receiver

#./server PORTNO CWnd loss_prob corr_prob
./server 8000 
#./receiver sender_hostname sender_portnumber filename loss_prob corr_prob
#./receiver
