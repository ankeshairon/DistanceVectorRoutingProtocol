#include <iostream>
#include <fstream>
#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <cstring>
#include <sstream>
#include <errno.h>
#include <vector>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <math.h>
#include <map>

using namespace std;

#define STDIN 0
#define MAX_MSG_SIZE 1472

struct Router {
	unsigned short id;
	unsigned int ip;
	unsigned short portNo;
	unsigned short linkCost;
	bool isNeighbor;

	unsigned short firstHopIdToReach;
};

unsigned short INFINITE = INFINITY;
unsigned short UNKNOWN = INFINITY;

vector<Router> allRouters;
unsigned short currentRouterId;
unsigned short currentPortNo;

float routingUpdateInterval;

int masterSocketDescriptor;
sockaddr_in masterSocketAddress;
unsigned int currentIp;

int distanceVectorPacketsReceived;

map<unsigned short, short> neighborsUpdateMissedCounter;
struct timeval updateTimer;

unsigned int convertIpTo4Bytes(string ip15Byte){
	return inet_addr(ip15Byte.c_str());
}

string convertIpTo15Bytes(unsigned int ip4Byte){
	struct in_addr inaddr;
	inaddr.s_addr = ip4Byte;
	string revived(inet_ntoa(inaddr));
	return revived;
}

void getAndSaveCurrentIp() {
	struct sockaddr_in socketAddress;
	char *ip = new char[256];

	int socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
	if (socketDescriptor < 0) {
		perror("Socket error");
	}

	memset(&socketAddress, 0, sizeof(socketAddress));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(53);     //DNS port number to send the request to
	socketAddress.sin_addr.s_addr = inet_addr("208.67.220.220");     //DNS IP

	if (connect(socketDescriptor, ((sockaddr*) &socketAddress), INET_ADDRSTRLEN) == -1) {
		cout << "Unable to connect. Could not determine current ip" << endl;
		return;
	}

	unsigned int addressLength = INET_ADDRSTRLEN;

	getsockname(socketDescriptor, ((sockaddr*) &socketAddress), &addressLength);

	inet_ntop(AF_INET, &(socketAddress.sin_addr), ip, 256);
	close(socketDescriptor);
	currentIp = convertIpTo4Bytes(ip);
}

void readNetworkDetailsFromFileAndInferCurrentRouterId(int& noOfServers, ifstream& fileInStream) {
	string tempIp;
	for (int i = 0; i < noOfServers; i++) {
		Router routerDetails;
		fileInStream >> routerDetails.id >> tempIp >> routerDetails.portNo;
		routerDetails.ip = convertIpTo4Bytes(tempIp);
		allRouters.push_back(routerDetails);

		if (routerDetails.ip == currentIp) {
			currentRouterId = routerDetails.id;
			currentPortNo = routerDetails.portNo;
		}
	}
}

void updateLinkCostsAndMarkneighbors(int noOfneighbors, int neighborCostEntries[][3]) {
	bool isModified;
	for (unsigned short k = 0; k < allRouters.size(); k++) {
		isModified = false;
		for (int j = 0; j < noOfneighbors; j++) {

			//is a valid neighbor entry for this router?
			if ((allRouters[k].id == neighborCostEntries[j][0] && currentRouterId == neighborCostEntries[j][1])
						|| (allRouters[k].id == neighborCostEntries[j][1] && currentRouterId == neighborCostEntries[j][0])) {
				//neighbor entry
				allRouters[k].linkCost = neighborCostEntries[j][2];
				allRouters[k].firstHopIdToReach = allRouters[k].id;
				allRouters[k].isNeighbor = true;

				neighborsUpdateMissedCounter.insert(pair<unsigned short, short>(allRouters[k].id, 0) );

				isModified = true;
				break;
			} else if(allRouters[k].id == currentRouterId){
				//self entry
				allRouters[k].linkCost = 0;
				allRouters[k].firstHopIdToReach = currentRouterId;
				allRouters[k].isNeighbor = false;
				isModified = true;
				break;
			}
		}
		if (!isModified) {
			//not a neighbor entry
			allRouters[k].linkCost = UNKNOWN;
			allRouters[k].firstHopIdToReach = UNKNOWN;
			allRouters[k].isNeighbor = false;
		}

	}
}

