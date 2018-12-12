# OS-Quacker-Server-pub-sub
A cross between Twitter and SnapChat, Quacker will have proprietary server technology called QuackIt that can connect news producers with news consumers, but with the twist that new news will always have faster access than old news.

Part 7:
p2.c is the final solution with all working parts together.
Compile using the makefile with the command 'make'.
Now you can run the program as ./p2
This will default the total_publisher, total_subscriber, and number of topics to 1.
Other values can be tested using ./p2 [total_publisher] [total_subscriber] [numer topics]
so for example ./p 10 2 3
will have 10 publishers, 2 subscribers, and 3 topics

The max topic entries is defined default to 1000 and must be changed in the code for other values.

Part 4:
All of the tests are saved as output files and named after their respective tests.
The test outputs provided are 111,n11,1m1,11t,n12,nmt
For example 111test.txt is when the total_publisher, total_subscriber, and number of topics is 1
All of the tests seem to be working perfectly except for when I have 1 pub m subs and 1 topic.

Part 5:
The archive section is practically working exceptfor the fact I feel I could have archived more information.
You can view this in archive.txt that I saved from one of my tests.


Overall the parts are all working together and seem to be finishing successfully. 

Recieved a grade of 105/100.
