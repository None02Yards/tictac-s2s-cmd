#ifndef PROTOCOL_H
#define PROTOCOL_H
#define MSG_STATUS "STATUS"   

#define DEFAULT_PORT 5555

#define MSG_HELLO  "HELLO"       // HELLO <name>
#define MSG_CHAT   "CHAT"        // CHAT <text...>
#define MSG_MOVE   "MOVE"        // MOVE <1-9>
#define MSG_BOARD  "BOARD"       // BOARD <9chars>
#define MSG_TURN   "TURN"        // TURN X|O
#define MSG_RESULT "RESULT"      // RESULT X|O|D
#define MSG_PING   "PING"
#define MSG_PONG   "PONG"

#endif