bool establishNetworkTopology(string topologyFile){
	ifstream fileInStream(topologyFile.c_str(), ios::in);
	int noOfServers, noOfneighbors;

	if (!fileInStream) {
		cout << "SERVER could not open topology file. Please try again.." << endl;
		return false;
	}

	fileInStream >> noOfServers >> noOfneighbors;

	readNetworkDetailsFromFileAndInferCurrentRouterId(noOfServers, fileInStream);

	//extract neighbor cost entries to a 2D array
	int neighborCostEntries[noOfneighbors][3];
	for (int i = 0; i < noOfneighbors; i++) {
		for (int j = 0; j < 3; j++) {
			fileInStream >> neighborCostEntries[i][j];
		}
	}

	updateLinkCostsAndMarkneighbors(noOfneighbors, neighborCostEntries);

	return true;
}

string getFilenameAndRouterInterval(vector<string> keywords) {
	string topologyFile;
	for (int i = 1; i < 5; i++) {
		if (keywords[i] == "-t") {
			topologyFile = keywords[i + 1];
		} else if (keywords[i] == "-i") {
			routingUpdateInterval = atof(keywords[i + 1].c_str());
		}
	}
	return topologyFile;
}

void updateLink(vector<string> keywords) {
	unsigned short routerLink1Id = atoi(keywords[1].c_str());
	unsigned short routerLink2Id = atoi(keywords[2].c_str());

	unsigned short newLinkCost;
	if (keywords[3] == "inf"){
		newLinkCost = INFINITE;
	}else{
		newLinkCost = atoi(keywords[3].c_str());
	}

	for (unsigned short k = 0; k < allRouters.size(); k++) {
		if ((allRouters[k].id == routerLink1Id && currentRouterId == routerLink2Id)
					|| (allRouters[k].id == routerLink2Id && currentRouterId == routerLink1Id)) {

			allRouters[k].linkCost = newLinkCost;

			cout << "Link cost updated successfully" << endl;
			cout << "UPDATE SUCCESS" << endl;
			return;
		}
	}
	cout << "UPDATE FAILED since no link exists between " << routerLink1Id << " and " << routerLink2Id << endl;
}

const char * getNextHopId(unsigned int& i) {
	if (allRouters[i].firstHopIdToReach == UNKNOWN) {
		return "Unknown";
	}
	stringstream ss;
	ss << allRouters[i].firstHopIdToReach;

	char * nextHopId = new char[20];
	strcpy(nextHopId, ss.str().c_str());

	ss.str("");
	return nextHopId;
}

const char * getLinkCost(unsigned int i) {
	if (allRouters[i].linkCost == INFINITE) {
		return "Infinity";
	}
	stringstream ss;
	ss << allRouters[i].linkCost;

	char * linkCost = new char[20];
	strcpy(linkCost, ss.str().c_str());

	ss.str("");
	return linkCost;
}

void displayDistanceVectorRoutingTable(){
	cout << "Destination-server-ID  Next-hop-server-id  cost-of-path" << endl;

	for(unsigned int i = 0; i < allRouters.size() ; i++){
		cout << "\t" << allRouters[i].id << "\t\t" << getNextHopId(i) << "\t\t\t" << getLinkCost(i) << endl;
	}
}

void getSerializedDVRTable(void *serializedTable){
	unsigned short data2Byte;
	void *ptr = serializedTable;

	data2Byte = (unsigned short)allRouters.size();
	memcpy(ptr, (void *)&data2Byte, 2);

	ptr = ((unsigned short *)ptr)+1;
	memcpy(ptr, (void *)&currentPortNo, 2);

	ptr = ((unsigned short *)ptr)+1;
	memcpy(ptr, (void *)&currentIp, 4);

	ptr = ((unsigned short *)ptr)+2;

	for (unsigned short i=0; i<allRouters.size();i++){
		memcpy(ptr, (void *)&allRouters[i].ip, 4);
		ptr = ((unsigned short *)ptr)+2;

		memcpy(ptr, (void *) &allRouters[i].portNo, 2);
		ptr = ((unsigned short *) ptr) + 1;

		memset(ptr, 0, 2);
		ptr = ((unsigned short *) ptr) + 1;

		memcpy(ptr, (void *) &allRouters[i].id, 2);
		ptr = ((unsigned short *) ptr) + 1;

		memcpy(ptr, (void *) &allRouters[i].linkCost, 2);
		ptr = ((unsigned short *) ptr) + 1;
	}
}

