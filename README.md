# XarxesDevelopment

Subject of networking, repository based by litle programs using the Windows Sockets 2 (Winsock) library.

## Lab 4: Multiplayer game in C++

### Team members
* Julià Mauri Costa
* Alexis Cosano Rodríguez

### Space of Conflict: Arena
Player versus player gamer where each one controls a spaceship. If you get hit 5 times, you die and the killer obtains a point. When the timer ends, the player with the most kills wins.

Game loop
* When open, the server begins a 10 second countdown, even with no clients connected.
* The match's duration is 2 minutes.
* Once the match is done, the Final Score lasts for 20 seconds, and goes back to the 10 second countdown to start another match.

F1 - Toggle UI invisibility
AWSD - Movement of the spaceship
Space key - Shoot

### Contents
* It admits a certain number of players. - Completely achieved
* It handles players leaving and joining events. - Completely achieved
* World state replication. - Completely achieved
* Redundant sending of input packets. - Completely achieved
* Delivery Manager. - Completely achieved

All work has been done over Discord and the vast majority of commits have been co-authored by both members of the team.

### Known issues
* Rarely, enemy spaceships' sprites cannot be visible, but the spaceships are still there. If they get destroyed, they become visible again.


## Lab 3: Server/Client chat

### Server
* Can kick users.

* Inform to clients:
  * Users connected.
  * Messages.
  * Users disconnections.
  * Kicked by server.
  
### Client
 * Send messages.
 * Recive all information and process.
 
### Boths
* Return button for client and server:
  * If server **closes**, clients detect the disconnection and return giving the feedback.
  * If client **disconnects**, server clear the space of that user and send to other users the disconnection.

* Logs everywhere:
  * Sockets error.
  * Information of connections/disconnections/messages.
  
#### Images
* Example of a sesson

![Example of a sesson](https://github.com/juliamauri/XarxesDevelopment/blob/main/Images/Chat1.PNG)

* Example server kick user

![Example server kick user](https://github.com/juliamauri/XarxesDevelopment/blob/main/Images/Chat2.PNG)

* Example of list users (local and over internet)

![Example of list users (local and over internet)](https://github.com/juliamauri/XarxesDevelopment/blob/main/Images/Chat3.PNG)
