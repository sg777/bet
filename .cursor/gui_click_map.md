# GUI E2E click map (for agent use only)

URL: `http://159.69.23.31:1234/`
Browser viewId: assigned per session, e.g. `a724f8`. Reuse `newTab: true` only on first navigate; thereafter reuse the same viewId.

After every page state change, re-snapshot (`browser_snapshot`). Refs are dynamic; the **labels** below are stable, the ref letters are not.

## Phase 4 — Set Nodes flow

| Step | Element | Stable label | Action | Notes |
|------|---------|-------------|--------|-------|
| 1 | Tab | `Private Table` | click | Switch from default Public Tables tab |
| 2 | Sub-tab | `Player` | click | Default sub-tab is Dealer; switch |
| 3 | textbox `player IP` | (default `159.69.23.31`) | leave | Pre-populated correctly |
| 4 | spinbutton `player Port` | (default `9001`) | leave | Pre-populated correctly |
| 5 | button | `Set Nodes` | click | WS connects, modal closes, "Find Table" pad appears |
| 6 | button | `Find Table` | click | Triggers `table_info` request to backend |

After step 6, snapshot returns 0 interactive refs even though SIT HERE buttons are visible — fall through to coordinate clicks.

## Phase 5 — Take seat

Snapshot is empty; `browser_take_screenshot` first, then `browser_mouse_click_xy`.

Two SIT HERE buttons appear at:

| Seat | Approx x, y in 1024x768 viewport | GUI semantic |
|------|---------------------------------|--------------|
| Top center | (657, 207) | seat 0 in GUI; click here lands on `<div class="ChipsGrid">` not the button — DO NOT use |
| Right side | (834, 325) | seat 1 in GUI; click here lands on `<div class="PlayerName">SITTING…>` and **does** trigger join_table — USE THIS |

Backend `playerid` is assigned by join order (we always join first → backend playerid = 0). GUI maps "self" to the right seat regardless. Per the user, "seat mapping is not an issue because that is what you clicked" — i.e. clicking (834, 325) is correct.

After click: backend logs `payin_tx`, then `Join request stored on player identity`.

## Phase 6 — p2 auto

`tmux send-keys -t p2 'clear && cd /root/bet/poker && ./bin/bet start player -c config/p2.ini --auto' Enter`. No GUI interaction.

## Phase 7-8 — betting

Once the dealer initiates round betting and Controls render in the GUI, the typical buttons are Check / Fold / Raise / Call / All-In. Coordinates depend on Controls.tsx layout; capture a fresh viewport screenshot just before clicking.

## Backend status / identity tips

- Confirm backend READY before clicking Find Table: `tmux capture-pane -t p1 -pS -50 | grep -E "Backend status: READY|GUI server started"`.
- After every action that should advance state, `tmux capture-pane -t dealer -pS -200 | grep -E "INITIATING BETTING|HOLE CARDS DEALT|FLOP DEALT|TIMEOUT|SHOWDOWN"`.