void resetRoutingUpdateTimer() {
	updateTimer.tv_sec = floor(routingUpdateInterval);
	updateTimer.tv_usec = (routingUpdateInterval - floor(routingUpdateInterval)) * 1000;
}

void sendDVRTableToNeighbors(void *serializedTable){
	int noOfBytesSent;
	struct sockaddr_in destinationSocketAddress;
	int destinationSocketAddressSize = sizeof(destinationSocketAddress);

	memset((char *) &destinationSocketAddress, 0, sizeof(destinationSocketAddress));
	destinationSocketAddress.sin_family = AF_INET;

	int messageSize = 8 + 12 * allRouters.size();

	for (unsigned short i = 0; i < allRouters.size(); i++) {
		if(allRouters[i].isNeighbor){
			destinationSocketAddress.sin_port = htons(allRouters[i].portNo);
			destinationSocketAddress.sin_addr.s_addr = allRouters[i].ip;

			if ((noOfBytesSent = sendto(masterSocketDescriptor, serializedTable, messageSize, 0, (struct sockaddr*) &destinationSocketAddress, destinationSocketAddressSize)) == -1) {
				cout << "Could not send to neighbor " + allRouters[i].id << endl;
			} else {
				cout << "Sent " << noOfBytesSent << " bytes to ";
				cout << convertIpTo15Bytes(allRouters[i].ip);
				cout << " successfully" << endl;
			}
		}
	}
}

void sendRoutingUpdatesToNeighbors(){
	void *serializedTable = new char[MAX_MSG_SIZE];

	getSerializedDVRTable(serializedTable);
	sendDVRTableToNeighbors(serializedTable);
	cout << "STEP SUCCESS!" << endl;
}

void displayDistanceVectorPacketsReceived(){
	cout << "Packets received since last checked : " << distanceVectorPacketsReceived << endl;
	distanceVectorPacketsReceived = 0;
}

Router * getRouterWithDetails(unsigned short id){
	for (unsigned short i = 0; i < allRouters.size(); i++) {
		if (allRouters[i].id == id){
			return &allRouters[i];
		}
	}
	return NULL;
}

Router * getRouterInDvrTableWithDetails(vector<Router> &dvrTable, unsigned int destinationIp, int destinationPort){
	for (unsigned short i = 0; i < dvrTable.size(); i++) {
		if (dvrTable[i].ip == destinationIp && dvrTable[i].portNo == destinationPort){
			return &dvrTable[i];
		}
	}
	return NULL;
}

void disableLink(unsigned short idToDisable){
	if(idToDisable == currentRouterId){
		cout << "Can not disable link with itself" << endl;
		return;
	}

	Router *routerToDisableLinkWith;
	routerToDisableLinkWith = getRouterWithDetails(idToDisable);

	if(routerToDisableLinkWith == NULL){
		cout << "Could not disable link because unknown router ID : " << idToDisable << endl;
		return;
	}

	if (!routerToDisableLinkWith->isNeighbor){
		cout << "Could not disable link between " << currentRouterId << " & " << idToDisable << " since they are not neighbors!" << endl;
		return;
	}

	routerToDisableLinkWith->linkCost = INFINITE;
	routerToDisableLinkWith->firstHopIdToReach = UNKNOWN;
	routerToDisableLinkWith->isNeighbor = false;

	//mark routers reachable through disabled router as unreachable
	for (unsigned short i = 0 ; i < allRouters.size() ; i++){
		if (allRouters[i].firstHopIdToReach == idToDisable){
			allRouters[i].firstHopIdToReach = UNKNOWN;
			allRouters[i].linkCost = INFINITE;
		}
	}

	cout << "Link between " << currentRouterId << " & " << idToDisable << " disabled successfully!" << endl;
	cout << "DISABLE SUCCESS!" << endl;
}

void crashRouter(){
	for (unsigned short i = 0 ; i < allRouters.size(); i++){
		allRouters[i].firstHopIdToReach = UNKNOWN;
		allRouters[i].linkCost = INFINITE;
		allRouters[i].isNeighbor = false;
	}
	cout << "CRASH SUCCESS!" << endl;
}

void incrementUpdateMissedCounterForAllEntries(){
	map<unsigned short, short>::iterator it = neighborsUpdateMissedCounter.begin();
	for (it=neighborsUpdateMissedCounter.begin(); it!=neighborsUpdateMissedCounter.end(); ++it){
		++it->second;
	}
}

