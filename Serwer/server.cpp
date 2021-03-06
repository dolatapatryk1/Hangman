#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <thread>
#include <unordered_set>
#include <signal.h>
#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <cstring>
#include <poll.h>
#include <time.h>
#include "Game.h"

using namespace std;

//#define BUFFOR_LENGTH 50
#define POINTS_TO_SUBTRACT_WHEN_LOSE_ALL_LIFES 5
#define LIFES 10
#define MAX_PLAYERS 16

char PLAYER_READY = '1';
string SEMICOLON = ";";
string GAME_STARTED = "1";
string SEND_FD_TO_PLAYER = "2";
string SEND_RANKING = "3";
string GAME_ENDED = "0";
string GAME_WIN = "1";
string GAME_LOSS = "0";
string GAME_ALREADY_STARTED = "4";

string pathToWords = "./words";

// server socket
int servFd;
//struct gameProperties game;
Game *game = new Game();

// client sockets
std::unordered_set<int> clientFds;
pollfd whatToWait[MAX_PLAYERS + 1] {};
map<char, map<int, long>> lettersSent;
std::unordered_set<char> handledLetters;
map<char, map<int, bool>> confirmationAboutDisablingLetter;

// handles SIGINT
void ctrl_c(int);

// converts cstring to port
uint16_t readPort(char * txt);

// sets SO_REUSEADDR
void setReuseAddr(int sock);

void acceptNewConnection();

bool checkPlayersReady();

void readMessage(int fd);

void sendToAll(char *buffer, int count);

void send(int fd, char *buffer, int count);

void sendFdToPlayer(int clientFd);

void sendRanking();

void readPoll();

void removePlayer(int clientFd);

void handleLetter(char letter, int clientFd);

void getLetterSendTime(char * buffer, int clientFd);

void setConfirmationAboutDisablingLetter(char letter, int clientFd);

bool checkIfEachPlayerDisableButton(char letter);

int checkWhoWasFirst(char letter);

void sendWordAndRanking();

void sendLetterWordRanking(char letter, int clientFd);

void sendEndGameAndWinOrLoss(bool win);

bool checkIfGameEnded();

void sendThatGameIsAlreadyStarted(int clientFd);

int main(int argc, char ** argv){
	game->setStarted(false);
	// get and validate port number
	if(argc != 2) error(1, 0, "Need 1 arg (port)");
	auto port = readPort(argv[1]);
	
	// create socket
	servFd = socket(AF_INET, SOCK_STREAM, 0);
	if(servFd == -1) error(1, errno, "socket failed");
	
	// graceful ctrl+c exit
	signal(SIGINT, ctrl_c);
	// prevent dead sockets from throwing pipe errors on write
	signal(SIGPIPE, SIG_IGN);
	
	setReuseAddr(servFd);
	
	// bind to any address and port provided in arguments
	sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons((short)port), .sin_addr={INADDR_ANY}};
	int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
	if(res) error(1, errno, "bind failed");
	
	// enter listening mode
	res = listen(servFd, 1);
	if(res) error(1, errno, "listen failed");
	
	for(int i = 0; i < MAX_PLAYERS + 1; i++) {
		whatToWait[i].fd = 0;
		whatToWait[i].events = POLLIN;
	}
	whatToWait[0].fd = servFd;
	whatToWait[0].events = POLLIN;
	printf("odpalono serwer\n");
	printf("czekamy na graczy\n");
	
	while(true){
		readPoll();
		if(game->getPlayers().size() < 2) {
			continue;
		}
		
		if(game->checkPlayersReady()) {
			puts("gracze gotowi");
			lettersSent.clear();
			handledLetters.clear();
			game->newGame();
			cout<<"haslo: "<<game->getWord()<<endl<<flush;
			sendWordAndRanking();
			while(game->isStarted()) {
				readPoll();
				if(checkIfGameEnded())
					break;
			}
		} else {
			continue;
		}
	}

	puts("koniec");	
	
	exit(0);
}


//definicje metod
uint16_t readPort(char * txt){
	char * ptr;
	auto port = strtol(txt, &ptr, 10);
	if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"illegal argument %s", txt);
	return port;
}

void setReuseAddr(int sock){
	const int one = 1;
	int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if(res) error(1,errno, "setsockopt failed");
}

void ctrl_c(int){
	for(int clientFd : clientFds)
		close(clientFd);
	close(servFd);
	delete game;
	printf("Closing server\n");
	exit(0);
}

void sendToAll(char * buffer, int count){
	decltype(clientFds) bad;
	for(int clientFd : clientFds){
		int res;
		res = write(clientFd, buffer, count);
		if(res!=count)
			bad.insert(clientFd);
	}
	for(int clientFd : bad){
		removePlayer(clientFd);
	}
}

void send(int fd, char * buffer, int count){
	int res;
	res = write(fd, buffer, count);
	if(res != count) {
		removePlayer(fd);
	}
}

