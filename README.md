# s2s-tictactoe

A tiny, TCP line-protocol **Tic-Tac-Toe** played between **two server processes** (host ↔ client) in the terminal.  
Includes real-time chat, auto-reconnect with exponential backoff, and an optional **bot** that takes over if the peer goes offline.

```bash
X | O | 3
---+---+---
4 | X | 6
---+---+---
7 | 8 | O

```


## Features

- **Two-process play** over raw TCP (no third-party services)
- **Line protocol** (`HELLO`, `MOVE`, `BOARD`, `TURN`, `RESULT`, `PING/PONG`, `STATUS`)
- **Auto-reconnect**  
  - Client: backoff **1s → 2s → 4s → 8s → 16s**  
  - Host: re-`accept()` when peer drops
- **Paused round UX** when peer disconnects (prompt to **/rematch** or **/exit**)
- **Bot takeover** (`/bot on`) – solo play when peer is offline
- **Scoreboard** (X/O/Draw) shown after each round, on demand via `/score`
- Turn hints after every move: “👉 Your turn” / “Waiting…”
- Minimal, portable C (pthreads + BSD sockets)

---

## 🗂 Project layout

```bash
/play — start a new round (keeps scores)

/rematch — alias for /play

/move <1-9> — place your mark (only on your turn)

/board — show the current grid (only during an active round)

/reset — reset current round (keeps scores, stays active)

/score — print scoreboard (X/O/Draw totals)

/hello — send a greeting (auto one-time reply)

/ping — liveness + round trip time (RTT)

/bot on|off — enable bot takeover when peer is offline

/exit — abandon a paused round

```

# ex

```bash 

/play
[start]
 1 | 2 | 3
---+---+---
 4 | 5 | 6
---+---+---
 7 | 8 | 9
👉 Your turn (X). Type: /move <1-9>
/move 5
[local]
 1 | 2 | 3
---+---+---
 4 | X | 6
---+---+---
 7 | 8 | 9
⏳ Waiting for O...
```
# bot role 

```bash
[net] Peer lost; reconnecting…
[s2s] peer lost; retrying in 1s...
[s2s] peer lost; retrying in 2s...
[s2s] peer lost; retrying in 4s...
[s2s] peer lost; retrying in 8s...
[s2s] peer lost; retrying in 16s...
[s2s] peer lost; retrying in 16s...
/bot on
🤖 Bot enabled for peer role X.
[s2s] peer lost; retrying in 16s...[bot] Taking over for x.
[s2s] peer lost; waiting for new connection...
/move 5
[local]
 1 | 2 | 3
---+---+---
 X | X | 6
---+---+---
 O | 8 | 9
🤖 Bot (O) is thinking...
[bot]
 O | 2 | 3
---+---+---
 X | X | 6
---+---+---
 O | 8 | 9
👉 Your turn (X). Type: /move <1-9>
/move 2
[local]
 O | X | 3
---+---+---
 X | X | 6
---+---+---
 O | 8 | 9
🤖 Bot (O) is thinking...
[bot]
 O | X | O
---+---+---
 X | X | 6
---+---+---
 O | 8 | 9
👉 Your turn (X). Type: /move <1-9>
/move 6
[local]
 O | X | O
---+---+---
 X | X | X
---+---+---
 O | 8 | 9
[result] X wins — type /play for a new round.

====== SCOREBOARD ======
X Wins : 1
O Wins : 0
Draws  : 0
========================
```

build JSON mode: 
```bash
make clean && make PROTO_JSON=1
```