vector<unsigned short> getneighborsThatMightHaveBeenDisconnected(){
	vector<unsigned short> disconnectedneighborIds;

	map<unsigned short, short>::iterator it = neighborsUpdateMissedCounter.begin();
	for (it=neighborsUpdateMissedCounter.begin(); it!=neighborsUpdateMissedCounter.end(); ++it){
		if(it->second >= 3){
			disconnectedneighborIds.push_back(it->first);
			neighborsUpdateMissedCounter.erase(it->first);
		}
	}

	return disconnectedneighborIds;
}

void disableLinkWithneighborThatAreNotSendingUpdates() {
	vector<unsigned short> disconnectedneighborIds;
	disconnectedneighborIds = getneighborsThatMightHaveBeenDisconnected();
	for (unsigned short i = 0; i < disconnectedneighborIds.size(); i++) {
		disableLink(disconnectedneighborIds[i]);
		cout << "Link with server with id " << disconnectedneighborIds[i] << " disabled since no updates received for 3 consecutive intervals" << endl;
	}
}

void step() {
	//time out
	sendRoutingUpdatesToNeighbors();
	incrementUpdateMissedCounterForAllEntries();
	disableLinkWithneighborThatAreNotSendingUpdates();
	resetRoutingUpdateTimer();
}

bool getUserInputAndDelegate() {
	char userInput[500];
	cin.getline(userInput, sizeof userInput);

	stringstream iss;
	iss << userInput;

	vector<string> keywords;
	string keyword;

	for (int i = 0; i < 5; i++) {
		iss >> keyword;
		keywords.push_back(keyword);
	}

	if (keywords[0] == "SERVER" || keywords[0] == "server") {
		cout << "SERVER command failed to execute. Can not overwrite existing network topology. Please restart to establish new topology" << endl;
	}  else if (keywords[0] == "UPDATE" || keywords[0] == "update") {
		updateLink(keywords);
	} else if (keywords[0] == "STEP" || keywords[0] == "step") {
		step();
	} else if (keywords[0] == "PACKETS" || keywords[0] == "packets") {
		displayDistanceVectorPacketsReceived();
	} else if (keywords[0] == "DISABLE" || keywords[0] == "disable") {
		disableLink(atoi(keywords[1].c_str()));
	} else if (keywords[0] == "CRASH" || keywords[0] == "crash") {
		crashRouter();
		return true;
	 } else if (keywords[0] == "DISPLAY" || keywords[0] == "display") {
		 displayDistanceVectorRoutingTable();
	} else if (keywords[0] == "HELP" || keywords[0] == "help") {
		cout << "Available operations: \nUpdate\nStep\nPackets\nDisable\nCrash\nDisplay\nHelp\nCreator\n";
	} else if (keywords[0] == "CREATOR" || keywords[0] == "creator") {
		 cout << "Name : Ankesh Airon\nUBIT name : ankeshai\nEmail ID : ankeshai@buffalo.edu\nPerson no:50096547";
	} else {
		cout << "Invalid keyword. Please try again. Here's a list of available options:" << endl;
		cout << "Available operations: \nUpdate\nStep\nPackets\nDisable\nCrash\nDisplay\nHelp\nCreator\n";
	}
	return false;
}

void populateSourceRouterDetailsFromReceivedPacket(unsigned short & noOfEntries, void*& dataPtr,
	unsigned short & sourcePort, unsigned int& sourceIp) {
	memcpy(&noOfEntries, dataPtr, 2);
	//	cout << noOfEntries << endl;

	dataPtr = ((unsigned short *) (dataPtr)) + 1;
	memcpy(&sourcePort, dataPtr, 2);
	//	cout << sourcePort << endl;

	dataPtr = ((unsigned short *) (dataPtr)) + 1;
	memcpy(&sourceIp, dataPtr, 4);
	//	cout << convertIpTo15Bytes(sourceIp) << endl;

	dataPtr = ((unsigned short *) (dataPtr)) + 2;
}

void populateRouterDetailsInEntry (void** dataPtr, Router *router) {
	memcpy(&(router->ip), *dataPtr, 4);
	*dataPtr = ((unsigned short *) (*dataPtr)) + 2;

	memcpy(&(router->portNo), *dataPtr, 2);
	*dataPtr = ((unsigned short *) (*dataPtr)) + 1;

	*dataPtr = ((unsigned short *) (*dataPtr)) + 1;

	memcpy(&(router->id), *dataPtr, 2);
	*dataPtr = ((unsigned short *) (*dataPtr)) + 1;

	memcpy(&(router->linkCost), *dataPtr, 2);
	*dataPtr = ((unsigned short *) (*dataPtr)) + 1;
}