void acceptNewConnection() {
		// prepare placeholders for client address
		sockaddr_in clientAddr{0};
		socklen_t clientAddrSize = sizeof(clientAddr);
		
		// accept new connection
		auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
		if(clientFd == -1) error(1, errno, "accept failed");
		
		// add client to all clients set
		clientFds.insert(clientFd);
		for(int i = 1; i < MAX_PLAYERS + 1; i++) {
			if(whatToWait[i].fd == 0) {
				whatToWait[i].fd = clientFd;
				break;
			}
		}

		Player *newPlayer = new Player(clientFd);
		game->addPlayer(newPlayer);
		if(!game->isStarted()) {
			sendFdToPlayer(clientFd);
			sendRanking();
		}
		else {
			sendThatGameIsAlreadyStarted(clientFd);
		}
		
		printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
}

void readPoll() {
	int ready = poll(whatToWait, MAX_PLAYERS + 1, -1);
	if(ready > 0) {
		for(pollfd & description : whatToWait) {
			if(description.fd == 0)
				continue;
			if(description.revents & POLLIN) {
				if(description.fd == servFd) {
					acceptNewConnection();
				} else {
					readMessage(description.fd);
				}
			}
			if(description.revents & POLLHUP) {
				removePlayer(description.fd);
			}
			if(description.revents & POLLERR) {
				removePlayer(description.fd);
			}
		}
	}
}

void readMessage(int fd) {
	char buffer[30];
	int count = read(fd, buffer, 30);
	if(count < 1) {
		removePlayer(fd);
	} else {
		printf("buffer: %s\n", buffer);
		if(buffer[0] == PLAYER_READY) {
			game->setPlayerReady(fd);
			if(!game->checkPlayersReady())
				sendRanking();
		} else if (buffer[0] >= 'A' && buffer[0] <= 'Z' && game->isStarted()) {
			getLetterSendTime(buffer, fd);
			handleLetter(buffer[0], fd);
		} 
	}
}

void removePlayer(int clientFd) {
	printf("removing %d\n", clientFd);
	clientFds.erase(clientFd);
	close(clientFd);
	game->removePlayer(clientFd);
	for(int i = 0; i < MAX_PLAYERS + 1; i++) {
		if(whatToWait[i].fd == clientFd) {
			whatToWait[i].fd = 0;
			break;
		}
	}
	sendRanking();
}

void handleLetter(char letter, int clientFd) {
	printf("dostałem literke: %c\n", letter);
	int points = game->calculatePoints(letter);
	int fd = checkWhoWasFirst(letter);
	Player *player = game->getPlayers().find(fd)->second;

	auto findLetterHandled = handledLetters.find(letter);
	if(!(findLetterHandled != handledLetters.end())) {
		if(points == 0) {
			player->loseLife();
			if(player->getLifes() == 0)
				player->subtractPoints(POINTS_TO_SUBTRACT_WHEN_LOSE_ALL_LIFES);
		} else {
			player->addPoints(points);
		}
		if(!checkIfGameEnded()) {
			for(int fileDesc : clientFds) {
				sendLetterWordRanking(letter, fileDesc);
			}
		}
		handledLetters.insert(letter);
	}
		
}

