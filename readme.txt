Author - Ankesh Airon
UBIT - ankeshai
Person no - 50096547

This is an implementation of count to infinity version of Distance Vector Routing algorithm. 

Following commands are supported :
Server -t <topology file name> -i <routing interval in SECONDS> - supported only at start up or after crash command
Display - displays routing table
Update <id1> <id2> <link cost> - only for links with current router
Step - sends dvr table to neighbours
Packets - displays no of packets received since this command was last used
Disable <router id> - disables link with a neighbour
Crash - emulates server crash. Program stops sending & listening to updates from neighbours. It takes it back to the state before SERVER command was used. SERVER command needs to be used to get it back on the network.
Help - displays list of available options
Creator - display creator information

A struct called "Router" has been defined to collate details of each logical router. It has members like id, ip, portNo, linkCost, firstHopIdToReach, isNeighbour; essentially everything that we need to store for a router. The data types of these members have been kept in line with the way they need to be sent keeping in mind the memory constraints - Unsigned short has been used to store 2 byte values & unsigned int for 4 byte values (ip). IP is being converted to user readable form using a custom utility function whenever there's a need to display it.
A vector<Router> is being used to hold the list of routers, which made it significantly easier to add/remove entries without having to worry about managing additional data structures.
A map<unsigned short, short> has been used to keep a count of updates being missed from each neighbour. It basically maps router id with updates being missed. The counter is updated appropriately whenever an update is received/missed. Entries are added whenever an update from an unknown router is received. Entry is removed when a neighbouring router doesn't send update for 3 consecutive intervals.
Besides these there are standard structs like timeval, sockaddr_in, sockaddr & in_addr which are used for socket communication.

The message to send is being packed using memcpy & the data is being pointed to at by void*. Void* was chosen as it doesn't mislead someone new to the code to believe all of the data to be a of a particular type. The cost of making the code readable was paid by having to typecast it to a different pointer type to increment/decrement it.

On receiving, the data is first cleanly extracted and stored in appropriate temp variables & further operations are done with these temp variables to keep the code flow clean & readable. 

In general, code has been kept readable by extracting functions for every logical unit, using pretty long variable & function names so that a person new to code doesn't have to guess what a variable or function might do.