vector<Router> reconstructDVRTable(void *dataPtr, unsigned short & noOfEntries){
	vector<Router> dvrTableReceived;

	for (int i = 0; i < noOfEntries; i++) {
		Router routerEntry;
		populateRouterDetailsInEntry(&dataPtr, &routerEntry);
		dvrTableReceived.push_back(routerEntry);
	}
	return dvrTableReceived;
}

void populateIdNextHopAndLinkCostForNewRouter(vector<Router> &routerEntries, Router* &sourceRouter) {
	for (unsigned short i = 0; i < routerEntries.size(); i++) {

		if (routerEntries[i].linkCost == 0) {
			sourceRouter->id = routerEntries[i].id;
			sourceRouter->firstHopIdToReach = routerEntries[i].id;
		}else if (routerEntries[i].id == currentRouterId) {
			sourceRouter->linkCost = routerEntries[i].linkCost;
		}
	}
}

void markRouterAsDisconnectedIfCountedToInfinity(Router*& routerEntry, Router*& sourceRouter, Router& dvrTableEntryRxdFromSource) {
//	cout << "===========" << routerEntry->linkCost <<  " <= " << sourceRouter->linkCost << " || " << routerEntry->linkCost << " <= " << dvrTableEntryRxdFromSource.linkCost << "===========" << endl;
	if (routerEntry->linkCost < sourceRouter->linkCost || routerEntry->linkCost < dvrTableEntryRxdFromSource.linkCost) {
		routerEntry->firstHopIdToReach = UNKNOWN;
		routerEntry->linkCost = INFINITE;
		routerEntry->isNeighbor = false;
	}
}

void resetUpdateMissedCounterForEntry(unsigned short id) {
	//decrement update missed counter for sender i.e. mark as update received
	if (neighborsUpdateMissedCounter[id] > 0){
		neighborsUpdateMissedCounter[id] = 0;
	}

}

unsigned short calculateFirstHopIdToReach(unsigned short destinationId){
	Router *firstHopRouter;
	unsigned int calculatedFirstHop;
	firstHopRouter = getRouterWithDetails(destinationId);
	if (firstHopRouter->firstHopIdToReach == firstHopRouter->id) {
		calculatedFirstHop = firstHopRouter->firstHopIdToReach;
	} else {
		calculatedFirstHop = calculateFirstHopIdToReach(firstHopRouter->firstHopIdToReach);
	}
	return calculatedFirstHop;
}

