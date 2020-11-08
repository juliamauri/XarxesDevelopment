# XarxesDevelopment

Subject of networking, repository based by litle programs using the Windows Sockets 2 (Winsock) library.

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