void sendFdToPlayer(int clientFd) {
	string fdString = to_string(clientFd);
	int length = fdString.length() + 2 + to_string(fdString.length()).length();
	char buffer[length];
	strcpy(buffer, SEND_FD_TO_PLAYER.c_str());
	strcat(buffer, to_string(fdString.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, fdString.c_str());
	send(clientFd, buffer, length);
}

void sendRanking() {
	string ranking = game->makeRanking();
	int length = ranking.length() + 2 + to_string(ranking.length()).length();
	char buffer[length];
	strcpy(buffer, SEND_RANKING.c_str());
	strcat(buffer, to_string(ranking.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, ranking.c_str());
	sendToAll(buffer, length);
}

void sendWordAndRanking() {
	string wordLengthString = to_string(game->getWordLength());
	string ranking = game->makeRanking();
	int length = game->getWordLength() + 2 + to_string(wordLengthString.length()).length();
	length += ranking.length() + 2 + to_string(ranking.length()).length();
	char buffer[length];

	strcpy(buffer, GAME_STARTED.c_str());
	strcat(buffer, wordLengthString.c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, game->getWordForPlayer().c_str());
	strcat(buffer, SEND_RANKING.c_str());
	strcat(buffer, to_string(ranking.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, ranking.c_str());
	printf("wiadomosc: %s\n", buffer);
	sendToAll(buffer, length);
}

void sendLetterWordRanking(char letter, int clientFd) {
	string wordLengthString = to_string(game->getWordLength());
	string ranking = game->makeRanking();
	int length = game->getWordLength() + 2 + to_string(wordLengthString.length()).length();
	length += ranking.length() + 2 + to_string(ranking.length()).length() + 1 + 2;
	char buffer[length];

	string letterString(1,letter);
	strcpy(buffer, letterString.c_str());
	strcat(buffer, to_string(game->getLifes()).c_str());
	strcat(buffer, to_string(game->getPlayers().find(clientFd)->second->getLifes()).c_str());
	strcat(buffer, GAME_STARTED.c_str());
	strcat(buffer, wordLengthString.c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, game->getWordForPlayer().c_str());
	strcat(buffer, SEND_RANKING.c_str());
	strcat(buffer, to_string(ranking.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, ranking.c_str());
	printf("wiadomosc: %s\n", buffer);
	send(clientFd, buffer, length);
}

void getLetterSendTime(char * buffer, int clientFd) {
	char letter = buffer[0];
	char *timeChar = new char[20];
	int i = 1;
	while(buffer[i] != ';') {
		timeChar[i-1] = buffer[i];
		i++;
	}
	map<char, map<int, long>>::iterator it = lettersSent.find(letter);
	if(it != lettersSent.end()) {
		it->second.insert(make_pair(clientFd, atol(timeChar)));
	} else {
		map<int, long> clientsWithTimes;
		clientsWithTimes.insert(make_pair(clientFd, atol(timeChar)));
		lettersSent.insert(make_pair(letter, clientsWithTimes));
	}

	delete timeChar;

	// map<char, map<int, bool>>::iterator iter = confirmationAboutDisablingLetter.find(letter);
	// if(iter != confirmationAboutDisablingLetter.end()) {
	// 	iter->second.insert(make_pair(clientFd, false));
	// } else {
	// 	map<int, bool> clientsWithConfirmation;
	// 	clientsWithConfirmation.insert(make_pair(clientFd, false));
	// 	confirmationAboutDisablingLetter.insert(make_pair(letter, clientsWithConfirmation));
	// }

}

void setConfirmationAboutDisablingLetter(char letter, int clientFd) {
	puts("aktualizuje potwierdzenie literki\n");
	confirmationAboutDisablingLetter.find(letter)->second[clientFd] = true;
}

bool checkIfEachPlayerDisableButton(char letter) {
	while(true){
		bool check = true;
		map<char, map<int, bool>>::iterator it = confirmationAboutDisablingLetter.find(letter);
		for(map<int, bool>::iterator iter=it->second.begin(); iter!=it->second.end(); ++iter) {
			if(iter->second == false){
				check = false;
				break;
			}
		}
		if(check == true)
			break;
	}
	return true;
}

int checkWhoWasFirst(char letter) {
	int fd = 0;
	map<char, map<int, long>>::iterator it = lettersSent.find(letter);
	long smallestTime = __LONG_MAX__;
	for(map<int, long>::iterator iter=it->second.begin(); iter!=it->second.end(); ++iter) {
		if(iter->second < smallestTime) {
			smallestTime = iter->second;
			fd = iter->first;
		}
	}
	return fd;
}

void sendEndGameAndWinOrLoss(bool win) {
	string ranking = game->makeRanking();
	int length = ranking.length() + 4 + to_string(ranking.length()).length();
	char buffer[length];
	strcpy(buffer, GAME_ENDED.c_str());
	if(win)
		strcat(buffer, GAME_WIN.c_str());
	else
		strcat(buffer, GAME_LOSS.c_str());
	strcat(buffer, SEND_RANKING.c_str());
	strcat(buffer, to_string(ranking.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, ranking.c_str());
	printf("wiadomosc: %s\n", buffer);
	sendToAll(buffer, length);
}

bool checkIfGameEnded() {
	if(game->getLifes() == 0) {
		game->endGame();
		puts("przegrana");
		sendEndGameAndWinOrLoss(false);
		return true;
	}
	if(game->compareWordAndWordForPlayer()) {
		game->endGame();
		puts("wygrana");
		sendEndGameAndWinOrLoss(true);
		return true;
	}
	if(clientFds.size() == 0) {
		game->endGame();
		puts("brak graczy");
		return true;
	}
	return false;
}

void sendThatGameIsAlreadyStarted(int clientFd) {
	string fdString = to_string(clientFd);
	string ranking = game->makeRanking();
	int length = fdString.length() + 2 + to_string(fdString.length()).length() + 1;
	length = ranking.length() + 2 + to_string(ranking.length()).length();
	char buffer[length];
	strcpy(buffer, GAME_ALREADY_STARTED.c_str());
	strcat(buffer, SEND_FD_TO_PLAYER.c_str());
	strcat(buffer, to_string(fdString.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, fdString.c_str());
	strcat(buffer, SEND_RANKING.c_str());
	strcat(buffer, to_string(ranking.length()).c_str());
	strcat(buffer, SEMICOLON.c_str());
	strcat(buffer, ranking.c_str());
	send(clientFd, buffer, length);
}