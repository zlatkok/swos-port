# CPU player control

`updateCpuPlayerControls()` is the original SWOS computer-player decision loop. It runs for each
computer-controlled team update and produces the same controls a human player would: an allowed movement
direction plus quick-fire, normal-fire, and per-frame fire signals. The rest of the game therefore does not
need separate implementations of passing, shooting, tackling, or ball aftertouch for CPU players.

## State-machine overview

Each update first advances the team's AI counter, clears the previous virtual controls, generates one random
value for all decisions in that update, and calculates the ball's distance and angle from the centre of the
opposing goal. It then follows one of two broad paths:

- During open play it chooses between shooting, changing the controlled player, turning toward the ball or
  goal, passing, and applying shot/pass aftertouch. Proximity flags and the closest player facing a requested
  direction influence those decisions.
- During a stoppage it acts only for `lastTeamPlayedBeforeBreak`. Separate branches handle kick-off and goal
  celebrations, goalkeeper restarts and goal kicks, throw-ins, corners, free kicks, fouls, and penalties.
  These branches steer toward a legal direction and eventually simulate quick or normal fire to resume play.

Two short shared timers prevent repeated fire commands and smooth direction changes. Shot strength and spin
are retained in the team state because the control decision and subsequent ball update happen at different
points in the player-update cycle.

The C++ implementation keeps the top-level routine as the dispatcher and separates policies that have clear
inputs and exit points. `calculateCpuGoalContext()` prepares the ball-to-goal geometry,
`clearCpuControls()` resets the virtual joypad, `handleCpuResultScreen()` advances unattended CPU-versus-CPU
matches, and the two restart-direction helpers perform legal-direction searches. The still-mechanical open
play and aftertouch sections remain inline until their register-based control flow has been converted.

`AI_turnDirection` deliberately remains in snapshot-managed SWOS memory. Recorded-game tests restore that
state when starting from an arbitrary captured frame; moving it to a private C++ static would make those
replays depend on whichever test happened to run previously.

## Restart timing investigation

The original routine calculates this changing deadline:

```text
100 + (currentGameTick & 63)
```

The value ranges from 100 through 163 ticks. While `stoppageTimerActive` is below it, the CPU follows a
direction-only preparation path. Once the timer reaches the deadline, the CPU enters the state-specific
restart logic that can press fire. The comparison was initially suspected of disabling restart attempts after
163 ticks, but the assembly operands show the opposite: reaching 163 guarantees that the active restart path
remains available. No stuck-restart bug has been established in this routine.
