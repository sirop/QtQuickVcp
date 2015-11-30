#ifndef MACHINETALK_H
#define MACHINETALK_H

enum SocketState {
    SocketDown = 1,
    SocketTrying = 2,
    SocketUp = 3,
    SocketTimeout = 4,
    SocketError = 5
};

#endif // MACHINETALK_H

