# Client/Server Example

This is an example application for a client/server application.
This was made for Win32, using Winsock, in pure C code. 
Some modifications would have to be done for OSX/*nix systems.

The application as is implements a chat client. All messages sent from a client get broadcast to the other
clients.
Messages on the clients are only received when a message is sent (or return is pressed).
The application implements threading on the server side, with a mutex object to shield a critical variable
containing the list of active connections.

The code has several vulnerabilities, but for simplicity's sake no validation is done of inputs in 
particular.

Message serialization is done through direct sending of structures. This only works if we guarantee the
sizes/alignment of structures are equal in both the client/server. If it is compiled on the same type of
system with the same compiler, it should work fine.

If that is not the case, some more sophisticated serialization has to be implemented, to convert endianess
and/or data sizes.

## License

[Apache 2.0](LICENSE)