void deserializeAndUpdateDVRTable(void *dataPtr){
	unsigned short noOfEntries;

	Router *sourceRouter;
	unsigned short sourcePort;
	unsigned int sourceIp;

	//read source router details and add it to the list if it's not there already
	populateSourceRouterDetailsFromReceivedPacket(noOfEntries, dataPtr, sourcePort, sourceIp);

	vector<Router> dvrTableRxdFromSource = reconstructDVRTable(dataPtr, noOfEntries);
	sourceRouter = getRouterInDvrTableWithDetails(allRouters, sourceIp, sourcePort);

	if (sourceRouter == NULL){
		//update received from a router unknown till now, so add this router
		sourceRouter = (Router *) calloc (1 ,sizeof(struct Router));
		sourceRouter->ip = sourceIp;
		sourceRouter->portNo = sourcePort;

		populateIdNextHopAndLinkCostForNewRouter(dvrTableRxdFromSource, sourceRouter);

		cout << "New router found. Sender with ip ";
		cout << convertIpTo15Bytes(sourceIp);
		cout << " and port " << sourcePort << " added to the list." << endl;

		allRouters.push_back(*sourceRouter);
		neighborsUpdateMissedCounter.insert(pair<unsigned short, short>(sourceRouter->id, 0));
	} else if (sourceRouter->linkCost == INFINITE){
		//previously disabled or known but disconnected router sending again
		populateIdNextHopAndLinkCostForNewRouter(dvrTableRxdFromSource, sourceRouter);

		neighborsUpdateMissedCounter.insert(pair<unsigned short, short>(sourceRouter->id, 0));
		cout << "Adding server with ip ";
		cout << convertIpTo15Bytes(sourceRouter->ip);
		cout << " and port " << sourceRouter->portNo << " as a neighbor" << endl;
	} else if (!sourceRouter->isNeighbor){
		//a previously known router with finite cost but a new neighbor
		int prospectiveNewDirectCost;
		prospectiveNewDirectCost = getRouterInDvrTableWithDetails(dvrTableRxdFromSource, currentIp, currentPortNo)->linkCost;

		// update link cost & first hop id if it should
		if (sourceRouter->linkCost > prospectiveNewDirectCost ){
			sourceRouter->linkCost = prospectiveNewDirectCost;
			sourceRouter->firstHopIdToReach = sourceRouter->id;
		}
		neighborsUpdateMissedCounter.insert(pair<unsigned short, short>(sourceRouter->id, 0));
		cout << "Adding server with ip ";
		cout << convertIpTo15Bytes(sourceRouter->ip);
		cout << " and port " << sourceRouter->portNo << " as a neighbor" << endl;
	} else{
		//decrement update missed counter for sender i.e. mark as update received
		resetUpdateMissedCounterForEntry(sourceRouter->id);
	}

	sourceRouter->isNeighbor = true;

	//update current router's dvr table from received table - dvrTableRxd
	Router *routerEntryInCurrentTable;

	for (unsigned short i = 0; i < dvrTableRxdFromSource.size(); i++) {
		//if router entry is corresponding to entry present in current table
		if((routerEntryInCurrentTable = getRouterInDvrTableWithDetails(allRouters, dvrTableRxdFromSource[i].ip, dvrTableRxdFromSource[i].portNo)) != NULL) {

			//existing entry
			if(routerEntryInCurrentTable->firstHopIdToReach == sourceRouter->id){
				//overwrite if entry from the original connecting link
				routerEntryInCurrentTable->linkCost = sourceRouter->linkCost + dvrTableRxdFromSource[i].linkCost;
				markRouterAsDisconnectedIfCountedToInfinity(routerEntryInCurrentTable, sourceRouter, dvrTableRxdFromSource[i]);
//				cout << "Setting link cost to " << routerEntryInCurrentTable->linkCost << " & first hop id as " << routerEntryInCurrentTable->firstHopIdToReach << " for router id " << routerEntryInCurrentTable->id << endl;
			}else if (routerEntryInCurrentTable->linkCost > sourceRouter->linkCost + dvrTableRxdFromSource[i].linkCost) {
				//update cost and set next hop if from some other link
				routerEntryInCurrentTable->linkCost = sourceRouter->linkCost + dvrTableRxdFromSource[i].linkCost;
				routerEntryInCurrentTable->firstHopIdToReach = calculateFirstHopIdToReach(sourceRouter->id);
				markRouterAsDisconnectedIfCountedToInfinity(routerEntryInCurrentTable, sourceRouter, dvrTableRxdFromSource[i]);
//				cout << "Setting link cost to " << routerEntryInCurrentTable->linkCost << " & first hop id as " << routerEntryInCurrentTable->firstHopIdToReach << " for router id " << routerEntryInCurrentTable->id << endl;
			}
		}//else add new entry for received entry
		else {
			routerEntryInCurrentTable = (Router *) calloc (1 ,sizeof(struct Router));
			routerEntryInCurrentTable->id = dvrTableRxdFromSource[i].id;
			routerEntryInCurrentTable->ip = dvrTableRxdFromSource[i].ip;
			routerEntryInCurrentTable->portNo = dvrTableRxdFromSource[i].portNo;
			routerEntryInCurrentTable->linkCost = sourceRouter->linkCost + dvrTableRxdFromSource[i].linkCost;
			routerEntryInCurrentTable->firstHopIdToReach = calculateFirstHopIdToReach(sourceRouter->id);
			routerEntryInCurrentTable->isNeighbor = false;

			allRouters.push_back(*routerEntryInCurrentTable);

			cout << "New router with ip ";
			cout << convertIpTo15Bytes(dvrTableRxdFromSource[i].ip);
			cout << " and port " << dvrTableRxdFromSource[i].portNo << " added based on DVR from ip ";
			cout << convertIpTo15Bytes(sourceIp);
			cout << " and port " << sourcePort << endl;
		}
	}
}

unsigned int getSenderIpFromPacket(void *dataPacket) {
	void *ptr = ((unsigned short *)dataPacket)+2;
	unsigned int ip;
	memcpy(&ip, ptr, 4);
	return ip;
}

void receiveAndSaveDVRTable() {
	int noOfBytesReceived;
	sockaddr_in senderSocketAddress;
	unsigned int destSocketAddressLength = sizeof(senderSocketAddress);
	char dataRxd[MAX_MSG_SIZE];

	if ((noOfBytesReceived = recvfrom(masterSocketDescriptor, dataRxd, MAX_MSG_SIZE, 0, (struct sockaddr*) (&senderSocketAddress),&destSocketAddressLength)) == -1) {
		char *ip = new char[256];
		inet_ntop(AF_INET, &(senderSocketAddress.sin_addr), ip, 256);
		cout << "Receive attempt failed from " << ip << " failed!" << endl;
		return;
	} else{
		unsigned int senderIp4 = getSenderIpFromPacket(dataRxd);
		string senderIp15 = convertIpTo15Bytes(senderIp4);
		cout << "Received a message of " << noOfBytesReceived << " bytes received from SERVER with Ip " << senderIp15 << endl;
		distanceVectorPacketsReceived = distanceVectorPacketsReceived + noOfBytesReceived;
	}

	void *data = &dataRxd;
	deserializeAndUpdateDVRTable(data);
}

int startRouter() {
	int activity, maxSocketDescriptor;
	fd_set readfds;
	bool hasCrashed = false;

	//create socket
	if ((masterSocketDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
		perror("socket failed");
		return -1;
	}

	//create socket sockaddr_in
	memset(&masterSocketAddress, 0, sizeof(sockaddr_in));

	//type of socket created
	masterSocketAddress.sin_family = AF_INET;
	masterSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	masterSocketAddress.sin_port = htons(currentPortNo);

	//bind the socket to port specified
	if (bind(masterSocketDescriptor, (struct sockaddr *) &masterSocketAddress,
			sizeof(struct sockaddr_in)) < 0) {
		perror("bind failed");
		return -1;
	}
	resetRoutingUpdateTimer();
	while (1) {
		//clear the socket set
		FD_ZERO(&readfds);

		//add standard input & master socket to set
		FD_SET(STDIN, &readfds);
		maxSocketDescriptor = STDIN;
		FD_SET(masterSocketDescriptor, &readfds);
		maxSocketDescriptor = masterSocketDescriptor;

		//wait for an activity on one of the sockets , so wait indefinitely
		activity = select(maxSocketDescriptor + 1, &readfds, NULL, NULL, &updateTimer);

		if ((activity < 0) && (errno != EINTR)) {
			perror("select error");
		}

		//If something happened on the std, respond to user input
		if (FD_ISSET(STDIN, &readfds)) {
			hasCrashed = getUserInputAndDelegate();
			if(hasCrashed){
				break;
			}
		} else if(FD_ISSET(masterSocketDescriptor, &readfds)) {
			receiveAndSaveDVRTable();
		} else {
			//time out
			step();
		}
	}
	close(masterSocketDescriptor);
	return 0;
}


bool readTopologyFileAndEstablishNetworkTopology(vector<string> keywords) {

	string topologyFile = new char[200];
	topologyFile = getFilenameAndRouterInterval(keywords);

	if (establishNetworkTopology(topologyFile)){
		cout << "Network topology established successfully! Router is now online..." << endl;
		cout << "SERVER SUCCESS" << endl;
		return true;
	}
	return false;
}

int main(int noOfArgs, char *args[]) {
	cout << "Welcome to Distance Vector Routing Algorithm implementation!" <<endl;

	getAndSaveCurrentIp();

	char userInput[500];
	vector<string> words;
	string newWord;
	while (true) {
		distanceVectorPacketsReceived = 0;
		currentRouterId = UNKNOWN;
		allRouters.clear();
		neighborsUpdateMissedCounter.clear();

		cout << "Router is currently OFFLINE. Use SERVER command to go ONLINE..." << endl;
		cin.getline(userInput, sizeof userInput);

		stringstream iss;
		iss << userInput;

		iss >> newWord;

		if(! (newWord == "SERVER" || newWord == "server")) {
			iss.str("");
			continue;
		}

		words.clear();
		words.push_back(newWord);

		for (int i = 0; i < 4; i++) {
			iss >> newWord;
			words.push_back(newWord);
		}

		if (readTopologyFileAndEstablishNetworkTopology(words)) {
			startRouter();
		}
	}
}
